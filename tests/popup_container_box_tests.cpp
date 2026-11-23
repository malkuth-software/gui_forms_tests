// PopupContainerBox: the popup opened with the application's content, the
// content's focus and its return, each way the popup closes, the keys,
// semantics and refusals.

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/popup_container_box.hpp"
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

struct RecordClosed final {
    std::vector<PopupCloseReason>* seen{};
    void operator()(const PopupClosed& closed) const { (*seen).push_back(closed.reason); }
};

struct CountOpened final {
    int* count{};
    void operator()() const { ++(*count); }
};

// A 400 by 300 window with the field at (20, 20), 200 by 24, and a button
// as the popup's content.
struct Fixture final {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<PopupContainerBox> field = make_control<PopupContainerBox>(StableId("root.due"));
    std::shared_ptr<Button> content = make_control<Button>(StableId("root.due.choose"), "Choose");
    Window window;
    std::vector<PopupCloseReason> closed;
    int opened{};
    SubscriptionToken closed_token;
    SubscriptionToken opened_token;

    Fixture() : window(root, {400.0, 300.0}) {
        (*field).set_requested_bounds(Rect{20.0, 20.0, 200.0, 24.0});
        (*root).add_child(field);
        (*field).set_popup_content(content);
        window.perform_layout();
        closed_token = (*field).popup_closed().subscribe(RecordClosed{&closed});
        opened_token = (*field).popup_opened().subscribe(CountOpened{&opened});
    }
};

void opening_and_closing() {
    std::shared_ptr<PopupContainerBox> loose = make_control<PopupContainerBox>(StableId("loose"));
    (*loose).set_popup_content(make_control<Button>(StableId("loose.content"), "Go"));
    require(!(*loose).show_popup() && !(*loose).popup_shown(), "outside a window the popup does not open");

    Fixture fixture;
    PopupContainerBox& field = *fixture.field;
    field.set_popup_size(Size{300.0, 120.0});
    require(fixture.window.request_focus(fixture.field), "the field takes the focus");
    require(field.show_popup() && field.popup_shown() && fixture.opened == 1 && (*fixture.content).parent(),
            "show_popup opens the popup with the content in it and raises popup_opened");
    require(!field.show_popup() && fixture.opened == 1, "a second show_popup is refused");
    require(fixture.window.focused_control() == fixture.content, "the content takes the focus");
    fixture.window.perform_layout();
    const Rect shown = (*fixture.content).absolute_bounds();
    require(shown.width == 300.0 && shown.height == 120.0 && shown.y >= 44.0,
            "the popup is the size given, below the field");
    require(field.anatomy_states().contains(AnatomyState::open), "the field is open while the popup is");
    require(field.close_popup(PopupCloseReason::accepted) && !field.popup_shown() &&
                fixture.closed == std::vector<PopupCloseReason>({PopupCloseReason::accepted}),
            "close_popup closes it and reports the reason");
    require(!(*fixture.content).parent() && fixture.window.focused_control() == fixture.field,
            "the content is given back and the focus returns to the field");
    require(!field.close_popup(PopupCloseReason::cancelled) && fixture.closed.size() == 1U,
            "closing a closed popup is refused");
    require(field.show_popup() && fixture.opened == 2, "the same content opens again");
    require(field.close_popup(PopupCloseReason::cancelled), "and closes again");
}

void dismissal_and_keys() {
    Fixture fixture;
    PopupContainerBox& field = *fixture.field;
    require(fixture.window.request_focus(fixture.field), "the field takes the focus");
    require(fixture.window.dispatch_key({KeyAction::down, PhysicalKey::f4}) && field.popup_shown(),
            "F4 opens the popup");
    require(fixture.window.dispatch_key({KeyAction::down, PhysicalKey::escape}) && !field.popup_shown() &&
                fixture.closed.back() == PopupCloseReason::escape,
            "Escape in the popup closes it as escape");
    require(fixture.window.dispatch_key({KeyAction::down, PhysicalKey::down, Modifier::alt}) && field.popup_shown(),
            "Alt+Down opens it");
    PointerEvent away{PointerAction::down, PointerButton::primary, Point{380.0, 290.0}};
    static_cast<void>(fixture.window.dispatch_pointer(away));
    require(!field.popup_shown() && fixture.closed.back() == PopupCloseReason::click_away,
            "a press away from the popup closes it as click_away");
    require(field.show_popup(), "the popup opens again");
    PointerEvent on_field{PointerAction::down, PointerButton::primary, Point{40.0, 30.0}};
    static_cast<void>(fixture.window.dispatch_pointer(on_field));
    require(!field.popup_shown() && fixture.closed.back() == PopupCloseReason::cancelled,
            "a press on the field cancels the open popup");
}

void semantics() {
    Fixture fixture;
    PopupContainerBox& field = *fixture.field;
    field.accessible_name = "Due date";
    field.text = std::string{"Tomorrow"};
    SemanticDescriptor descriptor = field.semantic_descriptor();
    require(descriptor.role == SemanticRole::combo_box && descriptor.name == "Due date" &&
                descriptor.value == "Tomorrow" && descriptor.has_popup &&
                descriptor.controlled_panel == "root.due.choose" &&
                !has_semantic_state(descriptor.states, SemanticState::expanded),
            "the field is a collapsed combo box with a pop-up controlling the content");
    require(field.on_semantic_action(SemanticAction::expand, {}) && field.popup_shown() &&
                has_semantic_state(field.semantic_descriptor().states, SemanticState::expanded),
            "the semantic expand opens the popup");
    require(field.on_semantic_action(SemanticAction::collapse, {}) && !field.popup_shown() &&
                fixture.closed.back() == PopupCloseReason::cancelled,
            "the semantic collapse cancels it");
}

void refusals() {
    Fixture fixture;
    PopupContainerBox& field = *fixture.field;
    require(field.show_popup(), "the popup opens");
    bool refused = false;
    try {
        field.set_popup_content(make_control<Button>(StableId("other"), "Other"));
    } catch (const std::logic_error&) {
        refused = true;
    }
    require(refused && field.popup_content() == fixture.content, "the content cannot change while open");
    require(field.close_popup(PopupCloseReason::cancelled), "the popup closes");
    refused = false;
    try {
        field.set_popup_content(fixture.root);
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a control in a window is refused as content");
    refused = false;
    try {
        field.set_popup_content(fixture.field);
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a control with a parent is refused as content");
    refused = false;
    try {
        field.set_popup_size(Size{-1.0, 10.0});
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a negative popup size is refused");
    refused = false;
    try {
        field.text = std::string{"\xff"};
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && field.text.read().empty(), "invalid UTF-8 text is refused");
    field.set_popup_content(nullptr);
    require(!field.show_popup(), "without content the popup does not open");
}

} // namespace

int main() {
    opening_and_closing();
    dismissal_and_keys();
    semantics();
    refusals();
    std::cout << "PopupContainerBox: opening and closing, dismissal and keys, semantics and refusals passed\n";
    return 0;
}
