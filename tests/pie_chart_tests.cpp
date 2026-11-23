// PieChart (plots package): slices kept by ID, shares, the doughnut hole,
// hit testing on the wedges and the legend, selection by click and keys,
// activation, semantics, the published parts and refusals.

#include "gui_forms/anatomy/types/anatomy_types.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/pie_chart.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

struct RecordSelection final {
    std::vector<std::string>* seen{};
    void operator()(const PieSliceSelection& change) const {
        (*seen).push_back(change.current_id.present() ? *change.current_id.value() : std::string("(none)"));
    }
};

struct RecordActivation final {
    std::vector<std::string>* seen{};
    void operator()(const PieSliceActivation& activation) const { (*seen).push_back(activation.slice_id); }
};

PieSlice slice(std::string id, std::string label, double value) {
    PieSlice result;
    result.stable_id = std::move(id);
    result.label = std::move(label);
    result.value = value;
    return result;
}

// Half, a quarter, a quarter: from the top, "a" covers the right half,
// "b" the lower left quarter and "c" the upper left quarter.
std::vector<PieSlice> sample_slices() {
    std::vector<PieSlice> slices;
    slices.push_back(slice("a", "Alpha", 50.0));
    slices.push_back(slice("b", "Beta", 25.0));
    slices.push_back(slice("c", "Gamma", 25.0));
    return slices;
}

Point centre(Rect bounds) {
    const Point point{bounds.x + bounds.width * 0.5, bounds.y + bounds.height * 0.5};
    return point;
}

void click(Window& window, const PieChart& chart, Point local, std::uint32_t clicks = 1U) {
    const Rect absolute = chart.absolute_bounds();
    const Point point{absolute.x + local.x, absolute.y + local.y};
    PointerEvent down{PointerAction::down, PointerButton::primary, point};
    down.click_count = clicks;
    static_cast<void>(window.dispatch_pointer(down));
    PointerEvent up{PointerAction::up, PointerButton::primary, point};
    static_cast<void>(window.dispatch_pointer(up));
}

void slices_and_shares() {
    std::shared_ptr<PieChart> chart = make_control<PieChart>(StableId("chart"));
    (*chart).set_slices(sample_slices());
    require((*chart).total() == 100.0 && (*chart).share(0U) == 0.5 && (*chart).share(2U) == 0.25,
            "the total sums the values and each share is its part of it");
    require((*chart).index_of("b") == stxlib::Optional<std::size_t>::of(1U) && (*chart).index_of("z").absent(),
            "slices are found by ID");
    (*chart).set_slice_value("c", 75.0);
    require((*chart).total() == 150.0 && (*chart).share(2U) == 0.5, "set_slice_value changes the total and shares");
    std::shared_ptr<PieChart> empty = make_control<PieChart>(StableId("empty"));
    std::vector<PieSlice> zero;
    zero.push_back(slice("z", "Zero", 0.0));
    (*empty).set_slices(std::move(zero));
    require((*empty).total() == 0.0 && (*empty).share(0U) == 0.0, "a zero total gives every slice no share");
}

void parts_and_hit_testing() {
    std::shared_ptr<PieChart> chart = make_control<PieChart>(StableId("chart"));
    (*chart).set_slices(sample_slices());
    Window window(chart, {400.0, 240.0});
    window.perform_layout();
    const ControlAnatomy& anatomy = (*chart).anatomy();
    require(validate_control_anatomy(anatomy).defect == AnatomyDefect::none && anatomy.control_id == "pie_chart" &&
                find_item_system(anatomy, "slice").present(),
            "PieChart publishes a valid anatomy with a slice system");
    const PieChart::Parts parts = (*chart).parts();
    require(parts.plot.present && parts.legend.present && parts.legend.bounds.x > parts.plot.bounds.x,
            "the legend sits after the plot");
    const std::vector<PieChart::Slice> slices = (*chart).realized_slices();
    require(slices.size() == 3U && slices[0].stable_id == "a" && slices[0].parts.slice.present &&
                slices[0].parts.slice_label.present && slices[0].parts.legend_swatch.present &&
                slices[0].parts.legend_label.present && slices[1].parts.legend_label.bounds.y >
                                                             slices[0].parts.legend_label.bounds.y,
            "every slice publishes its wedge, label and legend entry, the entries in order");
    const Point middle = centre(parts.plot.bounds);
    const double reach = std::min(parts.plot.bounds.width, parts.plot.bounds.height) * 0.25;
    require((*chart).slice_at({middle.x + reach, middle.y}) == stxlib::Optional<std::size_t>::of(0U) &&
                (*chart).slice_at({middle.x - reach, middle.y + reach * 0.5}) == stxlib::Optional<std::size_t>::of(1U) &&
                (*chart).slice_at({middle.x - reach, middle.y - reach * 0.5}) == stxlib::Optional<std::size_t>::of(2U),
            "the wedges run clockwise from the top in the order given");
    require((*chart).slice_at(centre(slices[2].parts.legend_label.bounds)) == stxlib::Optional<std::size_t>::of(2U),
            "a legend entry hits its slice");
    (*chart).doughnut_radius = 60.0;
    window.perform_layout();
    require((*chart).slice_at(middle).absent(), "the doughnut's hole hits nothing");
    (*chart).start_angle = 180.0;
    window.perform_layout();
    require((*chart).slice_at({middle.x - reach * 1.2, middle.y}) == stxlib::Optional<std::size_t>::of(0U),
            "start_angle turns the first slice");
    (*chart).legend_visible = false;
    window.perform_layout();
    require(!(*chart).parts().legend.present && !(*chart).realized_slices()[0].parts.legend_label.present,
            "without the legend no entry is published");
}

void selection_and_activation() {
    std::shared_ptr<PieChart> chart = make_control<PieChart>(StableId("chart"));
    (*chart).set_slices(sample_slices());
    Window window(chart, {400.0, 240.0});
    window.perform_layout();
    std::vector<std::string> selections;
    SubscriptionToken selected = (*chart).selection_changed().subscribe(RecordSelection{&selections});
    std::vector<std::string> activations;
    SubscriptionToken activated = (*chart).slice_activated().subscribe(RecordActivation{&activations});
    const std::vector<PieChart::Slice> slices = (*chart).realized_slices();
    click(window, *chart, centre(slices[1].parts.legend_label.bounds));
    require(selections == std::vector<std::string>({"b"}) &&
                (*chart).selected_id() == stxlib::Optional<std::string_view>::of("b"),
            "a click on a legend entry selects its slice");
    require((*chart).realized_slices()[1].states.selected, "the selected slice publishes selected");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::down}) &&
                window.dispatch_key({KeyAction::down, PhysicalKey::down}) &&
                selections == std::vector<std::string>({"b", "c"}),
            "Down moves to the next slice and stops at the last");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::home}) &&
                window.dispatch_key({KeyAction::down, PhysicalKey::enter}) &&
                selections.back() == "a" && activations == std::vector<std::string>({"a"}),
            "Home selects the first slice and Enter activates it");
    click(window, *chart, centre(slices[2].parts.legend_label.bounds), 2U);
    require(activations.back() == "c", "a double click activates");
    std::vector<PieSlice> next = sample_slices();
    next.erase(next.begin() + 2);
    (*chart).set_slices(std::move(next));
    require(selections.back() == "(none)" && (*chart).selected_id().absent(),
            "set_slices drops a selection whose slice is gone and reports it");
    (*chart).set_selected_id(stxlib::Optional<std::string_view>::of("a"));
    (*chart).set_slices(sample_slices());
    require((*chart).selected_id() == stxlib::Optional<std::string_view>::of("a") && selections.back() == "a",
            "the selection stays on its ID across set_slices");
}

void semantics() {
    std::shared_ptr<PieChart> chart = make_control<PieChart>(StableId("chart"));
    (*chart).accessible_name = "Usage";
    (*chart).set_slices(sample_slices());
    Window window(chart, {400.0, 240.0});
    window.perform_layout();
    const SemanticDescriptor descriptor = (*chart).semantic_descriptor();
    require(descriptor.role == SemanticRole::list && descriptor.name == "Usage", "the chart is a named list");
    const std::vector<SemanticNode> children = (*chart).semantic_virtual_children();
    require(children.size() == 3U && children[0].role == SemanticRole::list_item && children[0].name == "Alpha" &&
                children[0].value == "50, 50%" && children[1].position_in_set == stxlib::Optional<std::size_t>::of(2U) &&
                children[1].set_size == stxlib::Optional<std::size_t>::of(3U),
            "each slice is a list item with its value and share");
    std::vector<std::string> activations;
    SubscriptionToken activated = (*chart).slice_activated().subscribe(RecordActivation{&activations});
    require((*chart).on_semantic_child_action("c", SemanticAction::press, {}) &&
                (*chart).selected_id() == stxlib::Optional<std::string_view>::of("c") &&
                activations == std::vector<std::string>({"c"}),
            "the press action selects and activates");
    require(!(*chart).on_semantic_child_action("missing", SemanticAction::select, {}) &&
                !(*chart).on_semantic_child_action("a", SemanticAction::expand, {}),
            "an unknown slice and an unsupported action are refused");
}

void refusals() {
    std::shared_ptr<PieChart> chart = make_control<PieChart>(StableId("chart"));
    (*chart).set_slices(sample_slices());
    bool refused = false;
    try {
        std::vector<PieSlice> bad = sample_slices();
        bad[1].value = -1.0;
        (*chart).set_slices(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*chart).slices().size() == 3U && (*chart).slices()[1].value == 25.0,
            "a negative value is refused and the old slices stay");
    refused = false;
    try {
        std::vector<PieSlice> bad = sample_slices();
        bad[2].stable_id = "a";
        (*chart).set_slices(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a repeated ID is refused");
    refused = false;
    try {
        (*chart).set_slice_value("a", std::nan(""));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a value that is not finite is refused");
    refused = false;
    try {
        (*chart).set_selected_id(stxlib::Optional<std::string_view>::of("missing"));
    } catch (const std::out_of_range&) {
        refused = true;
    }
    require(refused && (*chart).selected_id().absent(), "an unknown selection is refused");
    refused = false;
    try {
        (*chart).doughnut_radius = 95.0;
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*chart).doughnut_radius.read() == 0.0, "a hole past maximum_doughnut_radius is refused");
    refused = false;
    try {
        (*chart).start_angle = 360.0;
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a start angle of a whole turn is refused");
}

} // namespace

int main() {
    slices_and_shares();
    parts_and_hit_testing();
    selection_and_activation();
    semantics();
    refusals();
    std::cout << "PieChart: slices and shares, parts and hit testing, selection and activation, semantics and "
                 "refusals passed\n";
    return 0;
}
