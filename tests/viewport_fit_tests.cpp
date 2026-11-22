// Stage VA, phase VA1: the visible-area guarantee (owner decisions of
// 2026-11-22; notes planning/stages/VA-visible-area/STUDY.md). The
// placement computation (contain, cover, the scale bounds, alignment); the
// viewport_fit and design_size properties and their refusals; the window's
// overflow records, content_overflow diagnostics and counters for children
// placed outside a panel, answered by scrolling, by an explicit clip, or not
// at all; the root against its slot; design sizes, answered by a scaled
// panel's stretch and unanswered inside a window before the in-tree scale
// frame; safe insets and the root slot; and the hosts' window-size clamp on
// the headless reference host, which leaves a full-screen frame alone.

#include "gui_forms/application.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/container_controls.hpp"
#include "gui_forms/host.hpp"
#include "gui_forms/layout/viewport_fit/viewport_fit.hpp"
#include "gui_forms/window.hpp"
#include "application/headless_application.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <stxlib/view.hpp>

namespace {

using namespace gui_forms;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double left, double right) {
    return std::abs(left - right) <= 1.0e-9;
}

bool near_rect(Rect left, Rect right) {
    const bool same = near(left.x, right.x) && near(left.y, right.y) && near(left.width, right.width) &&
                      near(left.height, right.height);
    return same;
}

std::string describe(Rect rect) {
    return "{" + std::to_string(rect.x) + ", " + std::to_string(rect.y) + ", " + std::to_string(rect.width) + ", " +
           std::to_string(rect.height) + "}";
}

// Whether `call` throws std::invalid_argument.
template <typename Call>
bool refuses(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct WriteViewportFit final {
    Control* control{};
    ViewportFit fit{};
    void operator()() const { (*control).viewport_fit = fit; }
};

struct WriteDesignSize final {
    Control* control{};
    Size size{};
    void operator()() const { (*control).design_size = size; }
};

struct WriteSafeInsets final {
    Window* window{};
    SafeInsets insets{};
    void operator()() const { (*window).set_safe_insets(insets); }
};

// The number of content_overflow diagnostics the window holds.
std::size_t overflow_diagnostics(const Window& window) {
    std::size_t count = 0U;
    for (const LayoutDiagnostic& diagnostic : window.layout_diagnostics()) {
        if (diagnostic.code == LayoutDiagnosticCode::content_overflow) ++count;
    }
    return count;
}

// The record of `owner`, or none.
std::optional<OverflowRecord> record_of(const Window& window, const Control& owner, OverflowCause cause) {
    const stxlib::View<OverflowRecord> records = window.overflow_records();
    for (stxlib::usize index = 0U; index < records.count(); ++index) {
        const OverflowRecord& record = records[index];
        if (record.owner == owner.runtime_id() && record.cause == cause) return record;
    }
    return std::nullopt;
}

// -- the placement ------------------------------------------------------------------------

void test_contain_and_cover_placement() {
    const Rect viewport{0.0, 0.0, 400.0, 400.0};
    const ViewportFit contain{};
    const ViewportPlacement contained = place_viewport_content(Size{800.0, 600.0}, viewport, contain);
    require(contained.applied == OverflowAnswer::contain && near(contained.scale, 0.5) && contained.fits,
            "contain takes the smaller ratio");
    require(near_rect(contained.destination, Rect{0.0, 50.0, 400.0, 300.0}),
            "contain centres in the leftover: " + describe(contained.destination));

    ViewportFit cover{};
    cover.answer = OverflowAnswer::cover;
    cover.maximum_scale = 4.0;
    const ViewportPlacement covered = place_viewport_content(Size{800.0, 600.0}, viewport, cover);
    require(covered.applied == OverflowAnswer::cover && near(covered.scale, 400.0 / 600.0) && !covered.fits,
            "cover takes the larger ratio and crops");
    require(near(covered.destination.height, 400.0) && near(covered.destination.x, (400.0 - 800.0 * 400.0 / 600.0) * 0.5),
            "cover centres the excess: " + describe(covered.destination));

    ViewportFit floored{};
    floored.minimum_scale = 0.75;
    const ViewportPlacement below = place_viewport_content(Size{800.0, 600.0}, viewport, floored);
    require(below.applied == OverflowAnswer::scroll && near(below.scale, 0.75) && !below.fits,
            "below the minimum the fallback answers at the minimum scale");
    require(near(below.destination.x, 0.0) && near(below.destination.y, 0.0),
            "the fallback places from the top left");

    const ViewportPlacement small = place_viewport_content(Size{100.0, 100.0}, viewport, contain);
    require(near(small.scale, 1.0) && near_rect(small.destination, Rect{150.0, 150.0, 100.0, 100.0}),
            "contain grows no further than the maximum scale");
    ViewportFit grow{};
    grow.maximum_scale = 4.0;
    grow.alignment = ContentAlignment::bottom_right;
    const ViewportPlacement grown = place_viewport_content(Size{100.0, 50.0}, viewport, grow);
    require(near(grown.scale, 4.0) && near_rect(grown.destination, Rect{0.0, 200.0, 400.0, 200.0}),
            "a larger maximum grows the content and aligns it: " + describe(grown.destination));

    const ViewportPlacement empty = place_viewport_content(Size{0.0, 10.0}, viewport, contain);
    require(near(empty.scale, 1.0) && empty.fits, "empty content is placed at scale 1");
    ViewportFit invalid{};
    invalid.maximum_scale = -1.0;
    require(place_viewport_content(Size{800.0, 600.0}, viewport, invalid) == contained,
            "an invalid fit is placed as the default");
}

void test_values_and_names() {
    require(valid_viewport_fit(ViewportFit{}), "the default fit is valid");
    ViewportFit fit{};
    fit.below_minimum = OverflowAnswer::contain;
    require(!valid_viewport_fit(fit), "below_minimum must be scroll or clip");
    fit = ViewportFit{};
    fit.minimum_scale = 2.0;
    require(!valid_viewport_fit(fit), "the maximum may not be below the minimum");
    fit = ViewportFit{};
    fit.maximum_scale = maximum_viewport_scale * 2.0;
    require(!valid_viewport_fit(fit), "the maximum is bounded");
    fit = ViewportFit{};
    fit.minimum_scale = std::nan("");
    require(!valid_viewport_fit(fit), "a scale must be finite");
    require(std::string(overflow_answer_name(OverflowAnswer::cover)) == "cover", "answer names");
    require(std::string(overflow_cause_name(OverflowCause::design_size_exceeds_viewport)) ==
                "design_size_exceeds_viewport",
            "cause names");
    require(std::string(layout_diagnostic_code_name(LayoutDiagnosticCode::content_overflow)) == "content_overflow",
            "diagnostic name");
    require(valid_safe_insets(SafeInsets{}) && !valid_safe_insets(SafeInsets{-1.0, 0.0, 0.0, 0.0}),
            "safe insets are finite and not negative");
    const Rect work{0.0, 0.0, 1000.0, 700.0};
    require(fit_to_work_area({300.0, 300.0, 1200.0, 900.0}, work) == Rect{0.0, 0.0, 1000.0, 700.0},
            "an oversized rectangle takes the work area");
    require(fit_to_work_area({900.0, 100.0, 200.0, 100.0}, work) == Rect{800.0, 100.0, 200.0, 100.0},
            "a fitting rectangle only moves");
}

void test_properties() {
    const std::shared_ptr<Panel> panel = make_control<Panel>(StableId("va.properties"));
    require((*panel).viewport_fit.read() == ViewportFit{}, "the default viewport fit");
    require((*panel).design_size.read() == Size{}, "the default design size is none");
    ViewportFit bad{};
    bad.maximum_scale = 0.0;
    require(refuses(WriteViewportFit{panel.get(), bad}), "an invalid fit is refused");
    require((*panel).viewport_fit.read() == ViewportFit{}, "a refused fit changes nothing");
    ViewportFit clip{};
    clip.answer = OverflowAnswer::clip;
    (*panel).viewport_fit = clip;
    require((*panel).viewport_fit.read().answer == OverflowAnswer::clip, "a fit is stored");
    require(refuses(WriteDesignSize{panel.get(), Size{100.0, 0.0}}), "one zero side is refused");
    require(refuses(WriteDesignSize{panel.get(), Size{-1.0, -1.0}}), "a negative design size is refused");
    (*panel).design_size = Size{640.0, 480.0};
    require((*panel).design_size.read() == Size{640.0, 480.0}, "a design size is stored");
    require(!(*panel).content_scrolls(), "a panel without auto scroll does not scroll its content");
    (*panel).auto_scroll = true;
    require((*panel).content_scrolls(), "auto scroll scrolls the content");
}

// -- the window's records ------------------------------------------------------------------

void test_placed_child_overflow_and_its_answers() {
    const std::shared_ptr<Panel> viewer = make_control<Panel>(StableId("va.viewer"));
    const std::shared_ptr<Label> page = make_control<Label>(StableId("va.viewer.page"), "A long page");
    (*page).set_requested_bounds({0.0, 0.0, 600.0, 900.0});
    (*viewer).add_child(page);
    Window window(viewer, {300.0, 200.0});
    window.perform_layout();

    std::optional<OverflowRecord> record =
        record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value(), "a child placed outside the panel is recorded");
    require((*record).answer == OverflowAnswer::contain && !(*record).answered,
            "the default answer, contain, is not carried out inside a window yet");
    require(near((*record).content_extent.width, 600.0) && near((*record).content_extent.height, 900.0) &&
                near((*record).viewport_extent.width, 300.0) && near((*record).viewport_extent.height, 200.0),
            "the record holds both extents");
    require(window.overflow_owner(*record) == viewer, "the record names its owner");
    require(window.unanswered_overflow_count() == 1U, "one unanswered overflow");
    require(overflow_diagnostics(window) == 1U, "the overflow is diagnosed once");
    const LayoutDiagnostic& diagnostic = window.layout_diagnostics().back();
    require(diagnostic.severity == LayoutDiagnosticSeverity::error && diagnostic.child == "va.viewer",
            "an unanswered overflow is an error naming its owner");
    MetricsSnapshot metrics = window.metrics_snapshot();
    require(metrics.overflow_contained == 1U && metrics.overflow_unanswered == 1U, "the overflow is counted");

    // The same overflow again changes nothing.
    (*viewer).invalidate(Dirty::layout);
    window.perform_layout();
    require(overflow_diagnostics(window) == 1U, "an unchanged overflow is not diagnosed again");

    // Auto scroll answers it.
    (*viewer).auto_scroll = true;
    window.perform_layout();
    record = record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value() && (*record).answer == OverflowAnswer::scroll && (*record).answered,
            "a scrolling panel answers by scrolling");
    require(window.unanswered_overflow_count() == 0U, "nothing is unanswered");
    require(overflow_diagnostics(window) == 2U && window.layout_diagnostics().back().severity ==
                                                       LayoutDiagnosticSeverity::warning,
            "the changed answer is diagnosed, as a warning");
    metrics = window.metrics_snapshot();
    require(metrics.overflow_scrolled == 1U, "the scroll is counted");

    // An explicit clip answers it too.
    (*viewer).auto_scroll = false;
    ViewportFit clip{};
    clip.answer = OverflowAnswer::clip;
    (*viewer).viewport_fit = clip;
    window.perform_layout();
    record = record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value() && (*record).answer == OverflowAnswer::clip && (*record).answered,
            "an explicit clip answers");

    // Content that fits leaves no record.
    (*page).set_requested_bounds({0.0, 0.0, 100.0, 50.0});
    window.perform_layout();
    require(window.overflow_records().empty() && window.unanswered_overflow_count() == 0U,
            "content that fits is no overflow");
}

void test_fitting_window_has_no_overflow() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.fits"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("va.fits.ok"), "OK");
    (*button).set_requested_bounds({10.0, 10.0, 80.0, 24.0});
    (*root).add_child(button);
    Window window(root, {400.0, 300.0});
    window.perform_layout();
    require(window.overflow_records().empty(), "a window whose content fits records nothing");
    require(window.unanswered_overflow_count() == 0U && overflow_diagnostics(window) == 0U,
            "and diagnoses nothing");
}

void test_root_larger_than_its_slot() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.root"));
    (*root).minimum_size = Size{500.0, 500.0};
    Window window(root, {300.0, 200.0});
    window.perform_layout();
    const std::optional<OverflowRecord> record =
        record_of(window, *root, OverflowCause::minimum_content_exceeds_viewport);
    require(record.has_value(), "a root larger than the client area is recorded");
    require(near((*record).viewport_extent.width, 300.0) && near((*record).content_extent.height, 500.0) &&
                !(*record).answered,
            "against the root slot, unanswered");
    window.resize({600.0, 600.0});
    window.perform_layout();
    require(!record_of(window, *root, OverflowCause::minimum_content_exceeds_viewport).has_value(),
            "a client area that holds the root ends the overflow");
}

void test_design_sizes() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.design"));
    const std::shared_ptr<Panel> canvas = make_control<Panel>(StableId("va.design.canvas"));
    (*canvas).set_requested_bounds({0.0, 0.0, 400.0, 300.0});
    (*canvas).design_size = Size{800.0, 600.0};
    const std::shared_ptr<ScaledPanel> face =
        make_control<ScaledPanel>(StableId("va.design.face"), Size{800.0, 600.0});
    (*face).set_requested_bounds({400.0, 0.0, 400.0, 300.0});
    (*root).add_child(canvas);
    (*root).add_child(face);
    Window window(root, {800.0, 300.0});
    window.perform_layout();
    const std::optional<OverflowRecord> canvas_record =
        record_of(window, *canvas, OverflowCause::design_size_exceeds_viewport);
    require(canvas_record.has_value() && !(*canvas_record).answered,
            "a design larger than a panel is unanswered before the in-tree scale frame");
    const std::optional<OverflowRecord> face_record =
        record_of(window, *face, OverflowCause::design_size_exceeds_viewport);
    require(face_record.has_value() && (*face_record).answered && near((*face_record).effective_scale, 0.5),
            "a scaled panel's stretch answers its design, at the smaller axis ratio");
    require((*face).applied_viewport_scale().present(), "a scaled panel reports its applied scale");
}

void test_safe_insets_and_the_root_slot() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.safe"));
    Window window(root, {400.0, 300.0});
    window.perform_layout();
    require(window.safe_insets() == SafeInsets{}, "no safe insets until a host reports any");
    require(near_rect(window.root_slot(), Rect{0.0, 0.0, 400.0, 300.0}), "the slot is the client area");
    window.set_safe_insets(SafeInsets{10.0, 20.0, 0.0, 30.0});
    window.perform_layout();
    require(near_rect(window.visible_client_rectangle(), Rect{10.0, 20.0, 390.0, 250.0}),
            "the visible rectangle keeps clear of the safe insets");
    require(near_rect(window.root_slot(), Rect{10.0, 20.0, 390.0, 250.0}), "so does the root slot");
    require(near_rect((*root).arranged_bounds(), Rect{10.0, 20.0, 390.0, 250.0}),
            "the root is arranged in it: " + describe((*root).arranged_bounds()));
    require(refuses(WriteSafeInsets{&window, SafeInsets{0.0, -1.0, 0.0, 0.0}}), "negative safe insets are refused");
    require(window.safe_insets() == SafeInsets{10.0, 20.0, 0.0, 30.0}, "a refused report changes nothing");
}

// -- the hosts' window-size clamp ----------------------------------------------------------

ApplicationWindow headless_entry(const char* id, Size size, Size minimum, Window*& model) {
    ApplicationWindow entry{};
    entry.stable_id = id;
    const std::shared_ptr<Control> root = make_control<Control>(StableId("root"));
    entry.model = std::make_unique<Window>(root, size);
    model = entry.model.get();
    entry.options.title = id;
    entry.options.initial_size = size;
    entry.options.minimum_size = minimum;
    return entry;
}

struct KeepHandle final {
    ApplicationWindowHandle* handle{};
    void operator()(Window&, ApplicationWindowHandle value) const { *handle = value; }
};

void test_oversized_window_is_fitted_to_the_work_area() {
    Window* model = nullptr;
    ApplicationWindowHandle main{};
    std::vector<ApplicationWindow> windows;
    ApplicationWindow primary = headless_entry("main", {800.0, 1200.0}, {100.0, 80.0}, model);
    primary.options.ready = KeepHandle{&main};
    windows.push_back(std::move(primary));
    host::HeadlessApplication application(std::move(windows));
    host::HeadlessWindowing& windowing = application.windowing();
    const std::optional<Rect> content = windowing.native_content(main.id());
    require(content.has_value(), "the window has a native rectangle");
    const double outer_bottom = (*content).y + (*content).height + host::HeadlessWindowing::border_width;
    require(outer_bottom <= 1040.0 && (*content).height < 1200.0,
            "a window taller than the work area is made to fit: " + describe(*content));
    require(near((*model).client_size().height, (*content).height), "the model hears of its new client size");
    require(windowing.snapshot().size_clamped_frames >= 1U, "the fit is counted");

    // A full-screen frame covers the monitor, beyond the work area: left alone.
    const double border = host::HeadlessWindowing::border_width;
    const double title = host::HeadlessWindowing::title_bar_height;
    const Rect full{border, title + border, 1920.0 - 2.0 * border, 1080.0 - title - 2.0 * border};
    application.simulate_user_frame(main.id(), full);
    const std::optional<Rect> kept = windowing.native_content(main.id());
    require(kept.has_value() && near_rect(*kept, full), "a full-screen frame is not fitted");
}

void test_minimum_larger_than_the_work_area_is_unfit() {
    Window* model = nullptr;
    ApplicationWindowHandle main{};
    std::vector<ApplicationWindow> windows;
    ApplicationWindow primary = headless_entry("main", {800.0, 1200.0}, {100.0, 1200.0}, model);
    primary.options.ready = KeepHandle{&main};
    windows.push_back(std::move(primary));
    host::HeadlessApplication application(std::move(windows));
    host::HeadlessWindowing& windowing = application.windowing();
    const std::optional<Rect> content = windowing.native_content(main.id());
    require(content.has_value() && near((*content).height, 1200.0), "the minimum size keeps the window");
    require(windowing.snapshot().unfit_frames >= 1U, "and the window is counted unfit");
    require(near((*model).client_size().height, 1200.0), "the model keeps its size");
}

} // namespace

int main() {
    try {
        test_contain_and_cover_placement();
        test_values_and_names();
        test_properties();
        test_placed_child_overflow_and_its_answers();
        test_fitting_window_has_no_overflow();
        test_root_larger_than_its_slot();
        test_design_sizes();
        test_safe_insets_and_the_root_slot();
        test_oversized_window_is_fitted_to_the_work_area();
        test_minimum_larger_than_the_work_area_is_unfit();
    } catch (const std::exception& error) {
        std::cerr << "viewport-fit-tests: fail: " << error.what() << '\n';
        return 1;
    }
    std::cout << "viewport-fit-tests: pass\n";
    return 0;
}
