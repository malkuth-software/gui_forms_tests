// HelpBook lays its page out only when what the layout is made from changes
// (the page, the wrap width, the font, the text metrics, the scale), keeps
// its scroll position when it is arranged again, and reads the pointer's
// local location (2026-11-24, from Pocket Reference's hang).

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/help_book.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/help/help_page/help_page.hpp"
#include "gui_forms/metrics/types/metrics_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <stxlib/optional.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

struct Fixture final {
    std::shared_ptr<Panel> root;
    std::shared_ptr<HelpBook> book;
    std::unique_ptr<Window> window;
};

// Appends a run to `page` and returns its index.
std::size_t add_run(HelpPageModel& page, std::string text, bool code, stxlib::Optional<std::size_t> link) {
    HelpPageRun run{};
    run.text = std::move(text);
    run.code = code;
    run.link = link;
    page.runs.push_back(std::move(run));
    return page.runs.size() - 1U;
}

std::size_t add_link(HelpPageModel& page, std::string target) {
    page.links.push_back(HelpPageLink{HelpLinkKind::topic, std::move(target), {}, {}});
    return page.links.size() - 1U;
}

// A block of `kind` over the runs from `first_run` to the end of the runs.
void add_block(HelpPageModel& page, HelpPageBlockKind kind, std::size_t first_run) {
    HelpPageBlock block{};
    block.kind = kind;
    block.first_run = first_run;
    block.run_count = page.runs.size() - first_run;
    page.blocks.push_back(std::move(block));
}

struct RecordLink final {
    std::vector<std::string>* seen{};
    void operator()(HelpLinkActivation& activation) const {
        (*seen).push_back(activation.target);
        activation.handled = true;
    }
};

// A long page, scrolled to its middle, in a book away from the window's
// origin: re-arranging keeps the scroll position and lays nothing out, and
// 100 pointer moves over it arrange, measure and flush nothing. The moves
// hit the link at its place in the book, not at the same point of the
// window, so the book reads the pointer's local location.
void scrolled_pointer_motion_costs_nothing() {
    Fixture fixture;
    fixture.root = make_control<Panel>(StableId("app"));
    fixture.book = make_control<HelpBook>(StableId("app.page"));
    (*fixture.book).set_requested_bounds({60.0, 80.0, 600.0, 400.0});
    (*fixture.root).add_child(fixture.book);
    fixture.window = std::make_unique<Window>(fixture.root, Size{700.0, 520.0});
    (*fixture.window).perform_layout();
    HelpPageModel page{};
    page.title = "Geometry and appearance";
    std::size_t middle_link = 0U;
    for (std::size_t index = 0U; index < 200U; ++index) {
        const std::size_t link = add_link(page, "type/Item" + std::to_string(index));
        if (index == 100U) middle_link = link;
        const std::size_t first = page.runs.size();
        add_run(page, "Paragraph " + std::to_string(index) + " has ", false, {});
        add_run(page, "a link", false, stxlib::Optional<std::size_t>::of(link));
        add_run(page, ", some ", false, {});
        add_run(page, "inline_code", true, {});
        add_run(page, " and enough words after them to wrap onto a second line in the book.", false, {});
        add_block(page, HelpPageBlockKind::paragraph, first);
    }
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const stxlib::Optional<double> top = (*fixture.book).block_top(100U);
    require(top.present() && (*top.value()) > 1000.0, "the page is long");
    static_cast<void>((*fixture.book).scroll_to(Point{0.0, (*top.value()) - 40.0}));
    (*fixture.window).perform_layout();
    const Point scrolled = (*fixture.book).scroll_position();
    require(scrolled.y > 1000.0, "the book scrolls to the middle of the page");
    const std::uint64_t passes = (*fixture.book).layout_passes();

    // A taller book re-arranges at the same width: the position stays and
    // the page is not laid out again.
    (*fixture.book).set_requested_bounds({60.0, 80.0, 600.0, 420.0});
    (*fixture.window).perform_layout();
    require((*fixture.book).scroll_position() == scrolled, "re-arranging keeps the scroll position");
    require((*fixture.book).layout_passes() == passes, "re-arranging at the same width lays nothing out");

    const stxlib::Optional<Rect> found = (*fixture.book).link_bounds(middle_link);
    require(found.present(), "the middle link is laid out");
    const Rect bounds = (*found.value());
    const Rect book_bounds = (*fixture.book).absolute_bounds();
    require(bounds.y > 0.0 && bounds.y < 380.0, "the middle link shows in the scrolled viewport");
    (*fixture.window).reset_activity_metrics();
    const MetricsSnapshot before = (*fixture.window).metrics_snapshot();
    PointerEvent move{};
    move.action = PointerAction::move;
    for (std::size_t step = 0U; step < 100U; ++step) {
        const double x = book_bounds.x + bounds.x + 2.0 + static_cast<double>(step % 7U);
        const double y = step % 2U == 0U ? book_bounds.y + bounds.y + bounds.height * 0.5
                                         : book_bounds.y + bounds.y + 60.0 + static_cast<double>(step);
        move.position = Point{x, y};
        static_cast<void>((*fixture.window).dispatch_pointer(move));
        (*fixture.window).perform_layout();
    }
    const MetricsSnapshot after = (*fixture.window).metrics_snapshot();
    require(after.controls_arranged == before.controls_arranged, "pointer motion arranges no control");
    require(after.measure_passes == before.measure_passes, "pointer motion measures nothing");
    require(after.read_barrier_flushes == before.read_barrier_flushes, "pointer motion flushes no layout");
    require((*fixture.book).layout_passes() == passes, "pointer motion lays nothing out");
    require((*fixture.book).scroll_position() == scrolled, "pointer motion keeps the scroll position");
    std::vector<std::string> followed;
    SubscriptionToken token = (*fixture.book).link_activated().subscribe(RecordLink{&followed});
    PointerEvent press{};
    press.button = PointerButton::primary;
    press.position = Point{book_bounds.x + bounds.x + 2.0, book_bounds.y + bounds.y + bounds.height * 0.5};
    press.action = PointerAction::down;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    press.action = PointerAction::up;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    require(followed.size() == 1U && followed[0] == "type/Item100", "the pointer hits the link at its place in the book");
}

} // namespace

int main() {
    scrolled_pointer_motion_costs_nothing();
    std::cout << "help book reference tests passed\n";
    return 0;
}
