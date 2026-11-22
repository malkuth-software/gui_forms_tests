// CheckedComboBox: items kept by ID, the field's text, checks from code and
// from the open list, opening and closing, semantics and refusals.

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/check_box_values.hpp"
#include "gui_forms/controls/checked_combo_box.hpp"
#include "gui_forms/controls/checked_list_box.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

struct RecordChange final {
    std::vector<std::string>* seen{};
    void operator()(const CheckedComboItemChange& change) const {
        (*seen).push_back((change.checked ? "+" : "-") + change.item_id);
    }
};

struct RecordDropDown final {
    std::vector<bool>* seen{};
    void operator()(bool open) const { (*seen).push_back(open); }
};

std::vector<CheckedComboItem> weekdays() {
    std::vector<CheckedComboItem> items;
    items.push_back({.stable_id = "mon", .text = "Monday", .checked = true});
    items.push_back({.stable_id = "tue", .text = "Tuesday", .checked = false});
    items.push_back({.stable_id = "wed", .text = "Wednesday", .checked = false});
    return items;
}

void items_and_text() {
    std::shared_ptr<CheckedComboBox> box = make_control<CheckedComboBox>(StableId("days"));
    (*box).set_items(weekdays());
    std::vector<std::string> changes;
    SubscriptionToken changed = (*box).item_check_changed().subscribe(RecordChange{&changes});
    require((*box).text() == "Monday" && (*box).checked_ids() == std::vector<std::string>({"mon"}),
            "the text and the checked IDs follow the checked items");
    (*box).set_item_checked("wed", true);
    require((*box).text() == "Monday, Wednesday" && (*box).item_checked("wed") &&
                changes == std::vector<std::string>({"+wed"}),
            "set_item_checked checks an item and raises item_check_changed");
    (*box).set_item_checked("wed", true);
    require(changes.size() == 1U, "an unchanged check raises nothing");
    (*box).separator = std::string{" / "};
    require((*box).text() == "Monday / Wednesday", "the separator joins the texts");
    (*box).set_all_checked(false);
    require((*box).text().empty() && (*box).checked_ids().empty() &&
                changes == std::vector<std::string>({"+wed", "-mon", "-wed"}),
            "set_all_checked unchecks every item, raising a change for each that changes");
    (*box).placeholder_text = std::string{"No days"};
    require((*box).semantic_descriptor().description == "No days", "the placeholder describes the empty field");
}

void the_open_list() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<CheckedComboBox> box = make_control<CheckedComboBox>(StableId("root.days"));
    (*box).set_requested_bounds(Rect{20.0, 20.0, 200.0, 26.0});
    (*root).add_child(box);
    (*box).set_items(weekdays());
    Window window(root, {400.0, 300.0});
    window.perform_layout();
    std::vector<std::string> changes;
    std::vector<bool> drops;
    SubscriptionToken changed = (*box).item_check_changed().subscribe(RecordChange{&changes});
    SubscriptionToken dropped = (*box).drop_down_changed().subscribe(RecordDropDown{&drops});
    require(window.request_focus(box), "the box takes the focus");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::f4}) && (*box).dropped_down() &&
                drops == std::vector<bool>({true}),
            "F4 opens the list and raises drop_down_changed");
    const std::shared_ptr<CheckedListBox> list = (*box).drop_down_list();
    require(list && (*list).items.read().size() == 3U && (*list).item_checked(0U) && !(*list).item_checked(1U) &&
                window.focused_control() == list,
            "the list shows the items with their checks and takes the focus");
    (*list).set_item_checked(1U, true);
    require((*box).item_checked("tue") && (*box).text() == "Monday, Tuesday" &&
                changes == std::vector<std::string>({"+tue"}),
            "a check in the list applies at once");
    (*box).set_item_checked("mon", false);
    require(!(*list).item_checked(0U) && changes.size() == 2U, "a check from code shows in the open list");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::escape}) && !(*box).dropped_down() &&
                !(*box).drop_down_list() && drops == std::vector<bool>({true, false}) &&
                window.focused_control() == box,
            "Escape closes the list and the focus returns to the box");
    require((*box).on_semantic_action(SemanticAction::expand, {}) && (*box).dropped_down() &&
                has_semantic_state((*box).semantic_descriptor().states, SemanticState::expanded),
            "the semantic expand opens the list");
    PointerEvent away{PointerAction::down, PointerButton::primary, Point{380.0, 290.0}};
    static_cast<void>(window.dispatch_pointer(away));
    require(!(*box).dropped_down(), "a press away closes the list");
    require((*box).show_drop_down(), "the list opens again");
    (*box).set_items(weekdays());
    require(!(*box).dropped_down() && (*box).text() == "Monday", "set_items closes the list and takes the new checks");
}

void semantics() {
    std::shared_ptr<CheckedComboBox> box = make_control<CheckedComboBox>(StableId("days"));
    (*box).accessible_name = "Repeat on";
    (*box).set_items(weekdays());
    const SemanticDescriptor descriptor = (*box).semantic_descriptor();
    require(descriptor.role == SemanticRole::combo_box && descriptor.name == "Repeat on" &&
                descriptor.value == "Monday" && descriptor.has_popup &&
                !has_semantic_state(descriptor.states, SemanticState::expanded),
            "the box is a collapsed combo box whose value is its text");
    require(!(*box).show_drop_down(), "outside a window the list does not open");
}

void refusals() {
    std::shared_ptr<CheckedComboBox> box = make_control<CheckedComboBox>(StableId("days"));
    (*box).set_items(weekdays());
    bool refused = false;
    try {
        std::vector<CheckedComboItem> bad = weekdays();
        bad[2].stable_id = "mon";
        (*box).set_items(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*box).items().size() == 3U && (*box).items()[2].stable_id == "wed",
            "a repeated ID is refused and the old items stay");
    refused = false;
    try {
        (*box).set_item_checked("sun", true);
    } catch (const std::out_of_range&) {
        refused = true;
    }
    require(refused, "an unknown item is refused");
    refused = false;
    try {
        (*box).max_drop_down_items = 0U;
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*box).max_drop_down_items.read() == 8U, "zero drop-down items are refused");
    refused = false;
    try {
        (*box).separator = std::string{"\xff"};
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*box).separator.read() == ", ", "an invalid separator is refused");
}

} // namespace

int main() {
    items_and_text();
    the_open_list();
    semantics();
    refusals();
    std::cout << "CheckedComboBox: items and text, the open list, semantics and refusals passed\n";
    return 0;
}
