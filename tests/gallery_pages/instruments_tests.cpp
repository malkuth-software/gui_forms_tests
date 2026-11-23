#include "support.hpp"
#include "gui_forms/controls/dial.hpp"
#include "gui_forms/controls/linear_gauge.hpp"
#include "gui_forms/controls/pie_chart.hpp"
#include "gui_forms/controls/radial_gauge.hpp"
#include "gui_forms/controls/seven_segment_display.hpp"

namespace gallery_pages_tests {

// The instruments page (W-5b, W-9): the readouts and their Ghost segments
// check box, the gauges' readings and alarms, the dial that drives the
// linked readout and gauge, and the pie and doughnut charts.
void test_instruments(gui_forms::Window& window) {
    select_page(window, "instruments");
    require(window.find("gallery.instruments.heading") != nullptr, "instruments page shows its heading");
    const std::shared_ptr<SevenSegmentDisplay> clock =
        std::dynamic_pointer_cast<SevenSegmentDisplay>(window.find("gallery.instruments.clock"));
    require(clock && (*clock).text.read() == "12:45" && (*clock).ghost_segments.read(), "the clock reads 12:45");
    click(window, window.find("gallery.instruments.ghosts"));
    window.perform_layout();
    require(!(*clock).ghost_segments.read(), "clearing Ghost segments hides the unlit segments");

    const std::shared_ptr<LinearGauge> temperature =
        std::dynamic_pointer_cast<LinearGauge>(window.find("gallery.instruments.temperature"));
    const std::shared_ptr<LinearGauge> pressure =
        std::dynamic_pointer_cast<LinearGauge>(window.find("gallery.instruments.pressure"));
    require(temperature && (*temperature).in_alarm(), "the temperature gauge shows an alarm");
    require(pressure && !(*pressure).in_alarm() && (*pressure).value_text() == "128 kPa",
            "the pressure gauge reads in kPa");

    const std::shared_ptr<Dial> dial = std::dynamic_pointer_cast<Dial>(window.find("gallery.instruments.dial"));
    const std::shared_ptr<SevenSegmentDisplay> readout =
        std::dynamic_pointer_cast<SevenSegmentDisplay>(window.find("gallery.instruments.dial.readout"));
    const std::shared_ptr<RadialGauge> linked =
        std::dynamic_pointer_cast<RadialGauge>(window.find("gallery.instruments.dial.gauge"));
    require(dial && readout && linked && (*readout).text.read() == "35", "the readout follows the dial");
    require(window.request_focus(dial), "the dial takes focus");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::end}), "End is handled");
    window.perform_layout();
    require((*readout).text.read() == "100" && (*linked).in_alarm(), "the linked gauge alarms above 80");

    const std::string semantics = window.semantic_snapshot().to_json();
    require(semantics.find("\"role\":\"slider\"") != std::string::npos, "the dial is a slider");

    const std::shared_ptr<PieChart> pie = std::dynamic_pointer_cast<PieChart>(window.find("gallery.instruments.pie"));
    const std::shared_ptr<PieChart> doughnut =
        std::dynamic_pointer_cast<PieChart>(window.find("gallery.instruments.doughnut"));
    require(pie && doughnut && (*pie).slices().size() == 5U && (*doughnut).slices().size() == 4U &&
                (*pie).label_style.read() == PieChartLabelStyle::outside && (*doughnut).doughnut_radius.read() == 55.0,
            "the page shows a pie with outside labels and a doughnut, each with its slices");
    require(window.request_focus(pie) && window.dispatch_key({KeyAction::down, PhysicalKey::down}) &&
                (*pie).selected_id() == stxlib::Optional<std::string_view>::of("documents"),
            "Down selects the pie's first slice");
}

}  // namespace gallery_pages_tests
