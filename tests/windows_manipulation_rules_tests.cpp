// The Windows host's precision-touchpad rules (windows_manipulation_rules.hpp):
// DirectManipulation's content transforms and viewport statuses as portable
// gesture and precise wheel events. No windows.h, so tested on every
// platform; the DirectManipulation glue itself is a hand check on a
// precision touchpad.

#include "src/host/windows/input/windows_manipulation_rules.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace gui_forms;
using namespace gui_forms::host::windows_detail;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double value, double expected) { return std::fabs(value - expected) < 1e-9; }

void test_pinch_about_the_focal_point() {
    ManipulationTracker tracker{};
    const Point focal{100.0, 50.0};
    const double pixels = 2.0;
    ManipulationReport report = manipulation_status(tracker, ManipulationStatus::running, focal, Modifier::none, 1U);
    require(!report.has_gesture && !report.has_wheel && tracker.running, "running starts a gesture, sends nothing");
    // A zoom by 2 about the focal point (device 200, 100): offset = (1 - 2) * focal.
    report = manipulation_transform(tracker, 2.0, Point{-200.0, -100.0}, focal, pixels, Modifier::control, 2U);
    require(report.has_gesture && !report.has_wheel, "a scale change is a pinch");
    require(report.gesture.kind == GestureKind::magnify && report.gesture.gesture_phase == GesturePhase::began,
            "the first pinch report is began");
    require(near(report.gesture.scale, 2.0), "the pinch's scale is the change");
    require(near(report.gesture.translation.x, 0.0) && near(report.gesture.translation.y, 0.0),
            "a zoom about the focal point has no travel");
    require(report.gesture.position.x == 100.0 && report.gesture.modifiers == Modifier::control,
            "the focal point and modifiers pass through");
    require(valid_gesture_event(report.gesture), "the pinch is a valid gesture");
    // Then a pure pan of 10 device pixels while pinching: a changed with travel.
    report = manipulation_transform(tracker, 2.0, Point{-190.0, -100.0}, focal, pixels, Modifier::none, 3U);
    require(report.has_gesture && report.gesture.gesture_phase == GesturePhase::changed, "the pinch continues");
    require(near(report.gesture.scale, 1.0) && near(report.gesture.translation.x, 5.0),
            "travel is logical units");
    report = manipulation_status(tracker, ManipulationStatus::inertia, focal, Modifier::none, 4U);
    require(report.has_gesture && report.gesture.gesture_phase == GesturePhase::ended, "fingers up end the pinch");
    report = manipulation_transform(tracker, 2.0, Point{-180.0, -100.0}, focal, pixels, Modifier::none, 5U);
    require(!report.has_gesture && !report.has_wheel, "nothing follows a pinch's end");
    report = manipulation_status(tracker, ManipulationStatus::ready, focal, Modifier::none, 6U);
    require(!report.has_gesture && !report.has_wheel && report.reset_content && !tracker.running,
            "ready closes nothing more and resets the content");
    report = manipulation_transform(tracker, 1.0, Point{}, focal, pixels, Modifier::none, 7U);
    require(!report.has_gesture && !report.has_wheel && near(tracker.scale, 1.0),
            "the reset's update only moves the baseline");
}

void test_pan_is_a_precise_wheel() {
    ManipulationTracker tracker{};
    const Point focal{10.0, 10.0};
    static_cast<void>(manipulation_status(tracker, ManipulationStatus::running, focal, Modifier::none, 1U));
    ManipulationReport report =
        manipulation_transform(tracker, 1.0, Point{0.0, 30.0}, focal, 1.5, Modifier::none, 2U);
    require(report.has_wheel && !report.has_gesture, "a pan is a wheel event");
    require(report.wheel.action == PointerAction::wheel && report.wheel.wheel_unit == WheelDeltaUnit::pixels,
            "in pixels");
    require(report.wheel.wheel_phase == WheelPhase::began && near(report.wheel.wheel_delta.y, 20.0),
            "began, content moving down scrolls up by logical pixels");
    report = manipulation_transform(tracker, 1.0, Point{-15.0, 30.0}, focal, 1.5, Modifier::none, 3U);
    require(report.wheel.wheel_phase == WheelPhase::changed && near(report.wheel.wheel_delta.x, -10.0),
            "then changed");
    report = manipulation_status(tracker, ManipulationStatus::inertia, focal, Modifier::none, 4U);
    require(report.has_wheel && report.wheel.wheel_phase == WheelPhase::ended, "fingers up end the contact");
    report = manipulation_transform(tracker, 1.0, Point{-15.0, 45.0}, focal, 1.5, Modifier::none, 5U);
    require(report.wheel.wheel_phase == WheelPhase::momentum, "inertia is momentum");
    report = manipulation_status(tracker, ManipulationStatus::ready, focal, Modifier::none, 6U);
    require(report.has_wheel && report.wheel.wheel_phase == WheelPhase::momentum_ended && report.reset_content,
            "ready ends the momentum");
}

void test_pan_turning_into_pinch() {
    ManipulationTracker tracker{};
    const Point focal{0.0, 0.0};
    static_cast<void>(manipulation_status(tracker, ManipulationStatus::running, focal, Modifier::none, 1U));
    static_cast<void>(manipulation_transform(tracker, 1.0, Point{0.0, 5.0}, focal, 1.0, Modifier::none, 2U));
    const ManipulationReport report =
        manipulation_transform(tracker, 1.25, Point{0.0, 5.0}, focal, 1.0, Modifier::none, 3U);
    require(report.has_wheel && report.wheel.wheel_phase == WheelPhase::ended, "the pan ends");
    require(report.has_gesture && report.gesture.gesture_phase == GesturePhase::began &&
                near(report.gesture.scale, 1.25),
            "and the pinch begins in the same report");
    const ManipulationReport cancelled =
        manipulation_status(tracker, ManipulationStatus::other, focal, Modifier::none, 4U);
    require(cancelled.has_gesture && cancelled.gesture.gesture_phase == GesturePhase::ended && !cancelled.has_wheel,
            "another status ends the pinch");
}

void test_unusable_reports() {
    ManipulationTracker tracker{};
    static_cast<void>(manipulation_status(tracker, ManipulationStatus::running, Point{}, Modifier::none, 1U));
    const ManipulationReport report =
        manipulation_transform(tracker, std::nan(""), Point{}, Point{}, 1.0, Modifier::none, 2U);
    require(!report.has_gesture && !report.has_wheel && near(tracker.scale, 1.0), "a NaN scale is dropped");
    const ManipulationReport idle = manipulation_status(tracker, ManipulationStatus::inertia, Point{}, Modifier::none, 3U);
    require(!idle.has_gesture && !idle.has_wheel, "inertia with nothing begun sends nothing");
}

void test_engagement() {
    require(direct_manipulation_allowed(nullptr), "unset, the window decides");
    require(!direct_manipulation_allowed("0"), "GUI_FORMS_DIRECT_MANIPULATION=0 turns it off");
    require(direct_manipulation_allowed("1") && direct_manipulation_allowed("") && direct_manipulation_allowed("00"),
            "any other value leaves it to the window");
    require(!direct_manipulation_wanted(true, false, false, false),
            "a window without a gesture handler keeps the plain wheel path");
    require(direct_manipulation_wanted(true, true, false, false), "a gesture handler engages it");
    require(!direct_manipulation_wanted(false, true, true, true), "the switch overrides every request");
    require(direct_manipulation_wanted(true, false, true, true), "a running gesture finishes after the request ends");
    require(!direct_manipulation_wanted(true, false, true, false), "then it is released");
}

} // namespace

int main() {
    try {
        test_pinch_about_the_focal_point();
        test_pan_is_a_precise_wheel();
        test_pan_turning_into_pinch();
        test_unusable_reports();
        test_engagement();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "windows manipulation rules tests passed\n";
    return EXIT_SUCCESS;
}
