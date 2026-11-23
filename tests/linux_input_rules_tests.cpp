// The X11 host's computed input (owner's mouse-event rule, 2026-11-08):
// click counts from the desktop's double-click time and distance, and the
// keys that ask for a context menu, and the XI 2.4 pinch as a portable
// GestureEvent. No Xlib, so tested on every platform.

#include "src/host/linux/application/linux_input_rules.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace gui_forms;
using namespace gui_forms::host::linux_detail;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_click_counts() {
    LinuxClickHistory history{};
    const std::uint32_t interval = 400U;
    const double distance = default_double_click_distance;
    require(next_click_count(history, 1U, 1000U, {10.0, 10.0}, interval, distance) == 1U, "a first press is one");
    require(next_click_count(history, 1U, 1200U, {12.0, 8.0}, interval, distance) == 2U,
            "a press within the time and distance is the second");
    require(next_click_count(history, 1U, 1300U, {12.0, 8.0}, interval, distance) == 3U, "and the third");
    require(next_click_count(history, 1U, 1400U, {12.0, 8.0}, interval, distance) == 1U, "then one again");
    require(next_click_count(history, 1U, 1900U, {12.0, 8.0}, interval, distance) == 1U,
            "a press after the interval is one");
    require(next_click_count(history, 1U, 2000U, {12.0 + distance + 1.0, 8.0}, interval, distance) == 1U,
            "a press beyond the distance is one");
    require(next_click_count(history, 3U, 2100U, {12.0 + distance + 1.0, 8.0}, interval, distance) == 1U,
            "another button starts again");
    LinuxClickHistory wrapped{};
    static_cast<void>(next_click_count(wrapped, 1U, 0xFFFFFF00U, {0.0, 0.0}, interval, distance));
    require(next_click_count(wrapped, 1U, 0x00000010U, {0.0, 0.0}, interval, distance) == 2U,
            "a wrapped server time compares by its difference");
    LinuxClickHistory desktop{};
    static_cast<void>(next_click_count(desktop, 1U, 0U, {0.0, 0.0}, 250U, 5.0));
    require(next_click_count(desktop, 1U, 260U, {4.0, 4.0}, 250U, 5.0) == 1U,
            "the desktop's own interval is used");
}

void test_context_keys() {
    require(requests_keyboard_context(PhysicalKey::context_menu, Modifier::none), "the Menu key asks");
    require(requests_keyboard_context(PhysicalKey::f10, Modifier::shift), "Shift+F10 asks");
    require(!requests_keyboard_context(PhysicalKey::f10, Modifier::none), "F10 alone does not");
    require(!requests_keyboard_context(PhysicalKey::f10, Modifier::shift | Modifier::control),
            "Ctrl+Shift+F10 does not");
}

bool near(double value, double expected) { return std::fabs(value - expected) < 1e-9; }

void test_pinch_events() {
    LinuxPinchState state{};
    const Point at{100.0, 50.0};
    const GestureEvent began = next_pinch_event(state, LinuxPinchStage::begin, false, at, 1.0, 0.0, {0.0, 0.0},
                                                Modifier::none, 10U);
    require(began.kind == GestureKind::magnify && began.gesture_phase == GesturePhase::began, "begin is began");
    require(near(began.scale, 1.0) && began.position.x == 100.0 && began.timestamp_nanoseconds == 10U,
            "began carries no change, at the focal point");
    const GestureEvent grown = next_pinch_event(state, LinuxPinchStage::update, false, at, 1.5, 3.0, {2.0, -1.0},
                                                Modifier::control, 20U);
    require(grown.gesture_phase == GesturePhase::changed && near(grown.scale, 1.5), "the first update's ratio");
    require(near(grown.rotation_degrees, -3.0), "a clockwise turn is a negative rotation");
    require(grown.translation.x == 2.0 && grown.translation.y == -1.0 && grown.modifiers == Modifier::control,
            "travel and modifiers pass through");
    const GestureEvent again = next_pinch_event(state, LinuxPinchStage::update, false, at, 3.0, 0.0, {0.0, 0.0},
                                                Modifier::none, 30U);
    require(near(again.scale, 2.0), "each update is the ratio to the previous absolute scale");
    const GestureEvent bad = next_pinch_event(state, LinuxPinchStage::update, false, at, 0.0, std::nan(""),
                                              {0.0, 0.0}, Modifier::none, 40U);
    require(near(bad.scale, 1.0) && bad.rotation_degrees == 0.0 && valid_gesture_event(bad),
            "an unusable scale or angle changes nothing");
    const GestureEvent ended = next_pinch_event(state, LinuxPinchStage::end, false, at, 3.0, 0.0, {0.0, 0.0},
                                                Modifier::none, 50U);
    require(ended.gesture_phase == GesturePhase::ended && near(ended.scale, 1.0), "end is ended");
    static_cast<void>(next_pinch_event(state, LinuxPinchStage::begin, false, at, 1.0, 0.0, {0.0, 0.0},
                                       Modifier::none, 60U));
    const GestureEvent cancelled = next_pinch_event(state, LinuxPinchStage::end, true, at, 0.5, 0.0, {0.0, 0.0},
                                                    Modifier::none, 70U);
    require(cancelled.gesture_phase == GesturePhase::cancelled && near(cancelled.scale, 0.5),
            "a cancelled end is cancelled, and a new gesture starts at 1");
}

} // namespace

int main() {
    try {
        test_click_counts();
        test_context_keys();
        test_pinch_events();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "linux input rules tests passed\n";
    return EXIT_SUCCESS;
}
