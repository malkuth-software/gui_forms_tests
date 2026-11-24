// Stage VA: the visible-area guarantee (owner decisions of 2026-11-22;
// notes planning/stages/VA-visible-area/STUDY.md). The placement
// computation (contain, cover, the scale bounds, alignment) and the scale
// frame's map; the viewport_fit and design_size properties, their
// refusals and the defaults per kind; the window's overflow records,
// content_overflow diagnostics and counters for children placed outside a
// panel, answered by the in-tree contain, by scrolling, by an explicit clip,
// or not at all below the floor; the owner's game case (a root taller than
// its window, and a browser canvas the browser partly hides) shown whole
// through the root frame, which before stage VA's in-tree frame was recorded
// unanswered with the bottom of the root outside the window; design sizes
// laid out and contained; nested contains and the floor on the effective
// scale; pointer, hit testing, the caret and semantics mapped through the
// frame, and a scaled control replayed through a transform group; the
// diagnostic overlay's outlines and the inspection's layout reasons; safe
// insets from a host event and the root slot; and the hosts' window-size
// clamp on the headless reference host, which leaves a full-screen frame
// alone.

#include "gui_forms/application.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/container_controls.hpp"
#include "gui_forms/host.hpp"
#include "gui_forms/diagnostics/invalidation_overlay/invalidation_overlay.hpp"
#include "gui_forms/layout/viewport_fit/viewport_fit.hpp"
#include "gui_forms/scene/scene_view/scene_view.hpp"
#include "gui_forms/types/painter/painter.hpp"
#include "gui_forms/types/scale_frame/scale_frame.hpp"
#include "gui_forms/visual_inspection/types/visual_inspection_types.hpp"
#include "gui_forms/window.hpp"
#include "application/headless_application.hpp"
#include "headless_host.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

bool near_point(Point left, Point right) {
    const bool same = std::abs(left.x - right.x) <= 1.0e-6 && std::abs(left.y - right.y) <= 1.0e-6;
    return same;
}

// The window's presentation settings with `floor` as the minimum content
// scale (decision 5).
void set_content_floor(Window& window, double floor) {
    PresentationSettings settings = window.presentation_settings();
    settings.minimum_content_scale = floor;
    window.set_presentation_settings(settings);
}

// A painter that draws nothing and keeps the scale of every transform group
// it is given (Painter::begin_transform): the window replays a control seen
// through a contain inside one.
class TransformPainter final : public Painter {
public:
    void save() override {}
    void restore() override {}
    void translate(Point) override {}
    void clip_rect(Rect) override {}
    void clip_rounded_rect(Rect, double) override {}
    void fill_rect(Rect, Color) override {}
    void fill_rounded_rect(Rect, double, Color) override {}
    void stroke_rect(Rect, Color, double) override {}
    void stroke_rounded_rect(Rect, double, Color, double) override {}
    void fill_linear_gradient(Rect, Point, Point, std::span<const GradientStop>) override {}
    void fill_radial_gradient(Rect, Point, Size, std::span<const GradientStop>) override {}
    void draw_box_shadow(Rect, double, Point, double, double, Color) override {}
    void draw_line(Point, Point, Color, double) override {}
    void draw_text_utf8(Point, std::string_view, FontSpec, Color) override {}
    void draw_image(ImageId, Rect, double) override {}
    void begin_transform(const PaintTransformGroup& group) override { scales.push_back(group.transform.xx); }
    void end_transform() override {}

    std::vector<double> scales;
};

void paint_whole(Window& window, Painter& painter) {
    window.perform_layout();
    const Size client = window.client_size();
    static_cast<void>(window.paint(painter, Rect{0.0, 0.0, client.width, client.height}));
}

// A press's local position, as the control's mouse_down raiser hands it.
struct KeepLocation final {
    Point* location{};
    void operator()(const MouseEventArgs& arguments) const { *location = arguments.location; }
};

// The semantic node of `stable_id` under `nodes`, or null.
const SemanticNode* semantic_node(const std::vector<SemanticNode>& nodes, std::string_view stable_id) {
    for (const SemanticNode& node : nodes) {
        if (node.stable_id == stable_id) return &node;
        const SemanticNode* const found = semantic_node(node.children, stable_id);
        if (found != nullptr) return found;
    }
    return nullptr;
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

void test_scale_frames() {
    const ScaleFrame identity{};
    require(is_identity_frame(identity), "the default frame is the identity");
    const ScaleFrame half{Point{10.0, 20.0}, 0.5};
    require(near_point(frame_to_outer(half, Point{100.0, 40.0}), Point{60.0, 40.0}), "a point maps out");
    require(near_point(frame_to_inner(half, Point{60.0, 40.0}), Point{100.0, 40.0}), "and back in");
    require(near_rect(frame_to_outer(half, Rect{0.0, 0.0, 200.0, 100.0}), Rect{10.0, 20.0, 100.0, 50.0}),
            "a rectangle stays a rectangle");
    const ScaleFrame quarter{Point{4.0, 0.0}, 0.5};
    const ScaleFrame both = compose_frames(half, quarter);
    require(near(both.scale, 0.25) &&
                near_point(frame_to_outer(both, Point{8.0, 8.0}), frame_to_outer(half, frame_to_outer(quarter, Point{8.0, 8.0}))),
            "composition maps as inner then outer");
    ViewportPlacement placed{};
    placed.scale = 0.5;
    placed.destination = Rect{50.0, 0.0, 300.0, 200.0};
    const ScaleFrame carried = placement_frame(Rect{-100.0, 0.0, 600.0, 400.0}, placed);
    require(near_point(frame_to_outer(carried, Point{-100.0, 0.0}), Point{50.0, 0.0}),
            "a placement's frame takes the content's corner to the destination's");
    ViewportFit fit{};
    fit.minimum_scale = 0.5;
    const ViewportFit nested = fit_within_outer_scale(fit, 0.25);
    require(near(nested.minimum_scale, 2.0) && near(nested.maximum_scale, 4.0) && valid_viewport_fit(nested),
            "the bounds are on the effective scale");

    // The canvases' framing is the same contain.
    const SceneViewTransform framed =
        scene_framed(SceneViewTransform{}, WorldRect{0.0, 0.0, 200.0, 100.0}, Size{400.0, 400.0}, 0.0, SceneZoomRange{});
    require(near(framed.zoom, 2.0) && near(framed.origin.y, -50.0), "scene_framed contains and centres");
}

void test_defaults_per_kind() {
    const std::shared_ptr<Control> plain = make_control<Control>(StableId("va.kind.control"));
    require((*plain).viewport_fit.read().answer == OverflowAnswer::clip, "a control clips by default, recorded");
    const std::shared_ptr<Panel> panel = make_control<Panel>(StableId("va.kind.panel"));
    require((*panel).viewport_fit.read() == panel_viewport_fit, "a Windows Forms panel contains (decision 4)");
    const std::shared_ptr<UserControl> user = make_control<UserControl>(StableId("va.kind.user"));
    require((*user).viewport_fit.read() == panel_viewport_fit, "so does a user control");
    const std::shared_ptr<FlexPanel> flex = make_control<FlexPanel>(StableId("va.kind.flex"));
    require((*flex).viewport_fit.read() == reflowing_viewport_fit, "a CSS panel reflows, then scrolls");
    const std::shared_ptr<PictureBox> picture = make_control<PictureBox>(StableId("va.kind.picture"));
    require((*picture).viewport_fit.read() == media_viewport_fit && (*picture).shows_media_content(),
            "media contains with no floor");
    require(!(*panel).shows_media_content(), "a panel shows controls");
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
    require(!static_cast<const Control&>(*panel).content_scrolls(), "a panel without auto scroll does not scroll its content");
    (*panel).auto_scroll = true;
    require(static_cast<const Control&>(*panel).content_scrolls(), "auto scroll scrolls the content");
}

// -- the window's records ------------------------------------------------------------------

void test_placed_child_overflow_and_its_answers() {
    const std::shared_ptr<Panel> viewer = make_control<Panel>(StableId("va.viewer"));
    const std::shared_ptr<Label> page = make_control<Label>(StableId("va.viewer.page"), "A long page");
    (*page).set_requested_bounds({0.0, 0.0, 600.0, 900.0});
    (*viewer).add_child(page);
    Window window(viewer, {300.0, 200.0});
    window.perform_layout();

    // 900 tall in 200: a contain of 2/9, below the floor of 1/2.
    std::optional<OverflowRecord> record =
        record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value(), "a child placed outside the panel is recorded");
    require((*record).answer == OverflowAnswer::contain && !(*record).answered && near((*record).effective_scale, 0.5),
            "a contain below the floor takes its fallback, scroll, at the floor: unanswered in a panel");
    require(near((*record).content_extent.width, 600.0) && near((*record).content_extent.height, 900.0) &&
                near((*record).viewport_extent.width, 300.0) && near((*record).viewport_extent.height, 200.0),
            "the record holds both extents");
    require(near((*viewer).content_frame().scale, 0.5), "the content is shown at the floor, cut");
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

    // A lower floor lets the contain answer it, through the content frame.
    set_content_floor(window, 0.1);
    window.perform_layout();
    record = record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    const double ratio = 200.0 / 900.0;
    require(record.has_value() && (*record).answered && near((*record).effective_scale, ratio),
            "a contain above the floor answers");
    require(window.unanswered_overflow_count() == 0U, "nothing is unanswered");
    const double shown_width = 600.0 * ratio;
    require(near_rect((*page).absolute_bounds(), Rect{(300.0 - shown_width) * 0.5, 0.0, shown_width, 200.0}),
            "the page is shown whole and centred: " + describe((*page).absolute_bounds()));
    require(near((*page).window_frame().scale, ratio), "its window frame carries the scale");
    require(overflow_diagnostics(window) == 2U && window.layout_diagnostics().back().severity ==
                                                       LayoutDiagnosticSeverity::warning,
            "the changed answer is diagnosed, as a warning");

    // Auto scroll answers it by scrolling, and the frame ends.
    (*viewer).auto_scroll = true;
    window.perform_layout();
    record = record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value() && (*record).answer == OverflowAnswer::scroll && (*record).answered,
            "a scrolling panel answers by scrolling");
    require(is_identity_frame((*viewer).content_frame()), "a scroll applies no frame");
    metrics = window.metrics_snapshot();
    require(metrics.overflow_scrolled == 1U, "the scroll is counted");

    // An explicit clip answers it too.
    (*viewer).auto_scroll = false;
    ViewportFit clip{};
    clip.answer = OverflowAnswer::clip;
    (*viewer).viewport_fit = clip;
    window.perform_layout();
    record = record_of(window, *viewer, OverflowCause::placed_child_outside_viewport);
    require(record.has_value() && (*record).answer == OverflowAnswer::clip && (*record).answered &&
                is_identity_frame((*viewer).content_frame()),
            "an explicit clip answers, at its own size");

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
                !(*record).answered && near(window.root_frame().scale, 0.5),
            "against the root slot; a contain of 0.4 is below the floor, unanswered at the floor");
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
    require(canvas_record.has_value() && (*canvas_record).answered && near((*canvas_record).effective_scale, 0.5),
            "a design larger than a panel is contained");
    require(near((*canvas).content_frame().scale, 0.5), "through the panel's content frame");
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

// Decision 10: a scaled panel stays an explicit, recorded stretch, and its
// aspect setting keeps the design's proportions in its uniform mode.
void test_scaled_panel_aspect() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.aspect"));
    const std::shared_ptr<ScaledPanel> face = make_control<ScaledPanel>(StableId("va.aspect.face"), Size{100.0, 100.0});
    (*face).set_requested_bounds({0.0, 0.0, 400.0, 200.0});
    const std::shared_ptr<Label> square = make_control<Label>(StableId("va.aspect.square"), "Square");
    (*face).add_at(square, Rect{0.0, 0.0, 100.0, 100.0});
    (*root).add_child(face);
    Window window(root, {400.0, 200.0});
    window.perform_layout();
    require((*face).aspect.read() == ScaledAspect::stretch, "a scaled panel stretches by default");
    require(near_rect((*square).arranged_bounds(), Rect{0.0, 0.0, 400.0, 200.0}),
            "a stretch fills both axes: " + describe((*square).arranged_bounds()));

    (*face).aspect = ScaledAspect::uniform;
    window.perform_layout();
    require(near_rect((*square).arranged_bounds(), Rect{100.0, 0.0, 200.0, 200.0}),
            "uniform keeps the proportions, centred: " + describe((*square).arranged_bounds()));
    ViewportFit left = (*face).viewport_fit.read();
    left.alignment = ContentAlignment::top_left;
    (*face).viewport_fit = left;
    window.perform_layout();
    require(near_rect((*square).arranged_bounds(), Rect{0.0, 0.0, 200.0, 200.0}),
            "the alignment comes from viewport_fit: " + describe((*square).arranged_bounds()));
    const stxlib::Optional<double> applied = (*face).applied_viewport_scale();
    require(applied.present() && near(*applied.value(), 2.0),
            "the scale is reported");
    require(window.unanswered_overflow_count() == 0U, "a scaled panel's content is never an unanswered overflow");
}

// -- the in-tree scale frame ----------------------------------------------------------------

// The owner's case: a game's root taller than its window, and a browser
// canvas the browser hides partly. Before the in-tree frame the root's
// bottom lay outside the window and the overflow was recorded unanswered.
void test_owner_game_case() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.game"));
    (*root).minimum_size = Size{400.0, 900.0};
    const std::shared_ptr<Button> start = make_control<Button>(StableId("va.game.start"), "Start");
    (*start).set_requested_bounds({10.0, 860.0, 100.0, 30.0});
    (*root).add_child(start);
    Window window(root, {600.0, 600.0});
    window.perform_layout();

    const std::optional<OverflowRecord> record =
        record_of(window, *root, OverflowCause::minimum_content_exceeds_viewport);
    const double ratio = 600.0 / 900.0;
    require(record.has_value() && (*record).answer == OverflowAnswer::contain && (*record).answered &&
                near((*record).effective_scale, ratio),
            "the root's contain answers its overflow");
    require(window.unanswered_overflow_count() == 0U, "nothing is unanswered");
    require(near(window.root_frame().scale, ratio), "through the root frame");
    const Rect shown = (*start).absolute_bounds();
    require(shown.y + shown.height <= 600.0 + 1.0e-9 && shown.x >= 0.0,
            "the button at the root's bottom is inside the window: " + describe(shown));
    const Point centre{shown.x + shown.width * 0.5, shown.y + shown.height * 0.5};
    require(window.hit_test(centre) == start, "and the pointer reaches it there");

    // The browser hides the canvas's bottom 100 units (its visual viewport).
    host::HeadlessHost host(window);
    require(host.dispatch(HostAttachEvent{Size{600.0, 600.0}, 1.0}, 5U).accepted(), "the host attaches");
    const HostDispatchResult reported = host.dispatch(HostSafeInsetsEvent{SafeInsets{0.0, 0.0, 0.0, 100.0}}, 10U);
    require(reported.accepted(), "a host reports safe insets");
    require(window.safe_insets() == SafeInsets{0.0, 0.0, 0.0, 100.0}, "the window takes them");
    window.perform_layout();
    const Rect hidden_shown = (*start).absolute_bounds();
    require(hidden_shown.y + hidden_shown.height <= 500.0 + 1.0e-9,
            "the root is contained within what the browser shows: " + describe(hidden_shown));
    require(near(window.root_frame().scale, 500.0 / 900.0), "at the smaller scale");
    const HostDispatchResult refused = host.dispatch(HostSafeInsetsEvent{SafeInsets{-1.0, 0.0, 0.0, 0.0}}, 11U);
    require(refused.error == HostDispatchError::invalid_geometry, "invalid safe insets are refused");
}

void test_design_size_layout_and_contain() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.designed"));
    (*root).design_size = Size{800.0, 600.0};
    const std::shared_ptr<Panel> fill = make_control<Panel>(StableId("va.designed.fill"));
    (*fill).dock = DockStyle::fill;
    const std::shared_ptr<Button> corner = make_control<Button>(StableId("va.designed.corner"), "Corner");
    (*corner).set_requested_bounds({700.0, 550.0, 100.0, 50.0});
    (*root).add_child(fill);
    (*root).add_child(corner);
    Window window(root, {400.0, 300.0});
    window.perform_layout();
    require(near_rect((*root).arranged_bounds(), Rect{0.0, 0.0, 800.0, 600.0}),
            "a root with a design size is laid out at it: " + describe((*root).arranged_bounds()));
    require(near(window.root_frame().scale, 0.5), "and contained into its slot");
    require(near_rect((*fill).absolute_bounds(), Rect{0.0, 0.0, 400.0, 300.0}),
            "a docked child fills the design, seen in the window: " + describe((*fill).absolute_bounds()));
    require(near_rect((*corner).absolute_bounds(), Rect{350.0, 275.0, 50.0, 25.0}),
            "a placed child keeps its design place: " + describe((*corner).absolute_bounds()));
    const std::optional<OverflowRecord> record = record_of(window, *root, OverflowCause::design_size_exceeds_viewport);
    require(record.has_value() && (*record).answered, "the design's overflow is answered");

    // A larger window shows the design at its own size (contain never enlarges by default).
    window.resize({1000.0, 800.0});
    window.perform_layout();
    require(is_identity_frame(window.root_frame()), "a design that fits is not scaled");
}

void test_nested_contains_and_the_floor() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.nested"));
    const std::shared_ptr<Panel> outer = make_control<Panel>(StableId("va.nested.outer"));
    (*outer).set_requested_bounds({0.0, 0.0, 600.0, 400.0});
    (*outer).design_size = Size{1000.0, 400.0};
    const std::shared_ptr<Panel> inner = make_control<Panel>(StableId("va.nested.inner"));
    (*inner).set_requested_bounds({0.0, 0.0, 500.0, 300.0});
    const std::shared_ptr<Label> leaf = make_control<Label>(StableId("va.nested.leaf"), "Leaf");
    (*leaf).set_requested_bounds({0.0, 0.0, 700.0, 300.0});
    (*inner).add_child(leaf);
    (*outer).add_child(inner);
    (*root).add_child(outer);
    Window window(root, {600.0, 400.0});
    window.perform_layout();
    require(near((*outer).content_frame().scale, 0.6), "the outer panel contains its design at 0.6");
    // The inner one needs 500/700 locally: an effective 0.43, below the floor of 0.5.
    const std::optional<OverflowRecord> record =
        record_of(window, *inner, OverflowCause::placed_child_outside_viewport);
    require(record.has_value() && !(*record).answered, "an inner contain cannot escape the floor by nesting");
    require(near((*leaf).window_frame().scale, 0.5), "the leaf is shown at the floor: " +
                                                         std::to_string((*leaf).window_frame().scale));
    require(near((*inner).content_frame().scale, 0.5 / 0.6), "the inner frame makes up the rest");

    set_content_floor(window, 0.25);
    window.perform_layout();
    const std::optional<OverflowRecord> answered =
        record_of(window, *inner, OverflowCause::placed_child_outside_viewport);
    require(answered.has_value() && (*answered).answered, "above the floor the inner contain answers");
    require(near((*leaf).window_frame().scale, 0.6 * 500.0 / 700.0), "the scales multiply");
}

void test_input_caret_semantics_and_replay() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.input"));
    (*root).design_size = Size{800.0, 600.0};
    const std::shared_ptr<Control> probe = make_control<Control>(StableId("va.input.probe"));
    (*probe).set_requested_bounds({200.0, 100.0, 200.0, 100.0});
    const std::shared_ptr<TextBox> field = make_control<TextBox>(StableId("va.input.field"));
    (*field).set_requested_bounds({200.0, 300.0, 300.0, 40.0});
    (*root).add_child(probe);
    (*root).add_child(field);
    Window window(root, {400.0, 300.0});
    window.perform_layout();
    require(near(window.root_frame().scale, 0.5), "the root is contained at one half");

    require(near_point((*probe).point_to_window(Point{100.0, 50.0}), Point{150.0, 75.0}), "local to window");
    require(near_point((*probe).point_from_window(Point{150.0, 75.0}), Point{100.0, 50.0}), "window to local");
    require(near_rect((*probe).rectangle_to_window(Rect{0.0, 0.0, 200.0, 100.0}), Rect{100.0, 50.0, 100.0, 50.0}),
            "a local rectangle in the window");
    require(window.hit_test(Point{150.0, 75.0}) == probe, "hit testing maps through the frame");
    require(window.hit_test(Point{250.0, 75.0}) != probe, "and misses beside the scaled control");

    Point pressed{};
    const SubscriptionToken token = (*probe).mouse_down().subscribe(KeepLocation{&pressed});
    static_cast<void>(window.dispatch_pointer({PointerAction::down, PointerButton::primary, Point{150.0, 75.0}}));
    static_cast<void>(window.dispatch_pointer({PointerAction::up, PointerButton::primary, Point{150.0, 75.0}}));
    require(near_point(pressed, Point{100.0, 50.0}),
            "a handler sees its own, unscaled coordinates: " + std::to_string(pressed.x) + ", " +
                std::to_string(pressed.y));

    require(window.request_focus(field), "the field takes focus");
    const stxlib::Optional<Rect> caret = window.text_input_caret();
    const Rect field_shown = (*field).absolute_bounds();
    require(caret.present() && (*caret.value()).height <= field_shown.height + 1.0e-9 &&
                Rect::intersection(*caret.value(), field_shown) == *caret.value(),
            "the input method's caret lies in the field as it is seen");

    const SemanticSnapshot semantics = window.semantic_snapshot();
    const SemanticNode* const node = semantic_node(semantics.roots, "va.input.field");
    require(node != nullptr && near_rect((*node).bounds, field_shown) &&
                near_rect(field_shown, Rect{100.0, 150.0, 150.0, 20.0}) && near((*node).effective_scale, 0.5),
            "the semantic node publishes the scaled rectangle and its scale");

    TransformPainter painter;
    paint_whole(window, painter);
    bool replayed = false;
    for (const double scale : painter.scales) {
        if (near(scale, 0.5)) replayed = true;
    }
    require(replayed, "a contained control replays through a transform group at its scale");
}

void test_overlay_outlines_and_inspection() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("va.overlay"));
    const std::shared_ptr<Panel> cut = make_control<Panel>(StableId("va.overlay.cut"));
    (*cut).set_requested_bounds({0.0, 0.0, 200.0, 100.0});
    ViewportFit clip{};
    clip.answer = OverflowAnswer::clip;
    (*cut).viewport_fit = clip;
    const std::shared_ptr<Label> wide = make_control<Label>(StableId("va.overlay.cut.wide"), "Wide");
    (*wide).set_requested_bounds({0.0, 0.0, 400.0, 50.0});
    (*cut).add_child(wide);
    const std::shared_ptr<FlexPanel> flow = make_control<FlexPanel>(StableId("va.overlay.flow"));
    (*flow).set_requested_bounds({0.0, 120.0, 100.0, 40.0});
    const std::shared_ptr<Label> long_label = make_control<Label>(StableId("va.overlay.flow.long"), "Long");
    (*long_label).set_requested_bounds({0.0, 0.0, 300.0, 40.0});
    (*long_label).minimum_size = Size{300.0, 40.0};
    (*flow).add_child(long_label);
    (*root).add_child(cut);
    (*root).add_child(flow);
    Window window(root, {400.0, 300.0});
    window.set_invalidation_overlay(stxlib::Optional<bool>::of(true));
    TransformPainter painter;
    paint_whole(window, painter);
    InvalidationOverlay* const overlay = window.invalidation_overlay();
    require(overlay != nullptr, "the overlay is on");
    bool clipped = false;
    bool unanswered = false;
    const stxlib::View<OverflowOutline> outlines = (*overlay).overflow_outlines();
    for (stxlib::usize index = 0U; index < outlines.count(); ++index) {
        if (outlines[index].kind == OverflowOutlineKind::clipped) clipped = true;
        if (outlines[index].kind == OverflowOutlineKind::unanswered) unanswered = true;
    }
    require(clipped, "a clipped owner is outlined in amber");
    require(unanswered == (window.unanswered_overflow_count() > 0U), "an unanswered one in red");
    require(overflow_outline_style(OverflowOutlineKind::unanswered).colour !=
                overflow_outline_style(OverflowOutlineKind::clipped).colour,
            "the two outlines differ");

    const VisualInspectionSnapshot snapshot = window.visual_inspection_snapshot();
    bool reasons = false;
    for (const VisualControlInspection& control : snapshot.controls) {
        if (control.stable_id != "va.overlay.cut") continue;
        reasons = control.layout.overflows.count() != 0U && control.layout.overflows[0].answer == OverflowAnswer::clip;
    }
    require(reasons, "the inspection lists the owner's overflow as a layout reason");
    require(snapshot.to_json().find("\"viewport\":{") != std::string::npos, "and writes it as JSON");
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
        test_scale_frames();
        test_defaults_per_kind();
        test_properties();
        test_placed_child_overflow_and_its_answers();
        test_fitting_window_has_no_overflow();
        test_root_larger_than_its_slot();
        test_design_sizes();
        test_scaled_panel_aspect();
        test_safe_insets_and_the_root_slot();
        test_owner_game_case();
        test_design_size_layout_and_contain();
        test_nested_contains_and_the_floor();
        test_input_caret_semantics_and_replay();
        test_overlay_outlines_and_inspection();
        test_oversized_window_is_fitted_to_the_work_area();
        test_minimum_larger_than_the_work_area_is_unfit();
    } catch (const std::exception& error) {
        std::cerr << "viewport-fit-tests: fail: " << error.what() << '\n';
        return 1;
    }
    std::cout << "viewport-fit-tests: pass\n";
    return 0;
}
