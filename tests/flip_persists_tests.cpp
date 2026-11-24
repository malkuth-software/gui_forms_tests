// The bit-flip run's persisting states (state hunt 2026-11-23, run 7): one
// flipped bit that left a control outside its defined states for good. Each
// case corrupts the stored state the way a flip did, through the library's
// test hook (detail::StateFlipAccess, a friend of the classes whose storage
// is corrupted, defined here and nowhere else), and checks the outcome the
// design now gives: the state is put right, or the process fails fast with
// the defect report.
//
// Modes: `repairs` runs the cases that are put right and exits 0. The others
// must end through the fail-fast path (cmake/expect_fail_fast.cmake checks
// the report and that "survived" is never printed):
// - lifecycle_bit, lifecycle_zero, lifecycle_dispose (F1): a lifecycle code
//   with one bit flipped, or zeroed, read by is_alive or dispose;
// - enumerator (F3): a retained enumerator outside its enumeration read back;
// - container_size (F2): a table read of retained state that throws
//   length_error, as a corrupted container header makes the copy throw;
// - range_bounds (F5): range bounds no write makes, read back.

#include "gui_forms/binding/property_table/property_table.hpp"
#include "gui_forms/binding/value/binding_value.hpp"
#include "gui_forms/component/component/component.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/numeric_up_down.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/track_bar.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace gui_forms::detail {

// The flips, each the corruption one bit (or a zeroed word) made in run 7.
struct StateFlipAccess final {
    static void flip_lifecycle_bit(Component& component, unsigned bit) {
        const std::uint32_t code = static_cast<std::uint32_t>(component.state_) ^ (std::uint32_t{1} << bit);
        component.state_ = static_cast<ComponentState>(code);
    }
    static void zero_lifecycle(Component& component) { component.state_ = static_cast<ComponentState>(0U); }

    // The kept inherited values, as current, with `enabled` set as given.
    static void set_kept_enabled(const Control& control, bool enabled) {
        static_cast<void>(control.inherited_values());
        control.inherited_.enabled = enabled;
    }

    static void set_range_value(RangeControl& range, double value) { range.value_ = value; }
    static double stored_range_value(const RangeControl& range) { return range.value_; }
    static void set_range_bounds(RangeControl& range, double minimum, double maximum) {
        range.minimum_ = minimum;
        range.maximum_ = maximum;
    }

    static void set_glow_target(NumericUpDown& numeric, double target) { numeric.scrub_glow_to_ = target; }

    static BindingValue read_dock_code(std::int64_t code) {
        BindingValue value = Control::current_enum_property_value(Control::dock_style_property_enum(), code);
        return value;
    }
};

} // namespace gui_forms::detail

namespace {

using namespace gui_forms;
using Flip = gui_forms::detail::StateFlipAccess;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// A death test must not open the system's crash dialog.
void quiet_crash_reporting() {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#if defined(_MSC_VER) || defined(_UCRT)
    _set_abort_behavior(0U, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#endif
}

// --- Put right ---------------------------------------------------------------

// F4, run 7 bytes 0x1d1, 0x1d2, 0x234: the kept inherited `enabled` false
// while the control's own `enabled` is true. The rewrite of `enabled` with
// the value it holds now derives the kept values again.
void kept_enabled_false_is_rederived_by_an_equal_write() {
    std::shared_ptr<Control> control = make_control<Control>(StableId("flip.enabled"));
    require((*control).states().enabled, "F4: enabled at rest");
    Flip::set_kept_enabled(*control, false);
    (*control).enabled = true;
    require((*control).enabled.read() && (*control).states().enabled && !(*control).states().disabled,
            "F4: an equal write of enabled puts the anatomy back to enabled");
}

// F4, run 7 byte 0x69: the anatomy enabled while the control's own enabled
// is false. Kept values that claim more than the control's own fact are not
// used: the next read derives them again, with no write at all.
void kept_enabled_beyond_own_fact_is_not_used() {
    std::shared_ptr<Control> control = make_control<Control>(StableId("flip.disabled"));
    (*control).enabled = false;
    require((*control).states().disabled, "F4: disabled after the write");
    Flip::set_kept_enabled(*control, true);
    require((*control).states().disabled && !(*control).states().enabled && !(*control).effectively_enabled(),
            "F4: kept enabled with the control's own enabled false is derived again");
}

// F5, run 7 bytes 0xaf0 to 0xaf7: a range value outside its bounds, or not a
// number. The read clamps it as set_range does, and a write of the value
// read stores it.
void range_value_outside_its_bounds_is_clamped() {
    std::shared_ptr<TrackBar> bar = make_control<TrackBar>(StableId("flip.track"));
    (*bar).set_range(10.0, 50.0);
    Flip::set_range_value(*bar, -4.0);
    require((*bar).value.read() == 10.0 && (*bar).normalized_value() == 0.0, "F5: a value below the minimum reads as it");
    (*bar).value = (*bar).value.read();
    require(Flip::stored_range_value(*bar) == 10.0, "F5: writing the value read stores it");
    Flip::set_range_value(*bar, 1.0e300);
    require((*bar).value.read() == 50.0 && (*bar).normalized_value() == 1.0, "F5: a value above the maximum reads as it");
    Flip::set_range_value(*bar, std::numeric_limits<double>::quiet_NaN());
    require((*bar).value.read() == 10.0, "F5: a value that is not a number reads as the minimum");
    (*bar).set_range(0.0, 20.0);
    require(Flip::stored_range_value(*bar) == 0.0, "F5: set_range gives a value that is not a number the minimum");
}

// F5, run 7 NumericUpDown bytes 0x10d1 to 0x10d7: the glow lit with no scrub.
// Layout fades it: the scrub owns the fact.
void glow_without_scrub_fades_on_layout() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("flip.root"));
    std::shared_ptr<NumericUpDown> numeric = make_control<NumericUpDown>(StableId("flip.numeric"));
    (*numeric).set_requested_bounds({10.0, 10.0, 120.0, 26.0});
    (*root).add_child(numeric);
    Window window(root, {200.0, 80.0});
    window.perform_layout();
    require(!(*numeric).scrubbing() && (*numeric).scrub_glow_target() == 0.0, "F5: unlit at rest");
    Flip::set_glow_target(*numeric, 1.0);
    (*numeric).set_requested_bounds({10.0, 10.0, 124.0, 26.0});
    window.perform_layout();
    require((*numeric).scrub_glow_target() == 0.0, "F5: a lit glow with no scrub fades on layout");
}

int repairs_mode() {
    kept_enabled_false_is_rederived_by_an_equal_write();
    kept_enabled_beyond_own_fact_is_not_used();
    range_value_outside_its_bounds_is_clamped();
    glow_without_scrub_fades_on_layout();
    std::cout << "flip-persists-tests: repairs pass\n";
    return 0;
}

// --- Fail fast -----------------------------------------------------------------

// F1, run 7 bytes 0x8 to 0xb: any one bit of the lifecycle word.
void lifecycle_bit_mode() {
    std::shared_ptr<Control> control = make_control<Control>(StableId("flip.lifecycle"));
    Flip::flip_lifecycle_bit(*control, 13U);
    const bool alive = (*control).is_alive();
    std::printf("survived: alive=%d\n", alive ? 1 : 0);
}

void lifecycle_zero_mode() {
    std::shared_ptr<Control> control = make_control<Control>(StableId("flip.zeroed"));
    Flip::zero_lifecycle(*control);
    const bool disposed = (*control).is_disposed();
    std::printf("survived: disposed=%d\n", disposed ? 1 : 0);
}

// F1: before, dispose returned at once for a code that was not alive, so an
// attached control could never be disposed. Now the code is checked first.
void lifecycle_dispose_mode() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("flip.root"));
    std::shared_ptr<Control> child = make_control<Control>(StableId("flip.child"));
    (*root).add_child(child);
    Window window(root, {100.0, 60.0});
    window.perform_layout();
    Flip::flip_lifecycle_bit(*child, 0U);
    (*child).dispose();
    std::printf("survived: dispose returned\n");
}

// F3, run 7 bytes 0x128 to 0x12c (dock, anchor, justify_self, align_self,
// auto_size_mode): an enumerator outside its enumeration, read back.
void enumerator_mode() {
    const BindingValue value = Flip::read_dock_code(77);
    std::printf("survived: %d\n", static_cast<int>(value.index()));
}

// A control whose one table property reads retained state that throws
// length_error on the copy, as a corrupted vector or string header does
// (run 7 F2: `classes` at 0x170, `accessible_description` at 0x248).
const PropertyDescriptor& describe_hollow() {
    static const PropertyDescriptor descriptor = property_table_descriptor(
        PropertyDescriptor{"hollow", BindingValueKind::text, "Behavior", "", BindingValue{std::string()}, Dirty::none},
        PropertyOperations{true, false, false, false});
    return descriptor;
}

BindingValue read_hollow(const Component&) {
    throw std::length_error("vector");
}

const PropertyEntry hollow_entries[] = {
    {"hollow", &describe_hollow, &read_hollow, nullptr, nullptr, nullptr, nullptr, nullptr},
};

class HollowControl final : public Control {
public:
    explicit HollowControl(StableId stable_id) : Control(std::move(stable_id)) {}
    [[nodiscard]] std::span<const PropertyEntry> property_table() const noexcept override { return hollow_entries; }
};

void container_size_mode() {
    std::shared_ptr<HollowControl> control = make_control<HollowControl>(StableId("flip.hollow"));
    const std::optional<BindingValue> value = (*control).property_value("hollow");
    std::printf("survived: %d\n", value ? 1 : 0);
}

// F5: range bounds that no write makes (the minimum above the maximum).
void range_bounds_mode() {
    std::shared_ptr<TrackBar> bar = make_control<TrackBar>(StableId("flip.bounds"));
    Flip::set_range_bounds(*bar, 50.0, 10.0);
    const double value = (*bar).value.read();
    std::printf("survived: %f\n", value);
}

} // namespace

int main(int argc, char** argv) {
    quiet_crash_reporting();
    const std::string_view mode = argc > 1 ? std::string_view(argv[1]) : std::string_view("repairs");
    try {
        if (mode == "repairs") return repairs_mode();
        if (mode == "lifecycle_bit") lifecycle_bit_mode();
        else if (mode == "lifecycle_zero") lifecycle_zero_mode();
        else if (mode == "lifecycle_dispose") lifecycle_dispose_mode();
        else if (mode == "enumerator") enumerator_mode();
        else if (mode == "container_size") container_size_mode();
        else if (mode == "range_bounds") range_bounds_mode();
        else {
            std::fprintf(stderr, "usage: gui_forms_flip_persists_tests repairs|lifecycle_bit|lifecycle_zero|"
                                 "lifecycle_dispose|enumerator|container_size|range_bounds\n");
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "flip-persists-tests: fail: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
