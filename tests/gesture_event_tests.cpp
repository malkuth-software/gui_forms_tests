// Touchpad gestures (gesture_events.hpp): the event's shape and validity,
// the window's wheel-like routing (target, then bubble, until handled),
// gesture latching and capture, the session's refusal of invalid reports,
// and the headless host's simulated pinch. The platform hosts' own
// translations are tested where they are plain values
// (macos_scroll_phase_tests, linux_input_rules_tests,
// windows_manipulation_rules_tests); a real trackpad is a hand check.

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/gesture_events/gesture_events.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/window/window.hpp"
#include "headless_host.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace gui_forms;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double value, double expected) { return std::fabs(value - expected) < 1e-9; }

// Records each on_gesture into a shared trace ("name:phase;"), and handles
// the gesture when told to.
class GestureProbe final : public Control {
public:
    GestureProbe(StableId id, std::string name, std::string& trace)
        : Control(std::move(id)), name_(std::move(name)), trace_(&trace) {}

    void on_gesture(GestureEventArgs& arguments) override {
        (*trace_) += name_;
        (*trace_) += arguments.phase == EventPhase::target ? ":target" : ":bubble";
        (*trace_) += ';';
        last = arguments;
        ++count;
        Control::on_gesture(arguments);
        if (handles) arguments.handled = true;
    }

    GestureEventArgs last{};
    int count{};
    bool handles{};

private:
    std::string name_;
    std::string* trace_;
};

// A gesture subscriber: counts and keeps the scale it saw.
struct CountGestures final {
    int* count;
    double* scale;
    void operator()(const GestureEventArgs& arguments) const {
        ++(*count);
        (*scale) = arguments.scale;
    }
};

GestureEvent gesture(GestureKind kind, GesturePhase phase, Point position, double scale = 1.0) {
    GestureEvent event{};
    event.kind = kind;
    event.gesture_phase = phase;
    event.position = position;
    event.scale = scale;
    return event;
}

void test_shape() {
    const GestureEvent identity{};
    require(identity.kind == GestureKind::magnify && identity.gesture_phase == GesturePhase::changed,
            "a default gesture is a magnify change");
    require(identity.scale == 1.0 && identity.rotation_degrees == 0.0 && identity.translation.x == 0.0 &&
                identity.translation.y == 0.0 && !identity.handled && identity.phase == EventPhase::target,
            "every transform field defaults to its identity");
    GestureEvent event = gesture(GestureKind::rotate, GesturePhase::began, {30.0, 40.0}, 1.0);
    event.rotation_degrees = 12.5;
    event.translation = {3.0, -4.0};
    event.modifiers = Modifier::shift;
    event.timestamp_nanoseconds = 77U;
    const GestureEventArgs arguments = gesture_event_arguments(event, {10.0, 15.0});
    require(arguments.kind == GestureKind::rotate && arguments.gesture_phase == GesturePhase::began,
            "the arguments keep the kind and phase");
    require(arguments.location.x == 20.0 && arguments.location.y == 25.0 && arguments.window_position.x == 30.0,
            "location is control-relative, window_position the window's");
    require(arguments.rotation_degrees == 12.5 && arguments.translation.y == -4.0 &&
                arguments.modifiers == Modifier::shift && arguments.timestamp_nanoseconds == 77U,
            "the transform, modifiers and time pass through");
    require(valid_gesture_event(event), "a finite gesture is valid");
    GestureEvent bad = event;
    bad.scale = 0.0;
    require(!valid_gesture_event(bad), "a zero scale is not");
    bad = event;
    bad.position.x = std::numeric_limits<double>::quiet_NaN();
    require(!valid_gesture_event(bad), "a NaN focal point is not");
    bad = event;
    bad.translation.y = std::numeric_limits<double>::infinity();
    require(!valid_gesture_event(bad), "an infinite translation is not");
    bad = event;
    bad.kind = static_cast<GestureKind>(9);
    require(!valid_gesture_event(bad), "an unknown kind is not");
    bad = event;
    bad.gesture_phase = static_cast<GesturePhase>(9);
    require(!valid_gesture_event(bad), "an unknown phase is not");
    require(gesture_phase_final(GesturePhase::cancelled) && gesture_phase_final(GesturePhase::momentum_ended) &&
                !gesture_phase_final(GesturePhase::ended) && !gesture_phase_final(GesturePhase::changed),
            "cancelled and momentum_ended end a gesture; ended may be followed by momentum");
}

// A parent panel holding two probes side by side.
struct Scene final {
    std::string trace;
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("gesture.root"));
    std::shared_ptr<GestureProbe> parent;
    std::shared_ptr<GestureProbe> left;
    std::shared_ptr<GestureProbe> right;
    std::unique_ptr<Window> window;

    Scene() {
        parent = make_control<GestureProbe>(StableId("gesture.parent"), std::string("parent"), trace);
        (*parent).set_requested_bounds({0.0, 0.0, 200.0, 100.0});
        left = make_control<GestureProbe>(StableId("gesture.left"), std::string("left"), trace);
        (*left).set_requested_bounds({0.0, 0.0, 100.0, 100.0});
        right = make_control<GestureProbe>(StableId("gesture.right"), std::string("right"), trace);
        (*right).set_requested_bounds({100.0, 0.0, 100.0, 100.0});
        (*parent).add_child(left);
        (*parent).add_child(right);
        (*root).add_child(parent);
        window = std::make_unique<Window>(root, Size{200.0, 100.0});
        (*window).perform_layout();
    }
};

void test_dispatch_order() {
    Scene scene;
    Window& window = *scene.window;
    int subscribed = 0;
    double subscribed_scale = 0.0;
    SubscriptionToken token = (*scene.left).gesture().subscribe(CountGestures{&subscribed, &subscribed_scale});
    const bool handled = window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::began, {50.0, 50.0}, 1.5));
    require(!handled, "a gesture nobody handles reports unhandled");
    require(scene.trace == "left:target;parent:bubble;", "the target runs first, then its ancestors bubble");
    require(subscribed == 1 && near(subscribed_scale, 1.5), "the gesture event's subscribers run from on_gesture");
    require(near((*scene.left).last.location.x, 50.0) && near((*scene.left).last.scale, 1.5),
            "the target sees its own location and the scale");
    scene.trace.clear();
    (*scene.left).handles = true;
    require(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::changed, {50.0, 50.0}, 1.1)),
            "a handled gesture reports handled");
    require(scene.trace == "left:target;", "handled ends the route at the target");
    token.disconnect();
}

void test_latching_and_capture() {
    Scene scene;
    Window& window = *scene.window;
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::began, {50.0, 50.0})));
    scene.trace.clear();
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::changed, {150.0, 50.0})));
    require(scene.trace == "left:target;parent:bubble;", "a gesture stays on the control it began over");
    scene.trace.clear();
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::smart_magnify, GesturePhase::ended, {150.0, 50.0})));
    require(scene.trace == "right:target;parent:bubble;", "a smart magnify goes to the control under it");
    scene.trace.clear();
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::cancelled, {150.0, 50.0})));
    require(scene.trace == "left:target;parent:bubble;", "the gesture's last event still goes to its control");
    scene.trace.clear();
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::changed, {150.0, 50.0})));
    require(scene.trace == "right:target;parent:bubble;", "after it, the latch is released");
    scene.trace.clear();
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::began, {150.0, 50.0})));
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::ended, {50.0, 50.0})));
    require(scene.trace == "right:target;parent:bubble;right:target;parent:bubble;",
            "a new gesture latches where it began");
    scene.trace.clear();
    (*scene.left).set_pointer_capture(true);
    static_cast<void>(window.dispatch_gesture(gesture(GestureKind::magnify, GesturePhase::began, {150.0, 50.0})));
    require(scene.trace == "left:target;parent:bubble;", "the captured control takes the gesture");
    (*scene.left).set_pointer_capture(false);
}

// A wheel subscriber that counts: a gesture must never become a wheel.
struct CountWheels final {
    int* count;
    void operator()(const MouseEventArgs&) const { ++(*count); }
};

void test_headless_simulation() {
    Scene scene;
    Window& window = *scene.window;
    host::HeadlessHost host(window);
    require(host.dispatch(HostAttachEvent{{200.0, 100.0}, 1.0}, 1000U).accepted(), "the host attaches");
    int wheels = 0;
    SubscriptionToken wheel_token = (*scene.left).mouse_wheel().subscribe(CountWheels{&wheels});
    const std::uint32_t handled = host.simulate_magnify({50.0, 50.0}, 4.0, 2U, 2000U, 1000U);
    require(handled == 0U, "nobody handles the simulated pinch yet");
    require(scene.trace == "left:target;parent:bubble;left:target;parent:bubble;left:target;parent:bubble;"
                           "left:target;parent:bubble;",
            "began, two changes and ended reach the control under the focal point");
    require((*scene.left).last.gesture_phase == GesturePhase::ended && near((*scene.left).last.scale, 1.0) &&
                (*scene.left).last.timestamp_nanoseconds == 5000U,
            "the simulation ends with ended at the last step's time");
    require(host.session().snapshot().gesture_events == 4U, "the session counts each gesture event");
    require(wheels == 0, "an unhandled pinch is not turned into wheel events");
    (*scene.left).handles = true;
    scene.trace.clear();
    require(host.simulate_magnify({50.0, 50.0}, 0.25, 2U, 6000U, 1000U) == 4U, "a handled pinch counts each event");
    require((*scene.left).count == 8, "every simulated event reached the probe");
    require(host.simulate_magnify({50.0, 50.0}, 0.0, 2U, 9000U, 1000U) == 0U &&
                host.simulate_magnify({50.0, 50.0}, 2.0, 0U, 9000U, 1000U) == 0U,
            "an unusable scale or no steps simulates nothing");
    const HostDispatchResult refused =
        host.dispatch(gesture(GestureKind::magnify, GesturePhase::changed, {50.0, 50.0}, -1.0), 10000U);
    require(refused.error == HostDispatchError::invalid_payload && (*scene.left).count == 8,
            "the session refuses an invalid gesture before the window sees it");
    wheel_token.disconnect();
}

// What the Windows host engages DirectManipulation by: a window's opt-in,
// or an attached control that asked; nothing else.
void test_gesture_input_request() {
    std::string trace;
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("request.root"));
    Window window(root, Size{100.0, 100.0});
    require(!window.gesture_input_requested(), "a window with no gesture handler asks for nothing");
    window.set_gesture_input(true);
    require(window.gesture_input_requested(), "the window's own opt-in asks");
    window.set_gesture_input(false);
    std::shared_ptr<GestureProbe> probe =
        make_control<GestureProbe>(StableId("request.probe"), std::string("probe"), trace);
    (*probe).set_gesture_input(true);
    require(!window.gesture_input_requested(), "a detached control does not ask");
    (*root).add_child(probe);
    require(window.gesture_input_requested(), "attached, it asks");
    (*probe).set_gesture_input(true);
    (*probe).set_gesture_input(false);
    require(!window.gesture_input_requested(), "and stops asking (a repeated set counts once)");
    (*probe).set_gesture_input(true);
    require(window.gesture_input_requested(), "asks again");
    static_cast<void>((*root).remove_child((*probe).runtime_id()));
    require(!window.gesture_input_requested() && (*probe).gesture_input(),
            "detached, its request leaves the window, the control keeps its own");
}

} // namespace

int main() {
    try {
        test_shape();
        test_dispatch_order();
        test_latching_and_capture();
        test_headless_simulation();
        test_gesture_input_request();
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_gesture_event_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "gui_forms_gesture_event_tests: all tests passed\n";
    return EXIT_SUCCESS;
}
