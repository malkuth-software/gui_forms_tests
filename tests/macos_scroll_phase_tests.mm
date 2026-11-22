#import <AppKit/AppKit.h>

#include "src/surface/macos/view/macos_gesture_phase.hpp"
#include "src/surface/macos/view/macos_scroll_phase.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// The macOS host's scroll gesture mapping (cp33): AppKit's contact and
// momentum phases, as a trackpad gesture reports them in order, become the
// portable WheelPhase the window latches by. The latching itself is tested
// headlessly (scrollable_control_tests,
// test_scroll_gesture_latches_and_does_not_chain). The touchpad gestures'
// phases and the magnify scale (macos_gesture_phase.hpp) are tested here
// too; their routing is gesture_event_tests.

namespace {

using gui_forms::WheelPhase;
using gui_forms::GesturePhase;
using gui_forms::host::macos::gesture_phase_for;
using gui_forms::host::macos::magnify_scale_for;
using gui_forms::host::macos::wheel_phase_for;

// One AppKit event's phases and the WheelPhase it must map to.
struct PhaseCase final {
    const char* name{};
    NSEventPhase phase{NSEventPhaseNone};
    NSEventPhase momentum{NSEventPhaseNone};
    WheelPhase expected{WheelPhase::none};
};

// One magnify or rotate event's phase and the GesturePhase it must map to.
struct GestureCase final {
    const char* name{};
    NSEventPhase phase{NSEventPhaseNone};
    GesturePhase expected{GesturePhase::changed};
};

// A magnification and the multiplied scale it must become.
struct ScaleCase final {
    const char* name{};
    double magnification{};
    double expected{1.0};
};

} // namespace

int main() {
    // A whole trackpad gesture in AppKit's order, then the other phases.
    const PhaseCase cases[] = {
        {"detented wheel", NSEventPhaseNone, NSEventPhaseNone, WheelPhase::none},
        {"fingers down", NSEventPhaseMayBegin, NSEventPhaseNone, WheelPhase::began},
        {"contact began", NSEventPhaseBegan, NSEventPhaseNone, WheelPhase::began},
        {"contact changed", NSEventPhaseChanged, NSEventPhaseNone, WheelPhase::changed},
        {"contact stationary", NSEventPhaseStationary, NSEventPhaseNone, WheelPhase::changed},
        {"contact ended", NSEventPhaseEnded, NSEventPhaseNone, WheelPhase::ended},
        {"momentum began", NSEventPhaseNone, NSEventPhaseBegan, WheelPhase::momentum},
        {"momentum changed", NSEventPhaseNone, NSEventPhaseChanged, WheelPhase::momentum},
        {"momentum ended", NSEventPhaseNone, NSEventPhaseEnded, WheelPhase::momentum_ended},
        {"momentum cancelled", NSEventPhaseNone, NSEventPhaseCancelled, WheelPhase::momentum_ended},
        {"contact cancelled", NSEventPhaseCancelled, NSEventPhaseNone, WheelPhase::cancelled},
    };
    int failures = 0;
    for (const PhaseCase& entry : cases) {
        const WheelPhase mapped = wheel_phase_for(entry.phase, entry.momentum);
        if (mapped != entry.expected) {
            std::printf("FAIL %s: mapped to %d, expected %d\n", entry.name, static_cast<int>(mapped),
                        static_cast<int>(entry.expected));
            ++failures;
        }
    }
    const GestureCase gestures[] = {
        {"pinch began", NSEventPhaseBegan, GesturePhase::began},
        {"pinch changed", NSEventPhaseChanged, GesturePhase::changed},
        {"pinch stationary", NSEventPhaseStationary, GesturePhase::changed},
        {"pinch ended", NSEventPhaseEnded, GesturePhase::ended},
        {"pinch cancelled", NSEventPhaseCancelled, GesturePhase::cancelled},
        {"no phase", NSEventPhaseNone, GesturePhase::changed},
    };
    for (const GestureCase& entry : gestures) {
        const GesturePhase mapped = gesture_phase_for(entry.phase);
        if (mapped != entry.expected) {
            std::printf("FAIL %s: mapped to %d, expected %d\n", entry.name, static_cast<int>(mapped),
                        static_cast<int>(entry.expected));
            ++failures;
        }
    }
    const ScaleCase scales[] = {
        {"no change", 0.0, 1.0},
        {"spread a tenth", 0.1, 1.1},
        {"pinch a quarter", -0.25, 0.75},
        {"impossible pinch", -1.0, 1.0},
        {"not finite", std::nan(""), 1.0},
    };
    for (const ScaleCase& entry : scales) {
        const double mapped = magnify_scale_for(entry.magnification);
        if (std::fabs(mapped - entry.expected) > 1e-12) {
            std::printf("FAIL %s: scale %f, expected %f\n", entry.name, mapped, entry.expected);
            ++failures;
        }
    }
    std::printf("gui_forms_macos_scroll_phase_tests: %d failures\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
