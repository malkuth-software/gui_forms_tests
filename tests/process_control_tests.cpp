// The process-control demonstrator (owner, 2026-11-22; skeudoindustrial
// surfaces, phase 3; demo/process_control/process_control.hpp): its dialog
// (the enamel classes, the three status lamps lit from the shipped jewel,
// FAULT failing, the CANCEL and OK keys as LampKeys with no text but their
// names, OK dark at rest and lit only while held, its lamp striking and
// cooling, CANCEL lit throughout, the presses moving on the engine's
// motion), its theme sheet
// (compiled on the root, rebuilt for the paint and the seed), and its
// strip: each lamp's state cycled off, on, error and fault, the radio
// lamps' colour set at run time, the coolant pinpoint, the Lamps toggle
// that puts the status lamps and OK's lamp out, the keys' messages,
// and --check's staging. Compositing a bake is covered by
// gui_forms_engine_light_planes_tests, the failing lamp by
// gui_forms_indicator_lamp_tests and gui_forms_engine_light_planes_tests,
// the enamel by gui_forms_engine_finish_tests.

#include "process_control.hpp"

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/indicator_lamp.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/radio_button.hpp"
#include "gui_forms/engine/motion_clock/motion_clock.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/theme/theme/theme.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace gui_forms;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// The test's time for the window's motion: moves start at it and frames are
// polled at it, never at the wall clock.
class ManualTime final : public MotionTimeSource {
public:
    [[nodiscard]] FrameTime now() const noexcept override { return time; }
    FrameTime time{std::chrono::seconds(1000)};
};

// Advances the test's time by `step` and polls the window's frames there.
void run_for(Window& window, ManualTime& time, std::chrono::milliseconds step) {
    time.time += step;
    static_cast<void>(window.poll_frame_schedule(time.time));
}

// The control's centre in window coordinates.
Point centre_of(const Control& control) {
    const Rect bounds = control.absolute_bounds();
    const Point centre{bounds.x + bounds.width * 0.5, bounds.y + bounds.height * 0.5};
    return centre;
}

template <typename Found>
std::shared_ptr<Found> find(Window& window, std::string_view id) {
    const std::shared_ptr<Found> found = std::dynamic_pointer_cast<Found>(window.find(id));
    if (found == nullptr) throw std::runtime_error("the demonstrator has no " + std::string(id));
    return found;
}

// A primary press and release at the control's centre.
void click(Window& window, const std::shared_ptr<Control>& control) {
    const Rect bounds = (*control).absolute_bounds();
    const Point centre{bounds.x + bounds.width * 0.5, bounds.y + bounds.height * 0.5};
    const bool down = window.dispatch_pointer({PointerAction::down, PointerButton::primary, centre});
    const bool up = window.dispatch_pointer({PointerAction::up, PointerButton::primary, centre});
    if (!down || !up) throw std::runtime_error("the click on " + std::string((*control).stable_id().value()) + " failed");
    window.perform_layout();
}

void test_the_sheet() {
    const std::string sheet = process_control::process_control_sheet(0U, process_control::study_seed);
    require(sheet.find("finish(blasted-enamel-glint, #4d676d, #a3a3a3, 11, 1.5)") != std::string::npos &&
                sheet.find("finish(blasted-enamel-heavy-glint, #656f74, #a3a3a3, 11, 1.5)") != std::string::npos,
            "the study's paint: ГОСТ 397 under ГОСТ 832's band, seed 11, the grain 1.5");
    require(sheet.find("light-map") == std::string::npos && sheet.find("lamp-power") == std::string::npos,
            "the sheet lights no key: the keys draw and animate their own bakes");
    require(process_control::process_control_sheet(process_control::enamel_paints.size(), 3U) ==
                process_control::process_control_sheet(0U, 3U),
            "the paint index wraps");
    require(process_control::process_control_sheet(1U, 12U).find("#80948a, #a3a3a3, 12, 1.5)") != std::string::npos,
            "another paint and seed reach the enamel");
}

void test_the_dialog(Window& window) {
    const std::shared_ptr<Control> root = window.find("process.root");
    require(root != nullptr && (*root).theme_override() != nullptr && !(*root).theme_sheet_pending(),
            "the program's sheet compiles on the root");
    require((*find<Control>(window, "process.dialog")).has_class("blasted-enamel") &&
                (*find<Control>(window, "process.dialog.title-band")).has_class("blasted-enamel-heavy"),
            "the dialog and its title band take the enamel classes");
    const std::array<std::string_view, 3> ids{"process.status.pump-ready", "process.status.heating",
                                              "process.status.fault"};
    for (const std::string_view id : ids) {
        const std::shared_ptr<IndicatorLamp> lamp = find<IndicatorLamp>(window, id);
        require((*lamp).light_planes.read() == "indicator_backed_up" && (*lamp).light_planes_shown() &&
                    (*lamp).lamp_color.read().present(),
                "each status lamp is the shipped jewel in its own colour");
    }
    require((*find<IndicatorLamp>(window, "process.status.pump-ready")).state.read() == IndicatorLampState::on &&
                (*find<IndicatorLamp>(window, "process.status.fault")).state.read() == IndicatorLampState::fault,
            "PUMP READY starts lit and FAULT failing");
    const std::shared_ptr<process_control::LampKey> ok = find<process_control::LampKey>(window, "process.key.ok");
    const std::shared_ptr<process_control::LampKey> cancel =
        find<process_control::LampKey>(window, "process.key.cancel");
    require((*ok).text.read().empty() && (*ok).semantic_descriptor().name == "OK" &&
                (*cancel).semantic_descriptor().name == "Cancel",
            "the bakes carry the legends: the keys have only their names");
    require((*ok).lamp_power() == 0.0 && (*ok).press_share() == 0.0,
            "at rest OK is not lit: the room's light alone");
    require((*cancel).lamp_power() == process_control::key_lit_power && (*cancel).press_share() == 0.0,
            "CANCEL is lit throughout, as in the study");

    // Held, OK's lamp strikes (40% at once, full after 60 ms) and the key
    // goes down; released, it comes up and the lamp cools out.
    ManualTime time{};
    window.motion_clock().set_time_source(&time);
    const Point on_ok = centre_of(*ok);
    require(window.dispatch_pointer({PointerAction::down, PointerButton::primary, on_ok}),
            "the press on OK is delivered");
    require((*ok).lamp_power() >= 0.4 * process_control::key_lit_power - 1e-9 &&
                (*ok).lamp_power() < process_control::key_lit_power,
            "a held key's lamp strikes at 40%");
    run_for(window, time, std::chrono::milliseconds(30));
    const double rising = (*ok).lamp_power();
    require(rising > 0.4 * process_control::key_lit_power && rising < process_control::key_lit_power,
            "the strike rises on the engine's frames");
    for (int frame = 0; frame < 10; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    require((*ok).lamp_power() == process_control::key_lit_power && (*ok).press_share() == 1.0,
            "held, the key is down and fully lit");
    require(window.dispatch_pointer({PointerAction::up, PointerButton::primary, on_ok}),
            "the release is delivered");
    require((*ok).lamp_power() == process_control::key_lit_power, "the lamp does not go out in one step");
    for (int frame = 0; frame < 16; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    require((*ok).lamp_power() == 0.0 && (*ok).press_share() == 0.0,
            "released, the lamp cools out and the key comes up");

    // CANCEL goes down and up, its lamp lit all the while.
    const Point on_cancel = centre_of(*cancel);
    require(window.dispatch_pointer({PointerAction::down, PointerButton::primary, on_cancel}),
            "the press on CANCEL is delivered");
    for (int frame = 0; frame < 10; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    require((*cancel).press_share() == 1.0 && (*cancel).lamp_power() == process_control::key_lit_power,
            "held, CANCEL is down and still lit");
    require(window.dispatch_pointer({PointerAction::up, PointerButton::primary, on_cancel}),
            "the release on CANCEL is delivered");
    for (int frame = 0; frame < 16; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    require((*cancel).press_share() == 0.0 && (*cancel).lamp_power() == process_control::key_lit_power,
            "released, CANCEL comes up lit");
    window.motion_clock().set_time_source(nullptr);
    const std::shared_ptr<Label> message = find<Label>(window, "process.message");
    click(window, ok);
    require((*message).text.read() == "Transfer started: pump to reserve tank.", "OK starts the transfer");
    click(window, cancel);
    require((*message).text.read() == "Transfer cancelled.", "CANCEL cancels it");
}

void test_the_strip(Window& window) {
    const std::shared_ptr<IndicatorLamp> pump = find<IndicatorLamp>(window, "process.status.pump-ready");
    const std::shared_ptr<Button> pump_state = find<Button>(window, "process.status.pump-ready.state");
    require((*pump_state).text.read() == "PUMP READY: on", "the state button names the lamp's state");
    click(window, pump_state);
    require((*pump).state.read() == IndicatorLampState::error && (*pump_state).text.read() == "PUMP READY: error",
            "a click moves on to error");
    click(window, pump_state);
    require((*pump).state.read() == IndicatorLampState::fault, "then fault");
    click(window, pump_state);
    require((*pump).state.read() == IndicatorLampState::off, "then off");
    click(window, pump_state);
    require((*pump).state.read() == IndicatorLampState::on, "then on again");

    const std::shared_ptr<Control> root = window.find("process.root");
    const std::shared_ptr<const Theme> first = (*root).theme_override();
    click(window, find<Button>(window, "process.paint"));
    require((*root).theme_sheet().find("#80948a") != std::string_view::npos && (*root).theme_override() != nullptr &&
                (*root).theme_override() != first,
            "the paint picker compiles the enamel over the next paint");
    click(window, find<Button>(window, "process.seed.up"));
    require((*root).theme_sheet().find(", 12, 1.5)") != std::string_view::npos &&
                (*find<Label>(window, "process.seed")).text.read() == "Seed 12",
            "the seed moves on");

    const std::shared_ptr<RadioButton> manual = find<RadioButton>(window, "process.feed.manual");
    const std::shared_ptr<RadioButton> automatic = find<RadioButton>(window, "process.feed.automatic");
    require((*manual).indicator_style.read() == ChoiceIndicatorStyle::lamp && (*manual).checked.read() &&
                !(*manual).lamp_color.read().present(),
            "the feed is a lamp radio, its colour the default");
    click(window, find<Button>(window, "process.radio.blue"));
    require((*manual).lamp_color.read().present() && (*manual).lamp_color.read() == (*automatic).lamp_color.read(),
            "a colour button sets both radio lamps' colour");

    const std::shared_ptr<CheckBox> coolant = find<CheckBox>(window, "process.coolant");
    const std::shared_ptr<IndicatorLamp> flow = find<IndicatorLamp>(window, "process.coolant-flow");
    require((*flow).lamp_style.read() == IndicatorLampStyle::pinpoint && (*flow).state.read() == IndicatorLampState::on,
            "the coolant flow is a lit pinpoint");
    click(window, coolant);
    require((*flow).state.read() == IndicatorLampState::off, "clearing Coolant puts it out");
    require(process_control::check_process_control_box(window, "process.coolant") && (*coolant).checked.read() &&
                (*flow).state.read() == IndicatorLampState::on,
            "--check sets a box as a click would");
    require(!process_control::check_process_control_box(window, "process.no-such-box"), "an unknown box is refused");

    const std::shared_ptr<CheckBox> lamps = find<CheckBox>(window, "process.lamps");
    const std::shared_ptr<process_control::LampKey> ok = find<process_control::LampKey>(window, "process.key.ok");
    click(window, lamps);
    require((*pump).state.read() == IndicatorLampState::off &&
                (*find<IndicatorLamp>(window, "process.status.fault")).state.read() == IndicatorLampState::off,
            "Lamps off puts the status lamps out");
    require((*find<process_control::LampKey>(window, "process.key.cancel")).lamp_power() ==
                process_control::key_lit_power,
            "and leaves CANCEL lit");
    ManualTime time{};
    window.motion_clock().set_time_source(&time);
    const Point on_ok = centre_of(*ok);
    require(window.dispatch_pointer({PointerAction::down, PointerButton::primary, on_ok}), "OK is pressed");
    for (int frame = 0; frame < 10; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    require((*ok).press_share() == 1.0 && (*ok).lamp_power() == 0.0,
            "with the lamps off a held key goes down but does not light");
    require(window.dispatch_pointer({PointerAction::up, PointerButton::primary, on_ok}), "and released");
    for (int frame = 0; frame < 10; ++frame) run_for(window, time, std::chrono::milliseconds(16));
    window.motion_clock().set_time_source(nullptr);
    click(window, lamps);
    require((*pump).state.read() == IndicatorLampState::on &&
                (*find<IndicatorLamp>(window, "process.status.fault")).state.read() == IndicatorLampState::fault &&
                (*ok).lamp_power() == 0.0,
            "and Lamps on lights the status lamps again, FAULT failing, the keys dark at rest");
}

} // namespace

int main() {
    try {
        test_the_sheet();
        std::unique_ptr<Window> window = process_control::make_process_control();
        (*window).set_active(true);
        (*window).perform_layout();
        test_the_dialog(*window);
        test_the_strip(*window);
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_process_control_tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "gui_forms_process_control_tests passed\n";
    return EXIT_SUCCESS;
}
