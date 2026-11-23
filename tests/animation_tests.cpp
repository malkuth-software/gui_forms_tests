#include "gui_forms/animation.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/diagnostic_controls.hpp"
#include "gui_forms/live_surface.hpp"
#include "gui_forms/range_controls.hpp"
#include "gui_forms/window.hpp"
#include "gui_forms_demo/easing_preview.hpp"
#include "support/frame_wake.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

using gui_forms::test_support::armed_wake;

namespace {

using namespace gui_forms;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool close_to(double left, double right, double tolerance = 0.000001) {
    return std::abs(left - right) <= tolerance;
}

void test_easing_endpoints_and_curves() {
    for (const EasingCurve curve : {
             EasingCurve::linear, EasingCurve::ease_in,
             EasingCurve::ease_out, EasingCurve::ease_in_out,
             EasingCurve::smooth_step, EasingCurve::back_out,
             EasingCurve::bounce_out, EasingCurve::elastic_out}) {
        require(close_to(apply_easing(curve, 0.0), 0.0) &&
                    close_to(apply_easing(curve, 1.0), 1.0),
                "every easing curve must preserve exact endpoints");
    }
    require(apply_easing(EasingCurve::ease_in, 0.5) < 0.5 &&
                apply_easing(EasingCurve::ease_out, 0.5) > 0.5 &&
                close_to(apply_easing(EasingCurve::smooth_step, 0.5), 0.5),
            "easing families must retain their declared timing character");
}

void test_timeline_delay_repeat_direction_and_finish() {
    AnimationSpec specification;
    specification.delay = 50ms;
    specification.duration = 100ms;
    specification.iterations = 2.0;
    specification.easing = EasingCurve::linear;
    specification.direction = AnimationDirection::alternate;
    AnimationTimeline timeline(specification);
    const FrameTime start{};
    timeline.start(start);

    const AnimationSample delayed = timeline.sample(start + 25ms);
    const AnimationSample first = timeline.sample(start + 100ms);
    const AnimationSample second = timeline.sample(start + 200ms);
    const AnimationSample finished = timeline.sample(start + 250ms);
    require(delayed.active && delayed.delayed && close_to(delayed.progress, 0.0),
            "timeline delay must preserve the directed initial value");
    require(first.active && first.iteration == 0U &&
                close_to(first.progress, 0.5),
            "first timeline iteration must advance normally");
    require(second.active && second.iteration == 1U &&
                close_to(second.progress, 0.5),
            "alternate timeline iteration must reverse deterministically");
    require(finished.finished && !finished.active &&
                close_to(finished.progress, 0.0),
            "finite alternate timeline must finish at its directed endpoint");
}

void test_timeline_pause_resume_retains_phase_without_catch_up() {
    AnimationSpec specification;
    specification.duration = 1000ms;
    specification.infinite = true;
    AnimationTimeline timeline(specification);
    const FrameTime start{};
    timeline.start(start);
    require(close_to(timeline.sample(start + 250ms).progress, 0.25),
            "timeline must advance before suspension");

    timeline.pause(start + 250ms);
    require(timeline.running() && timeline.paused() &&
                close_to(timeline.sample(start + 5s).progress, 0.25),
            "paused timeline must retain its exact phase at arbitrary sample times");
    timeline.pause(start + 10s);
    require(close_to(timeline.sample(start + 20s).progress, 0.25),
            "repeated pause must be idempotent");

    timeline.resume(start + 1250ms);
    require(timeline.running() && !timeline.paused() &&
                close_to(timeline.sample(start + 1250ms).progress, 0.25) &&
                close_to(timeline.sample(start + 1500ms).progress, 0.5),
            "resume must rebase suspended time and continue from the retained phase");
    timeline.resume(start + 2s);
    require(close_to(timeline.sample(start + 1750ms).progress, 0.75),
            "repeated resume must be idempotent");

    for (std::size_t cycle = 0U; cycle < 16U; ++cycle) {
        const FrameTime pause_at = start + 1750ms + 100ms * cycle;
        const double before = timeline.sample(pause_at).progress;
        timeline.pause(pause_at);
        timeline.resume(pause_at + 40ms);
        require(close_to(timeline.sample(pause_at + 40ms).progress, before),
                "every pause/resume cycle must preserve its boundary phase");
    }
}

void test_invalid_timeline_contract() {
    bool duration_rejected = false;
    bool iterations_rejected = false;
    try {
        AnimationSpec invalid;
        invalid.duration = FrameInterval::zero();
        AnimationTimeline timeline(invalid);
        static_cast<void>(timeline);
    } catch (const std::invalid_argument&) {
        duration_rejected = true;
    }
    try {
        AnimationSpec invalid;
        invalid.iterations = 0.0;
        AnimationTimeline timeline(invalid);
        static_cast<void>(timeline);
    } catch (const std::invalid_argument&) {
        iterations_rejected = true;
    }
    require(duration_rejected && iterations_rejected,
            "timeline must reject degenerate duration and finite iteration count");
}

void test_reduced_motion_remains_active_and_calm() {
    constexpr MotionPolicy reduced{true, false, true};
    static_assert(reduced.active());
    static_assert(!reduced.quiescent());
    static_assert(reduced.frame_interval(16ms) == 100ms);
    static_assert(reduced.speed_scale() == 0.35);
    static_assert(reduced.presentation_phase(0.0) == 0.25);
    static_assert(reduced.presentation_phase(1.0) == 0.75);

    constexpr MotionPolicy paused_reduced{true, true, true};
    constexpr MotionPolicy disabled_reduced{false, false, true};
    static_assert(paused_reduced.quiescent());
    static_assert(disabled_reduced.quiescent());
    require(reduced.active() && paused_reduced.quiescent() &&
                disabled_reduced.quiescent(),
            "reduced motion must remain active; only pause and disable may quiesce it");
}

class StripePainter final : public Painter {
public:
    void save() override { ++saves; }
    void restore() override { ++restores; }
    void translate(Point) override {}
    void clip_rect(Rect bounds) override { clip = bounds; }
    void fill_rect(Rect, Color) override {}
    void stroke_rect(Rect, Color, double) override {}
    void draw_line(Point, Point, Color, double) override { ++lines; }
    // Each marching band is one four-point polygon: its near side's two
    // corners (across 0), then its far side's (across the thickness).
    void fill_polygon(std::span<const Point> points, Color, PolygonFillRule) override {
        ++bands;
        if (points.size() != 4U) {
            leans_back = false;
            return;
        }
        band_width = points[1].x - points[0].x;
        slant = points[3].x - points[0].x;
        if (!(points[3].y > points[0].y) || !(slant > 0.0)) leans_back = false;
    }
    void draw_text_utf8(Point, std::string_view, FontSpec, Color) override {}
    void draw_image(ImageId, Rect, double) override {}

    int saves{};
    int restores{};
    int lines{};
    int bands{};
    Rect clip{};
    double band_width{};
    double slant{};
    bool leans_back{true};
};

// ADR-016: each tick damages only what the phase moved, and a bar that is
// painted keeps its timer.
void test_progress_animation_damages_what_moves() {
    std::shared_ptr<gui_forms::Panel> root = make_control<Panel>(StableId("animation.damage.root"));
    std::shared_ptr<gui_forms::ProgressBar> progress =
        make_control<ProgressBar>(StableId("animation.damage.progress"));
    (*progress).set_requested_bounds({0.0, 0.0, 240.0, 20.0});
    (*progress).value = 50.0;
    (*progress).visual_style = ProgressBarVisualStyle::marching_stripes;
    (*progress).animation_period = 1000ms;
    (*root).add_child(progress);
    Window window(root, {240.0, 40.0});
    window.perform_layout();
    for (std::size_t tick = 0U; tick < 3U; ++tick) {
        StripePainter painter;
        (*progress).on_paint(painter, {0.0, 0.0, 240.0, 20.0});
        static_cast<void>(window.take_damage());
        const std::uint64_t before = (*progress).effect_instance().counters().ticks;
        static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
        const DamageRegion damage = window.take_damage();
        const Rect bounds = damage.bounds();
        // The stripes are an effect program on the motion clock: its frame
        // makes ticks due, which the next paint runs.
        StripePainter after;
        (*progress).on_paint(after, {0.0, 0.0, 240.0, 20.0});
        // The fill is the left half of the 236-pixel interior, inset 2.
        require((*progress).effect_instance().counters().ticks > before && !bounds.empty() &&
                    bounds.x >= 1.0 && bounds.x + bounds.width <= 121.0 &&
                    bounds.y >= 1.0 && bounds.y + bounds.height <= 19.0 &&
                    window.next_wake().present(),
                "a painted marching-stripe bar must damage only its fill on each tick");
    }
}

// A control animated by an AnimationTimer: counts ticks, reports them as
// damaging or not, and can stop its own timer from a tick.
class TimerProbe final : public Control {
public:
    explicit TimerProbe(StableId id) : Control(std::move(id)) {}

    struct Tick final {
        TimerProbe* probe;
        AnimationTickResult operator()(FrameTime now) const {
            ++(*probe).ticks;
            (*probe).last_tick = now;
            if ((*probe).stop_in_tick) (*probe).timer.stop();
            const AnimationTickResult result = (*probe).damaging
                ? AnimationTickResult::damaged : AnimationTickResult::unchanged;
            return result;
        }
    };

    void on_paint(Painter&, Rect) override { timer.note_painted(); }

    AnimationTimer timer;
    int ticks{};
    FrameTime last_tick{};
    bool damaging{true};
    bool stop_in_tick{};
};

void test_animation_timer_is_paint_gated() {
    require(animation_interval_for_travel(20.0) == 50ms &&
                animation_interval_for_travel(1000.0) == display_frame_interval &&
                animation_interval_for_travel(0.0) == display_frame_interval &&
                animation_interval_for_travel(std::nan("")) == display_frame_interval &&
                animation_interval_for_travel(1.0e-300) == std::chrono::seconds(3600),
            "travel pacing must give one device pixel a tick, between the display and one hour");

    std::shared_ptr<TimerProbe> probe = make_control<TimerProbe>(StableId("animation.timer.probe"));
    (*probe).set_requested_bounds({0.0, 0.0, 40.0, 20.0});
    Window window(probe, {40.0, 20.0});
    StripePainter painter;
    bool rejected = false;
    try {
        (*probe).timer.start(*probe, 500us, TimerProbe::Tick{probe.get()});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && !(*probe).timer.started(),
            "an interval below one millisecond must be rejected without starting");

    // An interval of a display interval or more ticks at the nearest whole
    // number of display intervals, so that timers of different speeds share
    // frame polls.
    (*probe).timer.start(*probe, 20ms, TimerProbe::Tick{probe.get()});
    const FrameInterval near_one = armed_wake(window) - FrameClock::now();
    require(armed_wake(window).time_since_epoch() % display_frame_interval == FrameInterval{} &&
                near_one <= display_frame_interval && (*probe).timer.interval() == 20ms,
            "a 20 ms timer must tick at one display interval and report its given interval");
    (*probe).timer.start(*probe, 25ms, TimerProbe::Tick{probe.get()});
    require(armed_wake(window).time_since_epoch() % (display_frame_interval * 2) == FrameInterval{},
            "a 25 ms timer must tick at two display intervals");
    (*probe).timer.start(*probe, 32ms, TimerProbe::Tick{probe.get()});
    require((*probe).timer.ticking() && window.next_wake().present() &&
                armed_wake(window).time_since_epoch() % 32ms == FrameInterval{},
            "a started timer's first tick must fall on a multiple of its interval");
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require((*probe).ticks == 1 && (*probe).timer.ticking(),
            "the first tick counts the paint its start requested");
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require((*probe).ticks == 1 && !(*probe).timer.ticking() &&
                (*probe).timer.started() && window.next_wake().absent(),
            "a tick after an unpainted damaging tick must stop the timer and hold no wake");
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require((*probe).timer.ticking() && window.next_wake().present(),
            "the owner's next paint must start a waiting timer");

    // A late poll is one tick, and the cadence keeps its phase.
    const FrameTime due = armed_wake(window);
    const FramePollResult late = window.poll_frame_schedule(due + 105ms);
    require(late.ui_timer_ticks == 1U && (*probe).ticks == 2 &&
                armed_wake(window) == due + 128ms,
            "a late tick must be one tick, the next one the first deadline of its cadence");

    // A tick that changes nothing expects no paint and stays scheduled.
    (*probe).damaging = false;
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require((*probe).ticks == 4 && (*probe).timer.ticking(),
            "unchanged ticks must keep the timer running without paints");

    // Hidden: the timer waits for a paint instead of holding a wake.
    (*probe).visible = false;
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require((*probe).ticks == 4 && window.next_wake().absent() &&
                (*probe).timer.started(),
            "a hidden owner's timer must stop at its next tick without ticking");
    (*probe).visible = true;
    (*probe).timer.start(*probe, 32ms, TimerProbe::Tick{probe.get()});
    (*probe).visible = false;
    (*probe).timer.start(*probe, 32ms, TimerProbe::Tick{probe.get()});
    require(!(*probe).timer.ticking() && window.next_wake().absent(),
            "starting for a hidden owner must wait for its paint");
    (*probe).visible = true;
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require((*probe).timer.ticking(), "a paint must start a timer started while hidden");

    // A tick may stop its own timer; a stopped timer is not restarted by paint.
    (*probe).stop_in_tick = true;
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require((*probe).ticks == 5 && !(*probe).timer.started() &&
                window.next_wake().absent(),
            "a timer stopped by its tick must stay stopped through paints");

    // The window's cancel_frame_requests stops a ticking timer, and the
    // next paint does not start it again.
    (*probe).stop_in_tick = false;
    (*probe).timer.start(*probe, 20ms, TimerProbe::Tick{probe.get()});
    window.cancel_frame_requests();
    require(!(*probe).timer.ticking() && !(*probe).timer.started(),
            "cancelling the window's frame requests must stop a ticking timer");
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require(!(*probe).timer.ticking() && !(*probe).timer.started() &&
                window.next_wake().absent(),
            "a paint must not start a timer its window cancelled");

    // A timer waiting for a paint holds no request, and is stopped as well
    // (the Gallery accessibility clients freeze a page this way before
    // reading it: a hidden page's animation started at its first paint).
    (*probe).visible = false;
    (*probe).timer.start(*probe, 20ms, TimerProbe::Tick{probe.get()});
    require((*probe).timer.started() && !(*probe).timer.ticking(),
            "a hidden owner's timer must wait for a paint");
    window.cancel_frame_requests();
    require(!(*probe).timer.started(),
            "cancelling the window's frame requests must stop a timer waiting for a paint");
    (*probe).visible = true;
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require(!(*probe).timer.ticking() && !(*probe).timer.started() &&
                window.next_wake().absent(),
            "a paint must not start a waiting timer its window cancelled");

    // Another window's cancellation leaves the timer waiting; start after a
    // cancellation runs again.
    std::shared_ptr<TimerProbe> other_root = make_control<TimerProbe>(StableId("animation.timer.other"));
    Window other(other_root, {40.0, 20.0});
    (*probe).visible = false;
    (*probe).timer.start(*probe, 20ms, TimerProbe::Tick{probe.get()});
    other.cancel_frame_requests();
    (*probe).visible = true;
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require((*probe).timer.started() && (*probe).timer.ticking() && window.next_wake().present(),
            "another window's cancellation must not stop the timer, and start must run it again");
}

void test_progress_animation_and_hidden_suspension() {
    std::shared_ptr<gui_forms::Panel> root = make_control<Panel>(StableId("animation.root"));
    std::shared_ptr<gui_forms::ProgressBar> progress = make_control<ProgressBar>(StableId("animation.progress"));
    (*progress).set_requested_bounds({0.0, 0.0, 240.0, 20.0});
    (*progress).visual_style = ProgressBarVisualStyle::marquee;
    (*progress).animation_period = 1000ms;
    (*root).add_child(progress);
    Window window(root, {240.0, 20.0});
    require(window.next_wake().present(),
            "animated progress style must register a retained frame source");

    const FrameTime origin = FrameClock::now();
    (*progress).advance_animation(origin);
    (*progress).advance_animation(origin + 250ms);
    require((*progress).animation_phase() > 0.24 &&
                (*progress).animation_phase() < 0.26,
            "progress animation phase must follow its declared period");

    // The animation runs on a UI timer (ADR-016). A tick that finds the bar
    // unpainted since the last one stops the timer; the bar's next paint
    // starts it again.
    (*progress).visible = false;
    const double hidden_phase = (*progress).animation_phase();
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require(window.next_wake().absent() &&
                close_to((*progress).animation_phase(), hidden_phase),
            "a hidden progress bar must stop its timer at its next tick, without advancing");
    (*progress).visible = true;
    StripePainter shown_painter;
    (*progress).on_paint(shown_painter, {0.0, 0.0, 240.0, 20.0});
    require(window.next_wake().present(),
            "a reshown progress bar must start its timer again when painted");
    (*progress).visual_style = ProgressBarVisualStyle::continuous;
    require(window.next_wake().absent(),
            "static progress style must revoke its retained frame source");

    (*progress).overlay_style = ProgressBarOverlayStyle::moving_stripes;
    // The stripes are an effect program: the bar's paint enlists them on the
    // window's motion clock.
    StripePainter striped_painter;
    (*progress).on_paint(striped_painter, {0.0, 0.0, 240.0, 20.0});
    require(window.next_wake().present(),
            "moving stripes must animate a determinate progress style");
    (*progress).animation_enabled = false;
    require(window.next_wake().absent() && (*progress).animation_phase() == 0.0,
            "disabled stripe motion must paint statically without an idle wake");
    (*progress).animation_enabled = true;
    (*progress).motion_policy = MotionPolicy{true, true, false};
    require(window.next_wake().absent(), "paused stripes must leave the motion clock");
    (*progress).motion_policy = MotionPolicy{true, false, false};
    StripePainter resumed_painter;
    (*progress).on_paint(resumed_painter, {0.0, 0.0, 240.0, 20.0});
    require(window.next_wake().present(), "resumed stripes must enlist on the motion clock again when painted");
    // The stripes are an effect program on the window's motion clock
    // (effect_program_tests); the bar's own timer lease (ADR-016) now drives
    // the marquee alone, which the rest of this test follows.
    (*progress).overlay_style = ProgressBarOverlayStyle::none;
    (*progress).visual_style = ProgressBarVisualStyle::marquee;
    (*progress).advance_animation(FrameClock::now() + 325ms);
    const double retained_phase = (*progress).animation_phase();
    (*progress).motion_policy = MotionPolicy{true, true, false};
    require(window.next_wake().absent() &&
                close_to((*progress).animation_phase(), retained_phase),
            "paused progress motion must revoke its lease without destroying phase");
    (*progress).motion_policy = MotionPolicy{true, false, false};
    require(window.next_wake().present() &&
                close_to((*progress).animation_phase(), retained_phase),
            "resumed progress motion must retain phase before its next frame");
    (*progress).advance_animation(armed_wake(window));
    require((*progress).animation_phase() > retained_phase,
            "the first resumed frame must advance from the retained phase");
    const double pre_reduced_phase = (*progress).animation_phase();
    const FrameTime before_reduced = FrameClock::now();
    (*progress).motion_policy = MotionPolicy{true, false, true};
    const FrameTime after_reduced = FrameClock::now();
    (*progress).value = 73.0;
    require(window.next_wake().present() &&
                close_to((*progress).animation_phase(), pre_reduced_phase) &&
                close_to((*progress).value.read(), 73.0) &&
                has_semantic_state((*progress).semantic_descriptor().states,
                                   SemanticState::busy),
            "reduced motion must retain animation and state updates without resetting phase");
    const FrameTime reduced_wake = armed_wake(window);
    (*progress).advance_animation(reduced_wake);
    const double reduced_phase = (*progress).animation_phase();
    // Reduced motion scales the speed by 0.35 (and paces the timer at its
    // 100 ms floor): the first tick advances the phase 0.35 of the time
    // since the policy change, in periods of 1 second.
    const double reduced_step = reduced_phase - pre_reduced_phase;
    const double longest = std::chrono::duration<double>(reduced_wake - before_reduced).count();
    const double shortest = std::chrono::duration<double>(reduced_wake - after_reduced).count();
    require(reduced_phase > pre_reduced_phase && reduced_step <= 0.35 * longest + 1e-9 &&
                reduced_step >= 0.35 * shortest - 1e-9,
            "reduced progress motion must advance visibly at its calmer speed");
    (*progress).motion_policy = MotionPolicy{true, false, false};
    require(window.next_wake().present() &&
                close_to((*progress).animation_phase(), reduced_phase),
            "leaving reduced motion must retain the reduced timeline position");
    const std::uint64_t requests_before_batch =
        window.metrics_snapshot().scheduled_frame_requests;
    (*progress).motion_policy = MotionPolicy{true, true, true};
    require(window.next_wake().absent(),
            "batched pause plus reduced motion must quiesce without a transient lease");
    (*progress).motion_policy = MotionPolicy{true, false, false};
    require(window.next_wake().present() &&
                window.metrics_snapshot().scheduled_frame_requests ==
                    requests_before_batch + 1U,
            "batched live policy must publish exactly one replacement lease");

    const double phase_before_master_gate = (*progress).animation_phase();
    (*progress).motion_policy = MotionPolicy{false, false, false};
    require(window.next_wake().absent() &&
                (*progress).motion_policy.read() == MotionPolicy{false, false, false} &&
                close_to((*progress).animation_phase(), phase_before_master_gate) &&
                !has_semantic_state((*progress).semantic_descriptor().states,
                                    SemanticState::busy),
            "a disabled application motion policy must revoke its lease without mutating phase");
    (*progress).motion_policy = MotionPolicy{true, false, false};
    require(window.next_wake().present() &&
                close_to((*progress).animation_phase(), phase_before_master_gate) &&
                has_semantic_state((*progress).semantic_descriptor().states,
                                   SemanticState::busy),
            "re-enabling the application motion policy must resume from retained phase");

    for (std::size_t cycle = 0U; cycle < 16U; ++cycle) {
        (*progress).motion_policy = MotionPolicy{true, true, true};
        const double compound_phase = (*progress).animation_phase();
        require(window.next_wake().absent() && (*progress).motion_policy.read().paused &&
                    (*progress).motion_policy.read().reduced &&
                    !has_semantic_state((*progress).semantic_descriptor().states,
                                        SemanticState::busy),
                "compound pause plus reduced motion must remain a single quiescent policy");
        (*progress).motion_policy = MotionPolicy{true, false, true};
        require(window.next_wake().present() &&
                    close_to((*progress).animation_phase(), compound_phase) &&
                    has_semantic_state((*progress).semantic_descriptor().states,
                                       SemanticState::busy),
                "resuming while reduced must arm one calm animation lease");
        (*progress).motion_policy = MotionPolicy{true, true, false};
        require(window.next_wake().absent() &&
                    close_to((*progress).animation_phase(), compound_phase),
                "leaving reduced motion while paused must retain the suspended phase");
        (*progress).motion_policy = MotionPolicy{true, false, false};
        require(window.next_wake().present() &&
                    close_to((*progress).animation_phase(), compound_phase),
                "full motion must restore its frame lease from the retained phase");
    }
    PresentationSettings calm = window.presentation_settings();
    calm.reduced_motion = true;
    (*progress).motion_policy = MotionPolicy{true, false, false};
    window.set_presentation_settings(calm);
    require(!(*progress).motion_policy.read().reduced &&
                (*progress).effective_motion_policy().reduced &&
                window.next_wake().present(),
            "window reduced-motion preference must calm progress without overwriting local policy or stopping it");
    calm.reduced_motion = false;
    window.set_presentation_settings(calm);
    require(!(*progress).effective_motion_policy().reduced &&
                window.next_wake().present(),
            "clearing the window preference must restore full motion in one transition");
    (*progress).visual_style = ProgressBarVisualStyle::continuous;
    require(window.next_wake().absent(),
            "removing the last animated progress feature must revoke its frame source");
}

void test_progress_stripes_are_clipped_and_configurable() {
    std::shared_ptr<gui_forms::ProgressBar> progress = make_control<ProgressBar>(StableId("animation.stripes"));
    (*progress).set_requested_bounds({0.0, 0.0, 200.0, 24.0});
    (*progress).value = 50.0;
    (*progress).overlay_style = ProgressBarOverlayStyle::moving_stripes;
    (*progress).stripe_width = 6.0;
    Window window(progress, {200.0, 24.0});
    window.perform_layout();
    StripePainter painter;
    (*progress).on_paint(painter, {0.0, 0.0, 200.0, 24.0});
    // The barber pole (owner rulings of 2026-11-11): filled bands inside
    // the fill's one-unit rim, the pitch the interior's thickness (20), the
    // pale band 0.4 of it, leaning 45 degrees back from the leading edge
    // across the clip's 18.
    require(painter.saves >= 1 && painter.restores == painter.saves && painter.bands > 1 &&
                painter.clip == Rect{3.0, 3.0, 96.0, 18.0} && close_to(painter.band_width, 8.0) &&
                close_to(painter.slant, 18.0) && painter.leans_back,
            "striped progress must clip filled bands, leaning back, to the fill inside its rim");

    bool narrow_rejected = false;
    try {
        (*progress).stripe_width = 1.0;
    } catch (const std::invalid_argument&) {
        narrow_rejected = true;
    }
    require(narrow_rejected && close_to((*progress).stripe_width.read(), 6.0),
            "stripe geometry must reject unusable widths without mutation");
}

class ProgressEffectsPainter final : public Painter {
public:
    void save() override { ++saves; }
    void restore() override { ++restores; }
    void translate(Point) override {}
    void clip_rect(Rect bounds) override { clips.push_back(bounds); }
    void fill_rect(Rect, Color) override {}
    void stroke_rect(Rect, Color, double) override {}
    void fill_linear_gradient(Rect rect, Point, Point,
                              std::span<const GradientStop> stops) override {
        ++linear_gradients;
        if (stops.size() == 5U) ++five_stop_linear_gradients;
        if (stops.size() == 4U) ++four_stop_linear_gradients;
        last_linear = rect;
        largest_stop_count = std::max(largest_stop_count, stops.size());
    }
    void fill_linear_gradient_spread(
        Rect, Point start, Point end, std::span<const GradientStop> stops,
        GradientSpreadMode spread) override {
        ++spread_gradients;
        if (stops.size() == 5U) ++five_stop_spread_gradients;
        spread_mode = spread;
        spread_axis = {end.x - start.x, end.y - start.y};
        largest_stop_count = std::max(largest_stop_count, stops.size());
    }
    void fill_radial_gradient(Rect, Point, Size,
                              std::span<const GradientStop> stops) override {
        ++radial_gradients;
        largest_stop_count = std::max(largest_stop_count, stops.size());
    }
    void draw_line(Point from, Point to, Color, double width) override {
        if (width <= 1.5) {
            ++sparks;
            maximum_spark_length = std::max(
                maximum_spark_length,
                std::hypot(to.x - from.x, to.y - from.y));
        }
        else ++bands;
    }
    void fill_polygon(std::span<const Point>, Color, PolygonFillRule) override { ++bands; }
    void draw_text_utf8(Point, std::string_view, FontSpec, Color) override {}
    void draw_image(ImageId, Rect, double) override {}

    int saves{};
    int restores{};
    int linear_gradients{};
    int spread_gradients{};
    int radial_gradients{};
    int five_stop_linear_gradients{};
    int four_stop_linear_gradients{};
    int five_stop_spread_gradients{};
    int sparks{};
    int bands{};
    double maximum_spark_length{};
    std::size_t largest_stop_count{};
    GradientSpreadMode spread_mode{GradientSpreadMode::pad};
    Point spread_axis{};
    Rect last_linear{};
    std::vector<Rect> clips;
};

void test_progress_animation_style_family() {
    std::shared_ptr<gui_forms::ProgressBar> progress = make_control<ProgressBar>(
        StableId("animation.progress-style-family"));
    (*progress).set_requested_bounds({0.0, 0.0, 240.0, 24.0});
    (*progress).value = 64.0;
    (*progress).animation_period = 1000ms;
    Window window(progress, {240.0, 24.0});
    window.perform_layout();

    ProgressBarAnimationAppearance appearance =
        (*progress).animation_appearance.read();
    appearance.pulse_extent = 0.4;
    appearance.laser_edge_extent = 13.0;
    appearance.laser_phase_pitch = 12.0;
    (*progress).animation_appearance = appearance;
    require((*progress).animation_appearance.read() == appearance,
            "progress animation appearance must replace atomically");

    (*progress).visual_style = ProgressBarVisualStyle::luminance_pulse;
    const FrameTime origin = FrameClock::now();
    (*progress).advance_animation(origin);
    (*progress).advance_animation(origin + 250ms);
    ProgressEffectsPainter pulse;
    (*progress).on_paint(pulse, {0.0, 0.0, 240.0, 24.0});
    require(window.next_wake().present() &&
                pulse.five_stop_linear_gradients == 1 &&
                pulse.five_stop_spread_gradients == 0 &&
                std::find(pulse.clips.begin(), pulse.clips.end(),
                          Rect{2.0, 2.0, 151.04, 20.0}) != pulse.clips.end() &&
                pulse.largest_stop_count == 5U &&
                pulse.last_linear.width >= 8.0,
            "luminance progress must own one clipped, forward-moving soft pulse");

    (*progress).visual_style = ProgressBarVisualStyle::marching_stripes;
    (*progress).advance_animation(origin + 500ms);
    ProgressEffectsPainter stripes;
    (*progress).on_paint(stripes, {0.0, 0.0, 240.0, 24.0});
    require(stripes.bands > 2 && stripes.saves >= 1 &&
                stripes.restores == stripes.saves &&
                std::find(stripes.clips.begin(), stripes.clips.end(),
                          Rect{3.0, 3.0, 149.04, 18.0}) != stripes.clips.end(),
            "marching-stripe progress must remain clipped to determinate fill inside its rim");

    (*progress).visual_style = ProgressBarVisualStyle::laser_etch;
    (*progress).advance_animation(origin + 750ms);
    ProgressEffectsPainter laser;
    (*progress).on_paint(laser, {0.0, 0.0, 240.0, 24.0});
    require(laser.five_stop_spread_gradients == 1 &&
                laser.four_stop_linear_gradients >= 1 &&
                laser.spread_mode == GradientSpreadMode::repeat &&
                close_to(laser.spread_axis.x, 0.0) &&
                close_to(laser.spread_axis.y, appearance.laser_phase_pitch) &&
                laser.radial_gradients == 5 && laser.sparks >= 7 &&
                laser.maximum_spark_length < 5.0 &&
                std::find(laser.clips.begin(), laser.clips.end(),
                          Rect{2.0, 2.0, 151.04, 20.0}) != laser.clips.end(),
            "laser progress must combine a repeating phase with a compact white-hot corona and short sparks");

    (*progress).motion_policy = MotionPolicy{true, false, true};
    ProgressEffectsPainter reduced_laser;
    (*progress).on_paint(reduced_laser, {0.0, 0.0, 240.0, 24.0});
    require(reduced_laser.five_stop_spread_gradients == 1 &&
                reduced_laser.radial_gradients == 3 &&
                reduced_laser.sparks + 4 == laser.sparks &&
                reduced_laser.maximum_spark_length < 3.0,
            "reduced laser motion must preserve meaning with a calmer spark field");

    const ProgressBarAnimationAppearance retained =
        (*progress).animation_appearance.read();
    bool invalid_rejected = false;
    try {
        ProgressBarAnimationAppearance invalid = retained;
        invalid.laser_phase_pitch = 0.0;
        (*progress).animation_appearance = invalid;
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    require(invalid_rejected && (*progress).animation_appearance.read() == retained,
            "invalid progress effect geometry must not partially mutate appearance");

    (*progress).visual_style = ProgressBarVisualStyle::continuous;
    require(window.next_wake().absent(),
            "returning to a static progress style must revoke the frame lease");
}

void test_visual_style_round_trips() {
    std::shared_ptr<gui_forms::Button> button = make_control<Button>(StableId("animation.button"), "Button");
    std::shared_ptr<gui_forms::CheckBox> check = make_control<CheckBox>(StableId("animation.check"), "Check");
    std::shared_ptr<gui_forms::RadioButton> radio = make_control<RadioButton>(StableId("animation.radio"), "Radio");
    std::shared_ptr<gui_forms::TrackBar> slider = make_control<TrackBar>(StableId("animation.slider"));
    (*button).visual_style = ButtonVisualStyle::accent;
    (*check).indicator_style = ChoiceIndicatorStyle::toggle;
    (*radio).indicator_style = ChoiceIndicatorStyle::modern;
    (*slider).visual_style = TrackBarVisualStyle::filled;
    require((*button).visual_style.read() == ButtonVisualStyle::accent &&
                (*check).indicator_style.read() == ChoiceIndicatorStyle::toggle &&
                (*radio).indicator_style.read() == ChoiceIndicatorStyle::modern &&
                (*slider).visual_style.read() == TrackBarVisualStyle::filled,
            "public control visual variants must retain exact state");
}

void test_public_easing_preview_owns_scheduler_policy_and_semantics() {
    std::shared_ptr<gui_forms::EasingPreview> preview = make_control<EasingPreview>(
        StableId("animation.public.preview"));
    (*preview).set_requested_bounds({0.0, 0.0, 500.0, 340.0});
    (*preview).title = "Public easing proof";
    (*preview).marker_size = 18.0;
    Window window(preview, {500.0, 340.0});
    require((*preview).tracks().size() == 8U && (*preview).marker_size.read() == 18.0 &&
                window.next_wake().present() &&
                has_semantic_state((*preview).semantic_descriptor().states,
                                   SemanticState::busy),
            "EasingPreview must own all public curves, one frame lease, and busy semantics");

    PresentationSettings calm = window.presentation_settings();
    calm.reduced_motion = true;
    window.set_presentation_settings(calm);
    require((*preview).effective_motion_policy().reduced &&
                !(*preview).motion_policy.read().reduced && window.next_wake().present(),
            "window reduced-motion preference must calm EasingPreview without replacing its authored policy");
    calm.reduced_motion = false;
    window.set_presentation_settings(calm);

    const double before = (*preview).phase();
    (*preview).motion_policy = {true, false, true};
    require(window.next_wake().present() && (*preview).phase() == before,
            "reduced EasingPreview motion must retain phase and remain scheduled");
    (*preview).advance_animation(FrameClock::now() + 300ms);
    require((*preview).phase() > before &&
                (*preview).semantic_descriptor().numeric_value.present(),
            "public EasingPreview frames must advance reusable timeline state");

    (*preview).motion_policy = {true, true, true};
    require(window.next_wake().absent() &&
                !has_semantic_state((*preview).semantic_descriptor().states,
                                    SemanticState::busy),
            "paused EasingPreview motion must revoke its lease and busy state");
    (*preview).set_tracks({
        {EasingCurve::linear, "Linear only", Color::rgba(20, 80, 140)},
    });
    require((*preview).tracks().size() == 1U &&
                (*preview).tracks().front().label == "Linear only",
            "EasingPreview track collection must be reusable application configuration");
    bool empty_tracks_rejected = false;
    try {
        (*preview).set_tracks({});
    } catch (const std::invalid_argument&) {
        empty_tracks_rejected = true;
    }
    require(empty_tracks_rejected,
            "EasingPreview must reject an empty conformance surface");
}

} // namespace

// The display timing record and its grid (design note frame-source,
// section 2): presentations at phase + k * period, the next one strictly
// after a time, before the phase as well as after it.
void test_display_timing_grid() {
    const FrameTime phase{3ms};
    require(next_on_grid(phase, 8ms, FrameTime{3ms}) == FrameTime{11ms} &&
                next_on_grid(phase, 8ms, FrameTime{4ms}) == FrameTime{11ms} &&
                next_on_grid(phase, 8ms, FrameTime{10ms}) == FrameTime{11ms} &&
                next_on_grid(FrameTime{40ms}, 8ms, FrameTime{17ms}) == FrameTime{24ms} &&
                next_on_grid(FrameTime{40ms}, 8ms, FrameTime{16ms}) == FrameTime{24ms},
            "the next grid point is strictly after the time, on either side of the phase");
    const DisplayTiming assumed = DisplayTiming::assumed();
    require(!assumed.paced() && assumed.valid() && assumed.refresh_hertz() == 0.0 &&
                assumed.refresh_period == assumed_refresh_period &&
                next_presentation(assumed, FrameTime{5ms}) == FrameTime{5ms},
            "assumed timing is not paced: a frame is shown at the time it is polled");
    const DisplayTiming fixed = DisplayTiming::fixed_rate(8ms, phase);
    require(fixed.paced() && fixed.valid() && close_to(fixed.refresh_hertz(), 125.0, 1e-9) &&
                next_presentation(fixed, FrameTime{12ms}) == FrameTime{19ms},
            "a fixed rate shows a frame at the next point of its grid");
    const DisplayTiming reported = DisplayTiming::reported(DisplayTimingSource::display, 16ms, FrameTime{50ms});
    require(reported.source == DisplayTimingSource::display && reported.presentation_phase == FrameTime{50ms} &&
                next_presentation(reported, FrameTime{20ms}) == FrameTime{34ms},
            "a host's report phases the grid at its presentation, which may lie ahead");
    require(!DisplayTiming::fixed_rate(FrameInterval::zero()).valid() &&
                !DisplayTiming::fixed_rate(500us).valid() && !DisplayTiming::fixed_rate(2s).valid(),
            "a period outside 1 ms to 1 s is not valid");
    require(std::string_view(display_timing_source_name(DisplayTimingSource::assumed)) == "assumed" &&
                std::string_view(display_timing_source_name(DisplayTimingSource::display)) == "display" &&
                std::string_view(display_timing_source_name(DisplayTimingSource::measured)) == "measured" &&
                std::string_view(display_timing_source_name(DisplayTimingSource::nominal)) == "nominal" &&
                std::string_view(display_timing_source_name(DisplayTimingSource::fixed)) == "fixed",
            "every source has its diagnostic name");
}

// Display timing from refresh-counting completion stamps (X11 Present's ust
// and msc): the period over the refreshes since the anchor, phased at the
// latest stamp; nothing for the first stamp or one that does not advance;
// the anchor moving on keeps the period.
void test_refresh_counter_timing() {
    RefreshCounterTiming timing;
    require(timing.take(FrameTime{1000ms}, 100U).absent(), "the first stamp gives no timing");
    const stxlib::Optional<DisplayTiming> second = timing.take(FrameTime{1008ms}, 101U);
    require(second.present() && (*second.value()).source == DisplayTimingSource::display &&
                (*second.value()).refresh_period == 8ms && (*second.value()).presentation_phase == FrameTime{1008ms},
            "two stamps give the period between them, phased at the latest");
    const stxlib::Optional<DisplayTiming> skipped = timing.take(FrameTime{1024ms}, 103U);
    require(skipped.present() && (*skipped.value()).refresh_period == 8ms,
            "a stamp two refreshes on divides by the refreshes, not the stamps");
    require(timing.take(FrameTime{1030ms}, 103U).absent(), "a count that does not advance starts again");
    require(timing.take(FrameTime{1038ms}, 104U).present(), "and the next stamp gives timing again");
    require(timing.take(FrameTime{1000ms}, 105U).absent(), "a time that runs back starts again");
    FrameInterval worst{};
    for (std::uint64_t refresh = 1U; refresh <= 600U; ++refresh) {
        const FrameTime shown = FrameTime{1000ms} + std::chrono::microseconds(8333) * refresh;
        const stxlib::Optional<DisplayTiming> taken = timing.take(shown, 105U + refresh);
        require(taken.present(), "every advancing stamp gives timing");
        const FrameInterval period = (*taken.value()).refresh_period;
        const FrameInterval error = period > 8333us ? period - 8333us : 8333us - period;
        worst = std::max(worst, error);
    }
    require(worst == FrameInterval::zero(), "the anchor moving on every 240 refreshes keeps the exact period");
    timing.reset();
    require(timing.take(FrameTime{5000ms}, 9U).absent(), "after reset the first stamp gives no timing");
}

// Display timing measured from presentation completions alone: nothing
// before eight agreeing intervals, the median period phased at the latest
// completion, skipped refreshes counted as whole refreshes, a stall off the
// grid dropped, a doubled stamp ignored, a rest kept, and a clock running
// back starting again.
void test_display_timing_estimator() {
    DisplayTimingEstimator estimator;
    const FrameInterval period = 16667us;
    FrameTime completed{2000ms};
    require(estimator.take(completed).absent(), "the first completion gives no timing");
    for (int interval = 1; interval < 8; ++interval) {
        // Jitter of a few hundred microseconds either way.
        const FrameInterval jitter = interval % 2 == 0 ? 300us : -250us;
        completed = completed + period + jitter;
        require(estimator.take(completed).absent(), "fewer than eight intervals give no timing");
        completed = completed - jitter;
    }
    completed = completed + period;
    const stxlib::Optional<DisplayTiming> first = estimator.take(completed);
    require(first.present() && (*first.value()).source == DisplayTimingSource::measured &&
                (*first.value()).presentation_phase == completed,
            "eight agreeing intervals give measured timing phased at the latest completion");
    const FrameInterval measured = (*first.value()).refresh_period;
    require(measured >= period - 300us && measured <= period + 300us, "the median is the refresh period");

    // Three refreshes the window drew nothing in count as three.
    completed = completed + period * 3;
    const stxlib::Optional<DisplayTiming> skipped = estimator.take(completed);
    require(skipped.present() && (*skipped.value()).refresh_period >= period - 300us &&
                (*skipped.value()).refresh_period <= period + 300us,
            "a span of whole refreshes is divided into them");
    const std::size_t held = estimator.interval_count();
    // Two and a half refreshes is not on the grid: dropped.
    completed = completed + period * 5 / 2;
    require(estimator.take(completed).absent() && estimator.interval_count() == held,
            "a stall off the grid is dropped");
    // A second stamp for one refresh is ignored.
    require(estimator.take(completed + 200us).absent() && estimator.interval_count() == held,
            "a completion within a millisecond of the last is ignored");
    completed = completed + period;
    require(estimator.take(completed).present() && estimator.interval_count() == held + 1U,
            "the next refresh after a dropped stall measures again");
    // A rest of two seconds keeps the intervals and restarts from the stamp.
    completed = completed + 2s;
    require(estimator.take(completed).absent() && estimator.interval_count() == held + 1U,
            "a completion after a rest starts the next interval");
    completed = completed + period;
    require(estimator.take(completed).present(), "and the one after it reports at once");
    // Sixteen intervals at a new rate move the median over.
    const FrameInterval faster = 8333us;
    stxlib::Optional<DisplayTiming> latest;
    for (int interval = 0; interval < 16; ++interval) {
        completed = completed + faster;
        latest = estimator.take(completed);
    }
    require(latest.present() && (*latest.value()).refresh_period == faster,
            "a new rate replaces the old one once it fills the intervals");
    require(estimator.take(completed - 1s).absent() && estimator.interval_count() == 0U,
            "a clock running back forgets every interval");
    estimator.reset();
    require(estimator.take(FrameTime{9s}).absent() && estimator.interval_count() == 0U,
            "after reset the first completion gives no timing");
}

// A window measuring its timing from noted completions: measured timing
// replaces assumed timing, but never a fixed or display timing, and a reset
// forgets the completions.
void test_window_measures_presentation_completions() {
    std::shared_ptr<TimerProbe> probe = make_control<TimerProbe>(StableId("animation.timer.measured"));
    (*probe).set_requested_bounds({0.0, 0.0, 40.0, 20.0});
    Window window(probe, {40.0, 20.0});
    FrameTime completed{1s};
    bool set = false;
    for (int stamp = 0; stamp < 9; ++stamp) {
        set = window.note_presentation_completed(completed);
        completed = completed + 10ms;
    }
    require(set && window.display_timing().source == DisplayTimingSource::measured &&
                window.display_timing().refresh_period == 10ms,
            "nine completions ten milliseconds apart set measured timing on an assumed window");
    require(window.set_display_timing(DisplayTiming::fixed_rate(8ms)), "a fixed rate is accepted");
    require(!window.note_presentation_completed(completed) &&
                window.display_timing().source == DisplayTimingSource::fixed,
            "a measurement never overrides a fixed timing");
    require(window.set_display_timing(DisplayTiming::reported(DisplayTimingSource::nominal, 16ms, FrameTime{})),
            "a nominal rate is accepted");
    completed = completed + 10ms;
    require(window.note_presentation_completed(completed) &&
                window.display_timing().source == DisplayTimingSource::measured,
            "a measurement replaces a nominal rate, which has no phase");
    window.reset_presentation_estimate();
    completed = completed + 10ms;
    require(!window.note_presentation_completed(completed) &&
                window.display_timing().source == DisplayTimingSource::measured,
            "after a reset the completions start again and the held timing stays");
}

// A host reporting a live surface shown passes the window's display timing
// to the surface, so its producer paces to the window's frame source.
void test_live_surface_reports_carry_the_window_timing() {
    std::shared_ptr<TimerProbe> probe = make_control<TimerProbe>(StableId("animation.timer.live"));
    (*probe).set_requested_bounds({0.0, 0.0, 40.0, 20.0});
    Window window(probe, {40.0, 20.0});
    std::shared_ptr<LiveSurface> surface = LiveSurface::create({8U, 4U});
    window.report_live_surface_shown(*surface, 0U);
    require((*surface).presentation_record().timing.source == LiveTimingSource::none &&
                (*surface).presentation_record().presents == 1U,
            "an assumed window reports a presentation without timing");
    require(window.set_display_timing(DisplayTiming::fixed_rate(8ms, FrameTime{2ms})), "a fixed rate is accepted");
    window.report_live_surface_shown(*surface, 0U);
    const LivePresentationRecord record = (*surface).presentation_record();
    require(record.timing.source == LiveTimingSource::phased_timer && record.timing.refresh_period == 8ms &&
                (record.timing.last_vblank.time_since_epoch() - 2ms) % 8ms == FrameInterval{} &&
                record.timing.last_vblank <= FrameClock::now() && record.presents == 2U,
            "a paced window reports its period and a refresh on its grid");
}

// AnimationTimer on a window that knows its display's timing (design note
// frame-source, section 5): periods in whole refreshes on the display's
// grid, a shorter interval at one refresh, every refresh for
// start_every_frame, ticks at the frame's presentation time, a rate change
// re-deriving the period, and wants_display_frames only while a timer
// ticks.
void test_animation_timer_paces_to_the_display() {
    std::shared_ptr<TimerProbe> probe = make_control<TimerProbe>(StableId("animation.timer.paced"));
    (*probe).set_requested_bounds({0.0, 0.0, 40.0, 20.0});
    Window window(probe, {40.0, 20.0});
    StripePainter painter;
    require(!window.display_timing().paced() && !window.wants_display_frames(),
            "a window starts with assumed timing and wants no display frames");
    require(!window.set_display_timing(DisplayTiming::fixed_rate(500us)) && !window.display_timing().paced(),
            "an invalid timing is rejected and changes nothing");
    const FrameInterval phase = 3ms;
    require(window.set_display_timing(DisplayTiming::fixed_rate(8ms, FrameTime{phase})) &&
                window.display_timing().source == DisplayTimingSource::fixed &&
                window.display_timing().refresh_period == 8ms,
            "a fixed rate is accepted");

    (*probe).timer.start(*probe, 20ms, TimerProbe::Tick{probe.get()});
    require((armed_wake(window).time_since_epoch() - phase) % 24ms == FrameInterval{} &&
                (*probe).timer.interval() == 20ms && window.wants_display_frames(),
            "a 20 ms timer ticks at three 8 ms refreshes, on the display's grid");
    (*probe).timer.start(*probe, 2ms, TimerProbe::Tick{probe.get()});
    const FrameTime short_first = armed_wake(window);
    require((short_first.time_since_epoch() - phase) % 8ms == FrameInterval{},
            "an interval shorter than a refresh ticks at one refresh once the timing is known");
    static_cast<void>(window.poll_frame_schedule(short_first));
    require(armed_wake(window) == short_first + 8ms, "the shorter interval's cadence is one refresh");

    bool rejected = false;
    try {
        (*probe).timer.start_every_frame(*probe, AnimationTimer::Tick{});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "start_every_frame rejects an empty tick target");

    (*probe).damaging = false;
    (*probe).timer.start_every_frame(*probe, TimerProbe::Tick{probe.get()});
    const FrameTime first = armed_wake(window);
    const int before = (*probe).ticks;
    const FramePollResult polled = window.poll_frame_schedule(first);
    require((*probe).timer.interval() == FrameInterval::zero() && (*probe).ticks == before + 1 &&
                polled.presentation_time == first + 8ms && (*probe).last_tick == first + 8ms &&
                armed_wake(window) == first + 8ms,
            "every frame ticks once a refresh, at the time its frame is shown");
    const FramePollResult late = window.poll_frame_schedule(first + 8ms + 3ms);
    require(late.presentation_time == first + 16ms && (*probe).last_tick == first + 16ms &&
                armed_wake(window) == first + 16ms,
            "a late poll's frame is shown at the refresh after it");

    // The display drops to 62.5 Hz: the period follows, on the new grid.
    require(window.set_display_timing(DisplayTiming::fixed_rate(16ms, FrameTime{phase})),
            "a new rate is accepted");
    const FrameTime slowed = armed_wake(window);
    require((slowed.time_since_epoch() - phase) % 16ms == FrameInterval{},
            "a rate change moves the timer onto the new grid");
    static_cast<void>(window.poll_frame_schedule(slowed));
    require(armed_wake(window) == slowed + 16ms && (*probe).last_tick == slowed + 16ms,
            "every frame follows the new rate");
    // A report restating the phase with a little jitter moves nothing.
    const FrameTime steady = armed_wake(window);
    require(window.set_display_timing(DisplayTiming::fixed_rate(16ms, FrameTime{phase + 100us})) &&
                armed_wake(window) == steady,
            "a restated phase within the jitter tolerance leaves the deadline");
    require(window.set_display_timing(DisplayTiming::fixed_rate(16ms + 2us, FrameTime{phase})) &&
                window.display_timing().refresh_period == 16ms && armed_wake(window) == steady,
            "a period within a thousandth of the held one keeps the held period");

    // Idle: a damaging tick left unpainted stops the timer, and the window
    // wants no display frames.
    (*probe).damaging = true;
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    static_cast<void>(window.poll_frame_schedule(armed_wake(window)));
    require(!(*probe).timer.ticking() && (*probe).timer.started() && !window.wants_display_frames(),
            "a timer waiting for its paint holds no display frames");
    (*probe).on_paint(painter, {0.0, 0.0, 40.0, 20.0});
    require((*probe).timer.ticking() && window.wants_display_frames(), "the paint asks for display frames again");
    window.set_occluded(true, FrameClock::now());
    require(!window.wants_display_frames(), "an occluded window wants no display frames");
    window.set_occluded(false, FrameClock::now());
    (*probe).timer.stop();
    require(!window.wants_display_frames(), "a stopped timer wants no display frames");

    // Back to assumed timing: ticks at the poll's time again.
    require(window.set_display_timing(DisplayTiming::assumed()) && !window.display_timing().paced(),
            "assumed timing can be restored");
    (*probe).damaging = false;
    (*probe).timer.start(*probe, 32ms, TimerProbe::Tick{probe.get()});
    const FrameTime plain = armed_wake(window);
    const FramePollResult unpaced = window.poll_frame_schedule(plain);
    require(plain.time_since_epoch() % 32ms == FrameInterval{} && unpaced.presentation_time == plain &&
                (*probe).last_tick == plain,
            "with assumed timing a tick receives the poll's time on the clock's grid");
}

int main() {
    try {
        test_easing_endpoints_and_curves();
        test_timeline_delay_repeat_direction_and_finish();
        test_timeline_pause_resume_retains_phase_without_catch_up();
        test_invalid_timeline_contract();
        test_reduced_motion_remains_active_and_calm();
        test_animation_timer_is_paint_gated();
        test_display_timing_grid();
        test_refresh_counter_timing();
        test_display_timing_estimator();
        test_window_measures_presentation_completions();
        test_live_surface_reports_carry_the_window_timing();
        test_animation_timer_paces_to_the_display();
        test_progress_animation_and_hidden_suspension();
        test_progress_animation_damages_what_moves();
        test_progress_stripes_are_clipped_and_configurable();
        test_progress_animation_style_family();
        test_visual_style_round_trips();
        test_public_easing_preview_owns_scheduler_policy_and_semantics();
        std::cout << "gui_forms_animation_tests: all tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_animation_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
