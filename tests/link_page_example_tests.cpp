// The custom-control example (gui_forms examples/link_page,
// docs/user/custom-controls.md), driven headless: a scrolling page of 2,000
// wrapped paragraphs with links, placed beside a 160-pixel side panel so
// control and window coordinates differ.
//
// - Pointer motion: scrolled to the middle, 100 pointer moves over the page
//   lay out nothing (measure_pointer_motion) and flow no text.
// - Hover and click: the pointer over a link hovers it and repaints, and a
//   click on it reports it, hit-tested in control coordinates.
// - Scroll: wheel and scroll_to move the page without flowing text, and the
//   position holds across arranges.
// - Resize: a new width flows the text (once, or twice when the bar comes
//   or goes) and keeps the position within range; a second layout at the
//   same size flows nothing; the height alone flows nothing.
// - No layout diagnostic is recorded at any point.

#include "link_page.hpp"
#include "sample_document.hpp"

#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/layout/diagnostics/pointer_motion_probe.hpp"
#include "gui_forms/metrics/types/metrics_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace gui_forms;
using link_page_example::LinkActivation;
using link_page_example::LinkPage;

constexpr double side_width = 160.0;
constexpr double window_width = 760.0;
constexpr double window_height = 480.0;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Counts activations and keeps the last one's link and target.
struct ActivationLog final {
    void activated(LinkActivation& activation) {
        ++count;
        link = activation.link;
        target = std::string(activation.target);
    }
    std::size_t count{};
    std::size_t link{};
    std::string target;
};

// A side panel and the page, side by side, in a window on the heap (see
// invalidation_damage_tests.cpp: windows on main's stack add up in the
// sanitizer build).
struct Scene final {
    Scene() {
        root = make_control<Panel>(StableId("link-page.root"));
        side = make_control<Panel>(StableId("link-page.side"));
        (*side).set_requested_bounds(Rect{0.0, 0.0, side_width, window_height});
        page = make_control<LinkPage>(StableId("link-page.page"));
        (*page).set_requested_bounds(Rect{side_width, 0.0, window_width - side_width, window_height});
        (*page).set_paragraphs(link_page_example::sample_document(2000U));
        (*root).add_child(side);
        (*root).add_child(page);
        window = std::make_unique<Window>(root, Size{window_width, window_height});
        (*window).perform_layout();
        token = (*page).link_activated().subscribe(
            Delegate<LinkActivation&>::bind<ActivationLog, &ActivationLog::activated>(log));
    }

    // The page's area in window coordinates.
    [[nodiscard]] Rect page_area() const {
        const Rect requested = (*page).requested_bounds();
        return requested;
    }

    void pointer(PointerAction action, Point position, PointerButton button = PointerButton::none) {
        PointerEvent event{};
        event.action = action;
        event.position = position;
        event.button = button;
        event.pointer_id = 1;
        static_cast<void>((*window).dispatch_pointer(event));
    }

    void wheel(double pixels, Point position) {
        PointerEvent event{};
        event.action = PointerAction::wheel;
        event.position = position;
        event.wheel_delta = Point{0.0, pixels};
        event.wheel_unit = WheelDeltaUnit::pixels;
        event.pointer_id = 1;
        static_cast<void>((*window).dispatch_pointer(event));
    }

    // Scrolls to the middle of the content and lays out.
    void scroll_to_middle() {
        const double middle = (*page).content_height() * 0.5;
        static_cast<void>((*page).scroll_to(Point{0.0, middle}));
        (*window).perform_layout();
    }

    ActivationLog log{};
    std::shared_ptr<Panel> root;
    std::shared_ptr<Panel> side;
    std::shared_ptr<LinkPage> page;
    std::unique_ptr<Window> window;
    SubscriptionToken token{};
};

void test_lays_out_once() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    require(page.paragraph_count() == 2000U && page.link_count() > 4000U, "the sample document is loaded");
    require(page.flow_count() >= 1U && page.flow_count() <= 2U, "the first layout flows the text once or twice");
    require(page.content_height() > window_height * 10.0, "the content is many viewports tall");
    require(page.vscroll(), "the stated extent shows a vertical bar");
    const std::uint64_t flows = page.flow_count();
    page.invalidate(Dirty::arrange);
    (*(*scene).window).perform_layout();
    require(page.flow_count() == flows, "an arrange at the same width flows nothing");
    require((*(*scene).window).layout_diagnostics().empty(), "the first layout records nothing");
}

void test_pointer_motion_lays_out_nothing() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    (*scene).scroll_to_middle();
    const double position = page.scroll_position().y;
    require(position > 0.0, "the page scrolled to the middle");
    const std::uint64_t flows = page.flow_count();
    const PointerMotionCost cost = measure_pointer_motion(*(*scene).window, (*scene).page_area(), 100U);
    require(cost.moves == 100U, "100 pointer moves were made");
    require(cost.lays_out_nothing(), "100 pointer moves over the scrolled page lay out nothing");
    require(page.flow_count() == flows, "and flow no text");
    require(page.scroll_position().y == position, "and leave the position where it was");
    require((*(*scene).window).layout_diagnostics().empty(), "pointer motion records nothing");
}

void test_hover_and_click_in_control_coordinates() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    (*scene).scroll_to_middle();
    // The first link whose first piece is fully in the viewport.
    const Rect viewport = page.viewport_rectangle();
    std::size_t chosen = page.link_count();
    for (std::size_t link = 0U; link < page.link_count(); ++link) {
        const stxlib::Optional<Rect> bounds = page.link_bounds(link);
        const Rect* rect = bounds.value();
        if (rect != nullptr && viewport.contains(*rect)) {
            chosen = link;
            break;
        }
    }
    require(chosen < page.link_count(), "a link is visible in the middle of the page");
    const stxlib::Optional<Rect> chosen_bounds = page.link_bounds(chosen);
    const Rect local = *chosen_bounds.value();
    // Window coordinates are control coordinates shifted by the side panel.
    const Point inside{side_width + local.x + local.width * 0.5, local.y + local.height * 0.5};
    const std::uint64_t flows = page.flow_count();
    const MetricsSnapshot before = (*(*scene).window).metrics_snapshot();
    (*scene).pointer(PointerAction::move, inside);
    require(page.hovered_link() == stxlib::Optional<std::size_t>::of(chosen), "the pointer hovers the link under it");
    (*(*scene).window).perform_layout();
    const MetricsSnapshot after = (*(*scene).window).metrics_snapshot();
    require(after.controls_arranged == before.controls_arranged, "hovering arranges nothing");
    // The same local point without the side panel's shift is a different
    // place on the page: it must not be read as the link.
    (*scene).pointer(PointerAction::move, Point{local.x + local.width * 0.5, local.y + local.height * 0.5});
    require(page.hovered_link() != stxlib::Optional<std::size_t>::of(chosen),
            "a window point is not taken for a control point");
    (*scene).pointer(PointerAction::move, inside);
    (*scene).pointer(PointerAction::down, inside, PointerButton::primary);
    (*scene).pointer(PointerAction::up, inside, PointerButton::primary);
    require((*scene).log.count == 1U && (*scene).log.link == chosen, "a click on the link reports it");
    require(!(*scene).log.target.empty(), "with its target");
    require(page.flow_count() == flows, "hover and click flow no text");
    require((*(*scene).window).layout_diagnostics().empty(), "hover and click record nothing");
}

void test_scrolling_flows_nothing() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    const std::uint64_t flows = page.flow_count();
    const Rect area = (*scene).page_area();
    const Point over{area.x + area.width * 0.5, area.y + area.height * 0.5};
    for (int turn = 0; turn < 20; ++turn) {
        (*scene).wheel(-120.0, over);
        (*(*scene).window).perform_layout();
    }
    const double wheeled = page.scroll_target().y;
    require(wheeled > 0.0, "the wheel scrolls the page");
    (*scene).scroll_to_middle();
    const double middle = page.scroll_position().y;
    page.invalidate(Dirty::arrange);
    (*(*scene).window).perform_layout();
    require(page.scroll_position().y == middle, "the position holds across an arrange");
    require(page.flow_count() == flows, "scrolling flows no text");
    require((*(*scene).window).layout_diagnostics().empty(), "scrolling records nothing");
}

void test_resize_flows_once_and_keeps_the_position() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    Window& window = *(*scene).window;
    (*scene).scroll_to_middle();
    const std::uint64_t flows = page.flow_count();
    const double tall = page.content_height();
    // Narrower: more lines, the same position stays in range.
    page.set_requested_bounds(Rect{side_width, 0.0, (window_width - side_width) * 0.6, window_height});
    window.perform_layout();
    const std::uint64_t narrowed = page.flow_count();
    require(narrowed > flows && narrowed <= flows + 2U, "a new width flows the text once or twice");
    require(page.content_height() > tall, "a narrower page is taller");
    require(page.scroll_position().y > 0.0, "the position is kept");
    window.perform_layout();
    require(page.flow_count() == narrowed, "a second layout at the same width flows nothing");
    // Taller only: the width is unchanged, so nothing flows.
    page.set_requested_bounds(Rect{side_width, 0.0, (window_width - side_width) * 0.6, window_height - 80.0});
    window.perform_layout();
    require(page.flow_count() == narrowed, "a new height alone flows nothing");
    // The window itself resized.
    window.resize(Size{window_width + 120.0, window_height});
    window.perform_layout();
    require(page.flow_count() <= narrowed + 2U, "a window resize flows at most twice");
    const PointerMotionCost cost = measure_pointer_motion(window, (*scene).page_area(), 100U);
    require(cost.lays_out_nothing(), "after resizing, pointer motion still lays out nothing");
    require(window.layout_diagnostics().empty(), "resizing records nothing");
}

void test_refuses_bad_links() {
    const std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    LinkPage& page = *(*scene).page;
    const std::size_t links = page.link_count();
    std::vector<link_page_example::Paragraph> bad(1U);
    bad[0].text = "short";
    link_page_example::LinkSpan outside{};
    outside.begin = 2U;
    outside.end = 40U;
    bad[0].links.push_back(outside);
    bool refused = false;
    try {
        page.set_paragraphs(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a link outside its text is refused");
    require(page.link_count() == links && page.paragraph_count() == 2000U, "and nothing changed");
}

} // namespace

int main() {
    try {
        test_lays_out_once();
        test_pointer_motion_lays_out_nothing();
        test_hover_and_click_in_control_coordinates();
        test_scrolling_flows_nothing();
        test_resize_flows_once_and_keeps_the_position();
        test_refuses_bad_links();
    } catch (const std::exception& error) {
        std::cerr << "link_page_example_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "link_page_example_tests: passed\n";
    return EXIT_SUCCESS;
}
