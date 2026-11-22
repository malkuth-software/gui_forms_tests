#include "support.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/indicator_lamp.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/radio_button.hpp"
#include "gui_forms/controls/scaled_group_box.hpp"

namespace gallery_pages_tests {

// The industrial-surfaces page (skeudoindustrial surfaces, phase 5): its
// finish and glint swatches, the lamps (IndicatorLamps naming their bakes,
// each with its colour; a lamp whose bake is missing shows the theme's
// lens), their status line (no bakes are shipped, so it says where they
// are made), the Lamps check box that switches them, and the industrial
// theme over the GOST paints, put on the preview group and taken off; the
// preview's indicator lamps (Start and Stop switch Running, the Coolant
// check box its flow lamp, a pinpoint, and none is hit), its feed choice
// drawn with lamps (the row takes the click), and its danger key; the
// carbon fibre and brushed metal swatches beside the glints.
// Compositing the lamps is covered by gui_forms_engine_light_planes_tests
// and the baked lens by gui_forms_indicator_lamp_tests, and the industrial
// package's compile by gui_forms_theme_gost_palette_tests.
void test_industrial_surfaces(gui_forms::Window& window) {
    select_page(window, "industrial-surfaces");
    require(window.find("gallery.industrial-surfaces.heading") != nullptr,
            "industrial-surfaces page shows its heading");
    require(window.find("gallery.industrial.finish.hammertone") != nullptr &&
                window.find("gallery.industrial.finish.cloth-weave") != nullptr &&
                window.find("gallery.industrial.glint.painted-sand") != nullptr,
            "the page shows the finish and glint swatches");
    require(window.find("gallery.industrial.pattern.carbon-fibre") != nullptr &&
                window.find("gallery.industrial.pattern.brushed-metal") != nullptr,
            "the page shows the carbon fibre and brushed metal swatches");

    const std::shared_ptr<Label> bakes = std::dynamic_pointer_cast<Label>(window.find("gallery.industrial.bakes"));
    require(bakes != nullptr && !(*bakes).text.read().empty(), "the lamps' status line says what is shown");
    const std::shared_ptr<IndicatorLamp> ok_lamp =
        std::dynamic_pointer_cast<IndicatorLamp>(window.find("gallery.industrial.lamp.dialog-ok"));
    const std::shared_ptr<IndicatorLamp> clear_lamp =
        std::dynamic_pointer_cast<IndicatorLamp>(window.find("gallery.industrial.lamp.indicator-clear"));
    require(ok_lamp != nullptr && clear_lamp != nullptr, "the page places its lamps as indicator lamps");
    if (ok_lamp == nullptr || clear_lamp == nullptr) return;
    require((*ok_lamp).light_planes.read() == "dialog_ok_up" && (*clear_lamp).light_planes.read() == "indicator_clear_up",
            "each lamp names its bake");
    require((*ok_lamp).lamp_color.read().present() && (*ok_lamp).lamp_color.read() != (*clear_lamp).lamp_color.read(),
            "each lamp carries its own colour, so one bake serves every colour");
    const bool baked = (*ok_lamp).light_planes_shown();
    require(baked || (*bakes).text.read().find("No light-plane bakes") != std::string::npos,
            "without bakes the status line says where they are made");

    const std::shared_ptr<CheckBox> lamps = std::dynamic_pointer_cast<CheckBox>(window.find("gallery.industrial.lamps"));
    require(lamps != nullptr && !(*lamps).checked.read(), "the lamps start out");
    require((*ok_lamp).state.read() == IndicatorLampState::off, "and dark");
    click(window, lamps);
    window.perform_layout();
    require((*lamps).checked.read() && (*ok_lamp).state.read() == IndicatorLampState::on &&
                (*clear_lamp).state.read() == IndicatorLampState::on,
            "the Lamps check box switches them on");
    click(window, lamps);
    window.perform_layout();
    require(!(*lamps).checked.read() && (*ok_lamp).state.read() == IndicatorLampState::off, "and off");

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
