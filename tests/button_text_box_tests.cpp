// ButtonTextBox: buttons kept by ID at either end of a text field, their
// placement and published parts, clicks, F4 and Alt+Down, the text views,
// semantics and refusals.

#include "gui_forms/anatomy/types/anatomy_types.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button_text_box.hpp"
#include "gui_forms/controls/text_box.hpp"
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
#include <utility>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

struct RecordClick final {
    std::vector<std::string>* seen{};
    void operator()(const EditButtonClick& click) const { (*seen).push_back(click.button_id); }
};

struct RecordText final {
    std::vector<std::string>* seen{};
    void operator()(const std::string& text) const { (*seen).push_back(text); }
};

EditButton button(std::string id, EditButtonKind kind, std::string caption,
                  EditButtonPlacement placement = EditButtonPlacement::trailing) {
    EditButton result;
    result.stable_id = std::move(id);
    result.kind = kind;
    result.caption = std::move(caption);
    result.placement = placement;
    return result;
}

// A browse button and a clear button (no caption) at the trailing end.
std::vector<EditButton> sample_buttons() {
    std::vector<EditButton> buttons;
    buttons.push_back(button("browse", EditButtonKind::ellipsis, "Browse"));
    buttons.push_back(button("clear", EditButtonKind::close, ""));
    return buttons;
}

Point centre(Rect bounds) {
    const Point point{bounds.x + bounds.width * 0.5, bounds.y + bounds.height * 0.5};
    return point;
}

void press_and_release(Window& window, const ButtonTextBox& field, Point down_local, Point up_local) {
    const Rect absolute = field.absolute_bounds();
    PointerEvent down{PointerAction::down, PointerButton::primary,
                      Point{absolute.x + down_local.x, absolute.y + down_local.y}};
    static_cast<void>(window.dispatch_pointer(down));
    PointerEvent up{PointerAction::up, PointerButton::primary, Point{absolute.x + up_local.x, absolute.y + up_local.y}};
    static_cast<void>(window.dispatch_pointer(up));
}

// In a 300 by 30 field each mark face is 24 square, inset 2 inside the
// frame; the trailing faces run from the text out, so the browse face is
// at 247 and the clear face at 273, and the editor ends at 245.
void placement_and_parts() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    (*field).set_buttons(sample_buttons());
    Window window(field, {300.0, 30.0});
    window.perform_layout();
    const std::vector<ButtonTextBox::RealizedButton> buttons = (*field).realized_buttons();
    require(buttons.size() == 2U && buttons[0].stable_id == "browse" && buttons[1].stable_id == "clear",
            "every visible button is a realized item, in order");
    require(buttons[0].parts.button.present && buttons[0].parts.button.bounds.x == 247.0 &&
                buttons[0].parts.button.bounds.width == 24.0 && buttons[1].parts.button.bounds.x == 273.0,
            "the trailing faces run from the text out");
    require(buttons[0].parts.button_glyph.present && !buttons[0].parts.button_label.present,
            "a mark button publishes its glyph and no label");
    const ButtonTextBox::Parts parts = (*field).parts();
    require(parts.well.present && parts.content.present && parts.content.bounds.x == 5.0 &&
                parts.content.bounds.x + parts.content.bounds.width == 245.0,
            "the editor fills the room the buttons leave");
    require((*field).button_at(centre(buttons[1].parts.button.bounds)) == stxlib::Optional<std::size_t>::of(1U) &&
                (*field).button_at(Point{20.0, 15.0}).absent(),
            "button_at finds the face under a point");

    std::vector<EditButton> both = sample_buttons();
    both.push_back(button("unit", EditButtonKind::caption, "Metres", EditButtonPlacement::leading));
    (*field).set_buttons(std::move(both));
    window.perform_layout();
    const std::vector<ButtonTextBox::RealizedButton> placed = (*field).realized_buttons();
    require(placed.size() == 3U && placed[0].stable_id == "unit" && placed[0].parts.button.bounds.x == 3.0 &&
                placed[0].parts.button.bounds.width >= 24.0 && placed[0].parts.button_label.present &&
                !placed[0].parts.button_glyph.present,
            "a leading caption button sits at the start, as wide as its caption");
    require((*field).parts().content.bounds.x > placed[0].parts.button.bounds.x + placed[0].parts.button.bounds.width,
            "the editor starts after the leading button");

    (*field).set_button_visible("browse", false);
    window.perform_layout();
    require((*field).realized_buttons().size() == 2U && (*field).realized_item_count() == 2U,
            "a hidden button takes no room and is not published");
}

void clicks() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    (*field).set_buttons(sample_buttons());
    Window window(field, {300.0, 30.0});
    window.perform_layout();
    std::vector<std::string> seen;
    SubscriptionToken clicked = (*field).button_clicked().subscribe(RecordClick{&seen});
    const Point browse = centre((*field).realized_buttons()[0].parts.button.bounds);
    press_and_release(window, *field, browse, browse);
    require(seen == std::vector<std::string>({"browse"}), "a click on a button raises button_clicked");
    require(window.focused_control() == (*field).editor(), "a press on a button gives the editor the focus");
    press_and_release(window, *field, browse, Point{20.0, 15.0});
    require(seen.size() == 1U, "a press released off the button does not act");
    (*field).set_button_enabled("browse", false);
    press_and_release(window, *field, browse, browse);
    require(seen.size() == 1U && (*field).realized_buttons()[0].states.disabled,
            "a disabled button does not act and publishes disabled");
    require(!(*field).perform_button_click("browse") && (*field).perform_button_click("clear") &&
                seen == std::vector<std::string>({"browse", "clear"}),
            "perform_button_click acts as a click, and refuses a disabled button");
}

void keys() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    std::vector<EditButton> buttons = sample_buttons();
    buttons.insert(buttons.begin(), button("unit", EditButtonKind::caption, "Metres", EditButtonPlacement::leading));
    (*field).set_buttons(std::move(buttons));
    Window window(field, {300.0, 30.0});
    window.perform_layout();
    std::vector<std::string> seen;
    SubscriptionToken clicked = (*field).button_clicked().subscribe(RecordClick{&seen});
    require(window.request_focus((*field).editor()), "the editor takes the focus");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::f4}) && seen == std::vector<std::string>({"browse"}),
            "F4 acts on the first enabled trailing button");
    require(window.dispatch_key({KeyAction::down, PhysicalKey::down, Modifier::alt}) && seen.size() == 2U &&
                seen[1] == "browse",
            "Alt+Down acts on it too");
    (*field).set_button_visible("browse", false);
    (*field).set_button_enabled("clear", false);
    static_cast<void>(window.dispatch_key({KeyAction::down, PhysicalKey::f4}));
    require(seen.size() == 3U && seen[2] == "unit",
            "with no trailing button able to act, F4 acts on the first enabled button");
}

void text_views() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    std::vector<std::string> texts;
    SubscriptionToken changed = (*field).text_changed().subscribe(RecordText{&texts});
    (*field).text = std::string{"Documents"};
    (*field).placeholder_text = std::string{"Folder"};
    (*field).read_only = true;
    require((*(*field).editor()).text.read() == "Documents" && (*field).text.read() == "Documents" &&
                texts == std::vector<std::string>({"Documents"}),
            "the text is a view onto the editor's and raises text_changed");
    require((*(*field).editor()).placeholder_text.read() == "Folder" && (*(*field).editor()).read_only.read() &&
                (*field).read_only.read(),
            "the placeholder and read_only are views onto the editor's");
    require(!(*field).on_semantic_action(SemanticAction::set_value, "Other") && (*field).text.read() == "Documents",
            "a read-only field refuses a semantic set value");
}

void semantics() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    (*field).accessible_name = "Folder";
    std::vector<EditButton> buttons = sample_buttons();
    buttons[0].description = "Choose a folder";
    (*field).set_buttons(std::move(buttons));
    Window window(field, {300.0, 30.0});
    window.perform_layout();
    (*field).text = std::string{"Documents"};
    const SemanticDescriptor descriptor = (*field).semantic_descriptor();
    require(descriptor.role == SemanticRole::text_box && descriptor.name == "Folder" &&
                descriptor.value == "Documents" && !descriptor.include_descendants,
            "the field is one named text box whose value is the text");
    const std::vector<SemanticNode> children = (*field).semantic_virtual_children();
    require(children.size() == 2U && children[0].role == SemanticRole::button && children[0].name == "Browse" &&
                children[0].description == "Choose a folder" && children[0].has_popup &&
                children[1].name == "Clear" && !children[1].has_popup,
            "each button is a button node, a mark button without a caption named after its kind");
    std::vector<std::string> seen;
    SubscriptionToken clicked = (*field).button_clicked().subscribe(RecordClick{&seen});
    require((*field).on_semantic_child_action("clear", SemanticAction::press, {}) &&
                seen == std::vector<std::string>({"clear"}),
            "the press action raises button_clicked");
    (*field).set_button_enabled("clear", false);
    require(!(*field).on_semantic_child_action("clear", SemanticAction::press, {}) &&
                !(*field).on_semantic_child_action("missing", SemanticAction::press, {}) &&
                !(*field).on_semantic_child_action("browse", SemanticAction::expand, {}) && seen.size() == 1U,
            "a disabled or unknown button and an unsupported action are refused");
}

void refusals() {
    std::shared_ptr<ButtonTextBox> field = make_control<ButtonTextBox>(StableId("folder"));
    (*field).set_buttons(sample_buttons());
    bool refused = false;
    try {
        std::vector<EditButton> bad = sample_buttons();
        bad[1].stable_id = "browse";
        (*field).set_buttons(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && (*field).buttons().size() == 2U && (*field).buttons()[1].stable_id == "clear",
            "a repeated ID is refused and the old buttons stay");
    refused = false;
    try {
        std::vector<EditButton> bad = sample_buttons();
        bad[0].stable_id = "";
        (*field).set_buttons(std::move(bad));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "an empty ID is refused");
    refused = false;
    try {
        std::vector<EditButton> many;
        for (std::size_t index = 0; index <= ButtonTextBox::maximum_buttons; ++index) {
            many.push_back(button("b" + std::to_string(index), EditButtonKind::ellipsis, ""));
        }
        (*field).set_buttons(std::move(many));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "more than maximum_buttons buttons are refused");
    refused = false;
    try {
        (*field).set_button_enabled("missing", false);
    } catch (const std::out_of_range&) {
        refused = true;
    }
    require(refused, "an unknown button is refused");
    refused = false;
    try {
        static_cast<void>((*field).perform_button_click("missing"));
    } catch (const std::out_of_range&) {
        refused = true;
    }
    require(refused, "perform_button_click refuses an unknown button");
}

} // namespace

int main() {
    placement_and_parts();
    clicks();
    keys();
    text_views();
    semantics();
    refusals();
    std::cout << "ButtonTextBox: placement and parts, clicks, keys, text views, semantics and refusals passed\n";
    return 0;
}
