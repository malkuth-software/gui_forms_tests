#include "support.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/indicator_lamp.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/radio_button.hpp"
#include "gui_forms/controls/scaled_group_box.hpp"

#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace gallery_pages_tests {

// The industrial-surfaces page (skeudoindustrial surfaces, phase 5),
// rebuilt around the owner's process-control dialog (owner, 2026-11-22):
// the dialog's blasted enamel and its title band, its three status lamps
// (jewels lit from the shipped indicator bake, each in its colour, shown
// with no light-plane directory set, FAULT failing) and its CANCEL and OK
// keys, buttons the gallery theme draws with the light map (`lit-key`),
// with no text but their accessible names; the Lamps check box that
// switches the status lamps and OK (its `lamp-off` class) while CANCEL
// stays lit; the retired
// finish swatches gone; the industrial theme over the GOST paints, put on
// the preview group and taken off; the preview's indicator lamps (Start
// and Stop switch Running, the Coolant check box its flow lamp, a
// pinpoint, and none is hit), its feed choice drawn with lamps (the row
// takes the click), and its danger key; the carbon fibre swatch.
// Compositing the lamps is covered by gui_forms_engine_light_planes_tests,
// the light map by gui_forms_engine_primitive_tests, the failing lamp and
// the baked lens by gui_forms_indicator_lamp_tests, the enamel by
// gui_forms_engine_finish_tests, and the industrial package's compile by
// gui_forms_theme_gost_palette_tests.
void test_industrial_surfaces(gui_forms::Window& window) {
    select_page(window, "industrial-surfaces");
    require(window.find("gallery.industrial-surfaces.heading") != nullptr,
            "industrial-surfaces page shows its heading");
    require(window.find("gallery.industrial.dialog") != nullptr &&
                window.find("gallery.industrial.dialog.title-band") != nullptr &&
                window.find("gallery.industrial.dialog.heading") != nullptr,
            "the page shows the owner's dialog");
    require(window.find("gallery.industrial.finish.hammertone") == nullptr &&
                window.find("gallery.industrial.glint.painted-sand") == nullptr &&
                window.find("gallery.industrial.pattern.brushed-metal") == nullptr,
            "the finish swatches are retired");
    require(window.find("gallery.industrial.pattern.carbon-fibre") != nullptr,
            "the page shows the carbon fibre swatch");

    constexpr std::array<std::string_view, 3> status_ids{"gallery.industrial.status.pump-ready",
                                                         "gallery.industrial.status.heating",
                                                         "gallery.industrial.status.fault"};
    std::vector<std::shared_ptr<IndicatorLamp>> status{};
    for (const std::string_view id : status_ids) {
        const std::shared_ptr<IndicatorLamp> lamp = std::dynamic_pointer_cast<IndicatorLamp>(window.find(id));
        require(lamp != nullptr, "the dialog shows each status lamp as an indicator lamp");
        if (lamp == nullptr) return;
        status.push_back(lamp);
    }
    for (const std::shared_ptr<IndicatorLamp>& lamp : status) {
        require((*lamp).light_planes.read() == "indicator_backed_up" && (*lamp).light_planes_shown(),
                "each status lamp is the shipped jewel, shown with no light-plane directory");
        require((*lamp).lamp_color.read().present(), "and carries its own colour");
    }
    require((*status[0]).lamp_color.read() != (*status[1]).lamp_color.read() &&
                (*status[1]).lamp_color.read() != (*status[2]).lamp_color.read(),
            "PUMP READY, HEATING and FAULT each take their own colour");
    require((*status[0]).state.read() == IndicatorLampState::on && (*status[1]).state.read() == IndicatorLampState::on &&
                (*status[2]).state.read() == IndicatorLampState::fault,
            "the status lamps start lit, FAULT failing");
    require(!(*status[0]).hit_test_local(Point{4.0, 4.0}), "a status lamp is never hit");

    const std::shared_ptr<Button> cancel = std::dynamic_pointer_cast<Button>(window.find("gallery.industrial.key.cancel"));
    const std::shared_ptr<Button> ok = std::dynamic_pointer_cast<Button>(window.find("gallery.industrial.key.ok"));
    require(cancel != nullptr && ok != nullptr, "the dialog shows its CANCEL and OK keys as buttons");
    if (cancel == nullptr || ok == nullptr) return;
    require((*cancel).has_class("lit-key") && (*cancel).has_class("lit-cancel") && (*ok).has_class("lit-key") &&
                (*ok).has_class("lit-ok"),
            "each key takes the gallery theme's light-map classes");
    require((*cancel).text.read().empty() && (*ok).text.read().empty() &&
                (*cancel).semantic_descriptor().name == "Cancel" && (*ok).semantic_descriptor().name == "OK",
            "the bakes carry the legends: the keys have no text, only their accessible names");
    require(!(*ok).has_class("lamp-off"), "both keys start lit");

    const std::shared_ptr<CheckBox> lamps = std::dynamic_pointer_cast<CheckBox>(window.find("gallery.industrial.lamps"));
    require(lamps != nullptr && (*lamps).checked.read(), "the lamps start on");
    click(window, lamps);
    window.perform_layout();
    require(!(*lamps).checked.read() && (*status[0]).state.read() == IndicatorLampState::off &&
                (*status[2]).state.read() == IndicatorLampState::off && (*ok).has_class("lamp-off"),
            "the Lamps check box puts the status lamps and OK out");
    require(!(*cancel).has_class("lamp-off"), "while CANCEL stays lit");
    click(window, lamps);
    window.perform_layout();
    require((*lamps).checked.read() && (*status[0]).state.read() == IndicatorLampState::on &&
                (*status[2]).state.read() == IndicatorLampState::fault && !(*ok).has_class("lamp-off"),
            "and lights them again, FAULT failing");

    const std::shared_ptr<ScaledGroupBox> preview =
        std::dynamic_pointer_cast<ScaledGroupBox>(window.find("gallery.industrial.preview"));
    const std::shared_ptr<Label> state = std::dynamic_pointer_cast<Label>(window.find("gallery.industrial.preview.state"));
    require(preview != nullptr && state != nullptr && (*preview).theme_override() == nullptr,
            "the preview starts in the Gallery's theme");
    click(window, window.find("gallery.industrial.theme"));
    window.perform_layout();
    if ((*preview).theme_override() == nullptr) std::cerr << "  industrial theme: " << (*state).text.read() << '\n';
    require((*preview).theme_override() != nullptr, "the industrial theme compiles over the GOST paints and shows");
    click(window, window.find("gallery.industrial.theme"));
    window.perform_layout();
    require((*preview).theme_override() == nullptr, "clearing it restores the Gallery's theme");

    // The preview's lamps and danger key, in the default theme here.
    const std::shared_ptr<IndicatorLamp> running =
        std::dynamic_pointer_cast<IndicatorLamp>(window.find("gallery.industrial.preview.running"));
    const std::shared_ptr<IndicatorLamp> fault =
        std::dynamic_pointer_cast<IndicatorLamp>(window.find("gallery.industrial.preview.fault"));
    const std::shared_ptr<IndicatorLamp> flow =
        std::dynamic_pointer_cast<IndicatorLamp>(window.find("gallery.industrial.preview.coolant-lamp"));
    require(running != nullptr && fault != nullptr && flow != nullptr, "the preview holds three indicator lamps");
    if (running == nullptr || fault == nullptr || flow == nullptr) return;
    require((*running).state.read() == IndicatorLampState::off && (*fault).state.read() == IndicatorLampState::error &&
                (*flow).state.read() == IndicatorLampState::on,
            "Running starts off, Fault shows an error and Coolant flow is on");
    require(!(*running).hit_test_local(Point{4.0, 4.0}), "a lamp is never hit");
    click(window, window.find("gallery.industrial.preview.start"));
    require((*running).state.read() == IndicatorLampState::on, "Start lights Running");
    click(window, window.find("gallery.industrial.preview.stop"));
    require((*running).state.read() == IndicatorLampState::off, "Stop puts Running out");
    click(window, window.find("gallery.industrial.preview.coolant"));
    require((*flow).state.read() == IndicatorLampState::off, "clearing Coolant puts its flow lamp out");
    click(window, window.find("gallery.industrial.preview.coolant"));
    require((*flow).state.read() == IndicatorLampState::on, "and setting it lights it again");
    const std::shared_ptr<Button> erase =
        std::dynamic_pointer_cast<Button>(window.find("gallery.industrial.preview.erase"));
    require(erase != nullptr && (*erase).danger.read(), "Erase program is a danger key");
    require((*flow).lamp_style.read() == IndicatorLampStyle::pinpoint && !(*flow).parts().bezel.present,
            "Coolant flow is a pinpoint, with no bezel");

    // The feed choice is a radio group drawn with lamps: a click on the row
    // (its centre, on the caption, well away from the lamp) chooses.
    const std::shared_ptr<RadioButton> manual =
        std::dynamic_pointer_cast<RadioButton>(window.find("gallery.industrial.preview.manual"));
    const std::shared_ptr<RadioButton> automatic =
        std::dynamic_pointer_cast<RadioButton>(window.find("gallery.industrial.preview.automatic"));
    require(manual != nullptr && automatic != nullptr, "the preview holds the feed choice");
    if (manual == nullptr || automatic == nullptr) return;
    require((*manual).indicator_style.read() == ChoiceIndicatorStyle::lamp &&
                (*automatic).indicator_style.read() == ChoiceIndicatorStyle::lamp && (*manual).checked.read(),
            "the feed choice is drawn with lamps, Manual feed chosen");
    click(window, automatic);
    require((*automatic).checked.read() && !(*manual).checked.read(), "a click on the row chooses Automatic feed");
    click(window, manual);
    require((*manual).checked.read() && !(*automatic).checked.read(), "and Manual feed again");
}

}  // namespace gallery_pages_tests
