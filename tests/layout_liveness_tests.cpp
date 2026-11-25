// Layout liveness (docs/user/layout.md, "Retained layout"): a control whose
// layout never settles, or settles expensively, cannot hang the window. A
// control that invalidates its own layout from its arrange is stopped and
// named; a flush that does not converge keeps the layout it reached; a
// pointer move reads the kept layout and lays nothing out.

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/scrollable_control.hpp"
#include "gui_forms/dirty.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/layout/diagnostics/layout_diagnostics.hpp"
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
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace gui_forms;

// Each test holds its window on the heap (see invalidation_damage_tests.cpp:
// windows on main's inlined stack add up in the sanitizer build).

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// How many times `window` recorded `code` against the control `id`.
std::size_t diagnostics_of(const Window& window, LayoutDiagnosticCode code, std::string_view id) {
    std::size_t count = 0U;
    const std::span<const LayoutDiagnostic> diagnostics = window.layout_diagnostics();
    for (const LayoutDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == code && diagnostic.child == id) ++count;
    }
    return count;
}

void move_pointer(Window& window, Point position) {
    PointerEvent event{};
    event.action = PointerAction::move;
    event.position = position;
    static_cast<void>(window.dispatch_pointer(event));
}

// Counts its layout callbacks; optionally asks for its own arrange again
// from every arrange, the pattern of a page that lays itself out and then
// requests another layout.
class SelfDirtyingControl final : public Control {
public:
    explicit SelfDirtyingControl(StableId id) : Control(std::move(id)) {}

    Size measure(Size available) override {
        ++measure_calls;
        return Control::measure(available);
    }
    void arrange(Rect bounds) override {
        ++arrange_calls;
        Control::arrange(bounds);
        if (dirty_from_arrange) invalidate(Dirty::arrange);
        // A height-for-width control asks once more after new bounds.
        if (dirty_on_new_bounds && bounds != last_bounds) {
            last_bounds = bounds;
            invalidate(Dirty::arrange);
        }
    }

    bool dirty_from_arrange{};
    bool dirty_on_new_bounds{};
    Rect last_bounds{};
    std::uint64_t measure_calls{};
    std::uint64_t arrange_calls{};
};

// Pocket Reference's ReferencePage before HelpBook (the hang of
// 2026-11-24): a scrolling page with no children that lays its content
// out in every arrange. ScrollableControl::arrange clamps the position to
// the children's extent (none), the page sets its own extent, and puts the
// position it had back, which asks for another arrange.
class RestoringScrollPage final : public ScrollableControl {
public:
    explicit RestoringScrollPage(StableId id) : ScrollableControl(std::move(id)) { auto_scroll = true; }

    void arrange(Rect bounds) override {
        ++arrange_calls;
        const Point kept = scroll_position();
        ScrollableControl::arrange(bounds);
        arrange_scroll_viewport(Size{bounds.width, bounds.height}, Size{bounds.width, content_height});
        static_cast<void>(scroll_to(kept));
    }

    double content_height{2000.0};
    std::uint64_t arrange_calls{};
};

// The same page done the retained way: it states the extent of what it
// paints (painted_content_extent), so the base arrange keeps the position,
// and it re-flows only when its width changes.
class ExtentStatingPage final : public ScrollableControl {
public:
    explicit ExtentStatingPage(StableId id) : ScrollableControl(std::move(id)) { auto_scroll = true; }

    void arrange(Rect bounds) override {
        ++arrange_calls;
        if (bounds.width != flowed_width) {
            ++flows;
            flowed_width = bounds.width;
        }
        ScrollableControl::arrange(bounds);
    }
    [[nodiscard]] bool paints_scrolled_content() const noexcept override { return true; }
    [[nodiscard]] Size painted_content_extent() const noexcept override { return Size{0.0, 2000.0}; }

    double flowed_width{-1.0};
    std::uint64_t flows{};
    std::uint64_t arrange_calls{};
};

// A scroll-bar oscillation: the content is tall at the full width and short
// at the width a vertical bar leaves, so the bar appearing removes the need
// for it. The page flows its content at the viewport width it last had and
// asks for another arrange when the viewport width changed.
class OscillatingPage final : public ScrollableControl {
public:
    explicit OscillatingPage(StableId id) : ScrollableControl(std::move(id)) { auto_scroll = true; }

    void arrange(Rect bounds) override {
        ++arrange_calls;
        ScrollableControl::arrange(bounds);
        const double width = viewport_rectangle().width;
        const double height = width >= bounds.width ? bounds.height * 3.0 : bounds.height * 0.5;
        arrange_scroll_viewport(Size{bounds.width, bounds.height}, Size{width, height});
        if (viewport_rectangle().width != width) invalidate(Dirty::arrange);
    }

    std::uint64_t arrange_calls{};
};

// Two controls that each invalidate the other from their arrange: no
// control repeats itself, so only the flush's pass limit ends it.
class MutualControl final : public Control {
public:
    explicit MutualControl(StableId id) : Control(std::move(id)) {}

    void arrange(Rect bounds) override {
        ++arrange_calls;
        Control::arrange(bounds);
        if (partner != nullptr) (*partner).invalidate(Dirty::arrange);
    }

    Control* partner{};
    std::uint64_t arrange_calls{};
};

// A large page, expensive to lay out, with a tooltip; moving over it sets
// the status control's width, as hovering a link sets a status bar's text.
class HoverPage final : public Control {
public:
    explicit HoverPage(StableId id) : Control(std::move(id)) {}

    Size measure(Size available) override {
        ++measure_calls;
        return Control::measure(available);
    }
    void arrange(Rect bounds) override {
        ++arrange_calls;
        Control::arrange(bounds);
        if (dirty_from_arrange) invalidate(Dirty::arrange);
    }
    void on_mouse_move(MouseEventArgs& arguments) override {
        ++moves;
        if (status != nullptr) {
            Rect requested = (*status).requested_bounds();
            requested.width = (moves % 2U) == 0U ? 120.0 : 160.0;
            (*status).set_requested_bounds(requested);
        }
        Control::on_mouse_move(arguments);
    }

    Control* status{};
    bool dirty_from_arrange{};
    std::uint64_t moves{};
    std::uint64_t measure_calls{};
    std::uint64_t arrange_calls{};
};

// A control that invalidates its own arrange from every arrange is arranged
// twice in a flush, named once as layout_loop, and left clean: the window
// lays out nothing more until something else invalidates it.
void test_self_dirtying_arrange_is_stopped() {
    std::shared_ptr<SelfDirtyingControl> root = make_control<SelfDirtyingControl>(StableId("liveness.self"));
    (*root).dirty_from_arrange = true;
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(root, Size{200.0, 100.0});
    Window& window = *held_window;
    window.reset_activity_metrics();
    window.perform_layout();
    require((*root).arrange_calls == 2U, "a self-invalidating arrange runs twice in one flush, then stops");
    require(!has_dirty((*root).dirty(), Dirty::layout), "the stopped control keeps no layout request");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_loop, "liveness.self") == 1U,
            "the control is named once as layout_loop");
    require(window.metrics_snapshot().bounded_pass_limit_hits == 0U, "the loop ends before the pass limit");

    window.perform_layout();
    static_cast<void>((*root).absolute_bounds());
    require((*root).arrange_calls == 2U, "a later flush or geometry read lays out nothing");

    (*root).invalidate(Dirty::arrange);
    window.perform_layout();
    require((*root).arrange_calls == 4U, "an outside invalidation arranges it again, bounded the same way");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_loop, "liveness.self") == 1U,
            "and the control is not named again");
    const LayoutDiagnosticSeverity severity = layout_diagnostic_severity(LayoutDiagnosticCode::layout_loop);
    require(severity == LayoutDiagnosticSeverity::error && layout_diagnostic_code_name(LayoutDiagnosticCode::layout_loop) ==
                                                                std::string_view("layout_loop"),
            "layout_loop is an error with its own name");
}

// A control arranged in new bounds after asking for its own arrange is not
// a loop: a height-for-width control may ask once more at its new size.
void test_self_dirtying_arrange_in_new_bounds_is_not_a_loop() {
    std::shared_ptr<SelfDirtyingControl> root = make_control<SelfDirtyingControl>(StableId("liveness.once"));
    (*root).dirty_on_new_bounds = true;
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(root, Size{200.0, 100.0});
    Window& window = *held_window;
    window.perform_layout();
    require((*root).arrange_calls == 2U, "new bounds arrange twice: the arrange and the one it asked for");
    (*root).arrange_calls = 0U;
    window.resize(Size{240.0, 100.0});
    window.perform_layout();
    require((*root).arrange_calls == 2U, "a resize arranges twice again");
    require(window.layout_diagnostics().empty(), "and nothing is recorded");
}

// The ReferencePage pattern: with the page scrolled, every arrange puts the
// position back and asks for another. The flush stops after the second
// arrange, the position and extent hold, the page is named, and pointer
// moves afterwards lay out nothing.
void test_scroll_restored_in_arrange_cannot_hang() {
    std::shared_ptr<RestoringScrollPage> page = make_control<RestoringScrollPage>(StableId("liveness.page"));
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(page, Size{400.0, 300.0});
    Window& window = *held_window;
    window.perform_layout();
    require((*page).arrange_calls == 1U, "unscrolled, the page arranges once");
    require((*page).vscroll(), "the page's extent shows a vertical bar");

    static_cast<void>((*page).scroll_to(Point{0.0, 600.0}));
    (*page).arrange_calls = 0U;
    window.reset_activity_metrics();
    window.perform_layout();
    require((*page).arrange_calls == 2U, "scrolled, the page arranges twice and is stopped");
    require((*page).scroll_position().y == 600.0, "the position the page put back holds");
    require(!has_dirty((*page).dirty(), Dirty::layout), "and no layout request is left");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_loop, "liveness.page") == 1U,
            "the page is named as layout_loop");
    require(diagnostics_of(window, LayoutDiagnosticCode::scroll_in_own_arrange, "liveness.page") == 1U,
            "and as moving its own position from its arrange");

    for (int index = 0; index != 200; ++index) {
        move_pointer(window, Point{20.0 + static_cast<double>(index % 300), 30.0 + static_cast<double>(index % 200)});
    }
    window.perform_layout();
    require((*page).arrange_calls == 2U, "pointer moves and later flushes do not arrange the page again");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_loop, "liveness.page") == 1U,
            "and the page is named once");
}

// A page that states its painted extent keeps its position through
// arranges with no restore, arranges once, records nothing, and pointer
// motion over it lays out nothing (measure_pointer_motion, the probe an
// application's own tests call).
void test_painted_extent_keeps_the_position() {
    std::shared_ptr<ExtentStatingPage> page = make_control<ExtentStatingPage>(StableId("liveness.extent"));
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(page, Size{400.0, 300.0});
    Window& window = *held_window;
    window.perform_layout();
    require((*page).vscroll(), "the stated extent shows a vertical bar");
    static_cast<void>((*page).scroll_to(Point{0.0, 850.0}));
    (*page).arrange_calls = 0U;
    window.perform_layout();
    require((*page).arrange_calls == 1U, "the scroll arranges once");
    require((*page).scroll_position().y == 850.0, "the base arrange keeps the position");
    window.resize(Size{420.0, 300.0});
    window.perform_layout();
    require((*page).scroll_position().y == 850.0 && (*page).flows == 2U, "a resize re-flows once and keeps it");
    require(window.layout_diagnostics().empty(), "nothing is recorded");

    const PointerMotionCost cost = measure_pointer_motion(window, Rect{0.0, 0.0, 380.0, 300.0});
    require(cost.moves == 100U && cost.lays_out_nothing(), "100 pointer moves over the page lay out nothing");
    require((*page).flows == 2U, "and re-flow nothing");
}

// The probe catches the ReferencePage pattern's cost in the old toolkit's
// terms: after the loop is stopped, pointer motion lays out nothing there
// either.
void test_probe_after_a_stopped_loop() {
    std::shared_ptr<RestoringScrollPage> page = make_control<RestoringScrollPage>(StableId("liveness.probe"));
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(page, Size{400.0, 300.0});
    Window& window = *held_window;
    window.perform_layout();
    static_cast<void>((*page).scroll_to(Point{0.0, 900.0}));
    const PointerMotionCost cost = measure_pointer_motion(window, Rect{0.0, 0.0, 380.0, 300.0});
    require(cost.lays_out_nothing(), "the stopped page is not laid out again by pointer motion");
    bool refused = false;
    try {
        static_cast<void>(measure_pointer_motion(window, Rect{0.0, 0.0, 0.0, 10.0}));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "an empty area is refused");
}

// The scroll-bar oscillation converges: two arranges, the page named, and
// a bar state the window keeps.
void test_scroll_bar_oscillation_converges() {
    std::shared_ptr<OscillatingPage> page = make_control<OscillatingPage>(StableId("liveness.oscillating"));
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(page, Size{400.0, 300.0});
    Window& window = *held_window;
    window.reset_activity_metrics();
    window.perform_layout();
    require((*page).arrange_calls == 2U, "the oscillating page is arranged twice");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_loop, "liveness.oscillating") == 1U,
            "and named as layout_loop");
    const bool bar = (*page).vscroll();
    window.perform_layout();
    move_pointer(window, Point{100.0, 100.0});
    window.perform_layout();
    require((*page).arrange_calls == 2U && (*page).vscroll() == bar, "the bar state is kept, not recomputed");
}

// Controls that keep invalidating each other end at the pass limit: each
// still dirty is named once as layout_not_converged and its request is
// dropped, so later flushes do nothing.
void test_unconverged_flush_keeps_its_layout() {
    std::shared_ptr<Control> root = make_control<Control>(StableId("liveness.mutual.root"));
    std::shared_ptr<MutualControl> first = make_control<MutualControl>(StableId("liveness.mutual.first"));
    std::shared_ptr<MutualControl> second = make_control<MutualControl>(StableId("liveness.mutual.second"));
    (*first).set_requested_bounds({0.0, 0.0, 50.0, 40.0});
    (*second).set_requested_bounds({60.0, 0.0, 50.0, 40.0});
    (*first).partner = second.get();
    (*second).partner = first.get();
    (*root).add_child(first);
    (*root).add_child(second);
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(root, Size{200.0, 100.0});
    Window& window = *held_window;
    window.reset_activity_metrics();
    window.perform_layout();
    const MetricsSnapshot metrics = window.metrics_snapshot();
    require(metrics.bounded_pass_limit_hits == 1U, "the mutual invalidation reaches the pass limit once");
    require((*first).arrange_calls == 4U && (*second).arrange_calls == 4U, "each is arranged once per pass");
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_not_converged, "liveness.mutual.first") == 1U,
            "the control still dirty is named as layout_not_converged");
    require(!has_dirty((*first).dirty(), Dirty::layout) && !has_dirty((*second).dirty(), Dirty::layout) &&
                !has_dirty((*root).subtree_dirty(), Dirty::layout),
            "and the requests are dropped");

    window.perform_layout();
    static_cast<void>((*first).absolute_bounds());
    require((*first).arrange_calls == 4U && (*second).arrange_calls == 4U &&
                window.metrics_snapshot().bounded_pass_limit_hits == 1U,
            "later flushes and geometry reads lay out nothing");
}

// Pointer moves over a large page lay it out only when its layout was
// invalidated: a hover handler that changes another control's layout
// arranges that control, not the page, and the tooltip's subject is found
// from the kept layout.
void test_pointer_moves_do_not_lay_out_the_page() {
    std::shared_ptr<Control> root = make_control<Control>(StableId("liveness.hover.root"));
    std::shared_ptr<HoverPage> page = make_control<HoverPage>(StableId("liveness.hover.page"));
    std::shared_ptr<SelfDirtyingControl> status = make_control<SelfDirtyingControl>(StableId("liveness.hover.status"));
    (*page).set_requested_bounds({0.0, 0.0, 600.0, 400.0});
    (*page).tool_tip_text = "A large page";
    (*status).set_requested_bounds({0.0, 410.0, 120.0, 20.0});
    (*page).status = status.get();
    (*root).add_child(page);
    (*root).add_child(status);
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(root, Size{640.0, 440.0});
    Window& window = *held_window;
    window.perform_layout();
    (*page).arrange_calls = 0U;
    (*page).measure_calls = 0U;
    (*status).arrange_calls = 0U;
    for (int index = 0; index != 100; ++index) {
        move_pointer(window, Point{10.0 + static_cast<double>(index * 5), 10.0 + static_cast<double>(index * 3)});
    }
    window.perform_layout();
    require((*page).moves == 100U, "the page saw every move");
    require((*page).arrange_calls == 0U && (*page).measure_calls == 0U, "pointer moves never lay out the page");
    require((*status).arrange_calls > 0U, "the status control the hover changed is laid out");

    // The same page invalidating itself from its arrange (the 2026-11-24
    // hang): after the flush that stops it, moves lay it out no more.
    (*page).dirty_from_arrange = true;
    (*page).invalidate(Dirty::arrange);
    window.perform_layout();
    const std::uint64_t stopped = (*page).arrange_calls;
    require(stopped == 2U, "the self-invalidating page is stopped after two arranges");
    for (int index = 0; index != 100; ++index) {
        move_pointer(window, Point{12.0 + static_cast<double>(index * 5), 12.0 + static_cast<double>(index * 3)});
    }
    window.perform_layout();
    require((*page).arrange_calls == stopped, "pointer moves do not arrange the stopped page again");
}

// Layout invalidations from measure and from paint are named, once.
class MeasureDirtyingControl final : public Control {
public:
    explicit MeasureDirtyingControl(StableId id) : Control(std::move(id)) {}

    Size measure(Size available) override {
        invalidate(Dirty::arrange);
        return Control::measure(available);
    }
};

void test_measure_invalidation_is_named() {
    std::shared_ptr<Control> root = make_control<Control>(StableId("liveness.measure.root"));
    std::shared_ptr<MeasureDirtyingControl> child =
        make_control<MeasureDirtyingControl>(StableId("liveness.measure.child"));
    (*child).set_requested_bounds({0.0, 0.0, 50.0, 40.0});
    (*root).add_child(child);
    const std::unique_ptr<Window> held_window = std::make_unique<Window>(root, Size{200.0, 100.0});
    Window& window = *held_window;
    window.perform_layout();
    (*child).invalidate(Dirty::measure);
    window.perform_layout();
    require(diagnostics_of(window, LayoutDiagnosticCode::layout_invalidated_in_measure, "liveness.measure.child") == 1U,
            "a control invalidating its own layout from measure is named once");
}

} // namespace

int main() {
    try {
        test_self_dirtying_arrange_is_stopped();
        test_self_dirtying_arrange_in_new_bounds_is_not_a_loop();
        test_scroll_restored_in_arrange_cannot_hang();
        test_painted_extent_keeps_the_position();
        test_probe_after_a_stopped_loop();
        test_scroll_bar_oscillation_converges();
        test_unconverged_flush_keeps_its_layout();
        test_pointer_moves_do_not_lay_out_the_page();
        test_measure_invalidation_is_named();
        std::cout << "gui_forms_layout_liveness_tests: all tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_layout_liveness_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
