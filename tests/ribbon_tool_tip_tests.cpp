// RibbonToolTip (ribbon phase 4): enhanced tooltips on a control and on
// ToolStrip items, resolved from their commands; the disabled reason; hover
// delay and item changes; keyboard-initiated display as focus moves along a
// strip; F1 reporting the help topic; a semantic, input-transparent overlay.

#include "gui_forms/binding/property_type/property_type.hpp"
#include "gui_forms/commands/command/command.hpp"
#include "gui_forms/components/ribbon_tool_tip/ribbon_tool_tip.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/tool_strip.hpp"
#include "gui_forms/dirty.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/scheduler/types/scheduler_types.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/text/markdown/markdown.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/types/paint_types/paint_types.hpp"
#include "gui_forms/window/tool_tips/tool_tips.hpp"
#include "gui_forms/window/window.hpp"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace gui_forms;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

const SemanticNode* find_role(const std::vector<SemanticNode>& nodes, SemanticRole role) {
    for (const SemanticNode& node : nodes) {
        if (node.role == role) return &node;
        if (const SemanticNode* found = find_role(node.children, role)) return found;
    }
    return nullptr;
}
const SemanticNode* find_role(std::vector<SemanticNode>&& nodes, SemanticRole role) = delete;

void fire_next(Window& window) {
    const stxlib::Optional<FrameTime> wake = window.next_wake();
    require(wake.present(), "a UI deadline is scheduled");
    static_cast<void>(window.poll_frame_schedule(*wake.value()));
}

void hover(Window& window, Point point) {
    static_cast<void>(window.dispatch_pointer({PointerAction::move, PointerButton::none, point}));
}

Point centre(Rect bounds) {
    const Point point{bounds.x + bounds.width * 0.5, bounds.y + bounds.height * 0.5};
    return point;
}

struct RecordVisibility final {
    std::vector<RibbonToolTipEvent>* seen{};
    void operator()(const RibbonToolTipEvent& event) const { (*seen).push_back(event); }
};

// Clears every mapping when a tooltip hides.
struct ClearOnHide final {
    RibbonToolTip* tips{};
    void operator()(const RibbonToolTipEvent& event) const {
        if (!event.shown) (*tips).clear();
    }
};

struct RecordHelp final {
    std::vector<RibbonToolTipHelpRequest>* seen{};
    void operator()(const RibbonToolTipHelpRequest& request) const { (*seen).push_back(request); }
};

struct Fixture final {
    Fixture() : window(root, {480.0, 260.0}) {
        (*bold).set_description("Make the selected text bold.");
        (*bold).set_shortcut("Ctrl+B");
        (*paste).set_description("Add content from the Clipboard.");
        (*paste).set_shortcut("Ctrl+V");
        (*paste).set_availability_reason("The Clipboard is empty.");
        (*paste).set_enabled(false);
        (*strip).set_items({
            {.stable_id = "bold", .command = bold},
            {.stable_id = "paste", .command = paste},
            {.stable_id = "plain", .text = "Plain"},
        });
        (*root).set_requested_bounds({0.0, 0.0, 480.0, 260.0});
        (*strip).set_requested_bounds({0.0, 0.0, 480.0, 25.0});
        (*button).set_requested_bounds({40.0, 120.0, 140.0, 30.0});
        (*root).add_child(strip);
        (*root).add_child(button);
        window.perform_layout();
    }

    std::shared_ptr<Command> bold = std::make_shared<Command>("bold", "Bold");
    std::shared_ptr<Command> paste = std::make_shared<Command>("paste", "Paste");
    Control::Ptr root = make_control<Control>(StableId("root"));
    std::shared_ptr<ToolStrip> strip = make_control<ToolStrip>(StableId("strip"));
    std::shared_ptr<Button> button = make_control<Button>(StableId("button"), "Format Painter");
    Window window;
};

Point item_point(const ToolStrip& strip, std::string_view id) {
    for (const ToolStrip::Item& item : strip.realized_items()) {
        if (item.stable_id == id) return centre(item.parts.item_face.bounds);
    }
    require(false, "the item is laid out");
    return {};
}

void content_from_commands_and_hover() {
    Fixture fixture;
    RibbonToolTip tips(fixture.window);
    tips.initial_delay = 30ms;
    tips.auto_pop_delay = 0ms;
    tips.set_item_tool_tip(fixture.strip, "bold", {.help_topic = "format.bold"});
    tips.set_item_tool_tip(fixture.strip, "paste", {.image = ImageId{7U}});
    std::vector<RibbonToolTipEvent> events;
    SubscriptionToken changes = tips.visibility_changed().subscribe(RecordVisibility{&events});
    require(tips.has_tool_tip(*fixture.strip, "bold") && !tips.has_tool_tip(*fixture.strip, "plain"),
            "item mappings are per item");

    hover(fixture.window, item_point(*fixture.strip, "bold"));
    require(!tips.visible() && fixture.window.next_wake().present(), "hover arms the initial delay");
    fire_next(fixture.window);
    fixture.window.perform_layout();
    const RibbonToolTipContent& bold = tips.shown_content();
    require(tips.visible() && tips.active_item_id() == "bold" && bold.title == "Bold" &&
                bold.description == "Make the selected text bold." && bold.shortcut == "Ctrl+B" &&
                bold.disabled_reason.empty() && bold.help_topic == "format.bold",
            "an item's tooltip takes its title, description and shortcut from its command");
    require(events.size() == 1U && events[0].shown && events[0].item_id == "bold" && !events[0].keyboard_initiated,
            "showing reports the item, pointer-initiated");
    const SemanticSnapshot snapshot = fixture.window.semantic_snapshot();
    const SemanticNode* tooltip = find_role(snapshot.roots, SemanticRole::tool_tip);
    require(tooltip != nullptr && (*tooltip).name == "Bold" && (*tooltip).description == "Make the selected text bold.",
            "the overlay is a named tool tip to accessibility");
    const Rect overlay = tips.overlay_bounds();
    require(overlay.y >= 25.0 && overlay.width <= 330.0 && fixture.window.hit_test(centre(overlay)) == fixture.root,
            "the overlay opens below the strip, within its maximum width, and lets input through");

    // Moving to a disabled item replaces the tooltip after the delay.
    hover(fixture.window, item_point(*fixture.strip, "paste"));
    require(!tips.visible() && events.size() == 2U && !events[1].shown, "moving to another item hides the tooltip");
    fire_next(fixture.window);
    const RibbonToolTipContent& paste = tips.shown_content();
    require(tips.visible() && tips.active_item_id() == "paste" && paste.disabled_reason == "The Clipboard is empty." &&
                paste.image.value == 7U,
            "a disabled command's tooltip shows its availability reason and the image");

    // An item without a mapping shows nothing; leaving hides.
    hover(fixture.window, item_point(*fixture.strip, "plain"));
    require(!tips.visible() && fixture.window.next_wake().absent(), "an unmapped item shows nothing");
    static_cast<void>(fixture.window.dispatch_pointer({PointerAction::leave, PointerButton::none, {470.0, 250.0}}));
    require(!tips.visible(), "leaving hides");
}

void keyboard_and_help() {
    Fixture fixture;
    RibbonToolTip tips(fixture.window);
    tips.initial_delay = 1ms;
    tips.auto_pop_delay = 0ms;
    tips.set_item_tool_tip(fixture.strip, "bold", {.help_topic = "format.bold"});
    tips.set_item_tool_tip(fixture.strip, "paste", {});
    tips.set_tool_tip(fixture.button, {.title = "Format Painter", .description = "Copy formatting.",
                                       .help_topic = "format.painter"});
    std::vector<RibbonToolTipEvent> events;
    std::vector<RibbonToolTipHelpRequest> help;
    SubscriptionToken changes = tips.visibility_changed().subscribe(RecordVisibility{&events});
    SubscriptionToken requests = tips.help_requested().subscribe(RecordHelp{&help});

    // Focus reaches the strip: its first item's tooltip shows,
    // keyboard-initiated; Right moves it to the next item.
    require(fixture.window.request_focus(fixture.strip), "the strip takes focus");
    fire_next(fixture.window);
    require(tips.visible() && tips.active_item_id() == "bold" && events.back().keyboard_initiated,
            "keyboard focus on a strip shows its focused item's tooltip");
    KeyEvent right{KeyAction::down, PhysicalKey::right};
    require(fixture.window.dispatch_key(right), "Right moves along the strip");
    require(!tips.visible(), "moving focus hides the previous item's tooltip");
    // Paste is disabled, so Right goes past it to Plain, which has no
    // mapping; the tooltip stays hidden.
    require(!tips.visible() && fixture.window.next_wake().absent(), "an unmapped item shows nothing");

    // F1 while a tooltip with a help topic shows reports the topic.
    tips.show(fixture.strip, "bold");
    KeyEvent f1{KeyAction::down, PhysicalKey::f1};
    require(fixture.window.dispatch_key(f1) && help.size() == 1U && help[0].help_topic == "format.bold" &&
                help[0].item_id == "bold" && help[0].target_stable_id == "strip" && !tips.visible(),
            "F1 reports the help topic and closes the tooltip");
    tips.show(fixture.strip, "paste");
    static_cast<void>(fixture.window.dispatch_key(f1));
    require(help.size() == 1U, "F1 with a tooltip that has no help topic reports nothing");
    tips.hide();
    static_cast<void>(fixture.window.dispatch_key(f1));
    require(help.size() == 1U, "F1 with no tooltip reports nothing");

    // A whole control: focus shows it.
    require(fixture.window.request_focus(fixture.button), "the button takes focus");
    fire_next(fixture.window);
    require(tips.visible() && tips.active_control() == fixture.button && tips.active_item_id().empty() &&
                tips.shown_content().title == "Format Painter",
            "keyboard focus on a mapped control shows its tooltip");
    require(fixture.window.dispatch_key(f1) && help.size() == 2U && help[1].help_topic == "format.painter",
            "F1 reports a control's help topic");
}

void validation_and_removal() {
    Fixture fixture;
    RibbonToolTip tips(fixture.window);
    bool threw = false;
    try {
        tips.set_item_tool_tip(fixture.strip, "missing", {.title = "Missing"});
    } catch (const std::out_of_range&) {
        threw = true;
    }
    require(threw, "an item that is not on the strip is refused");
    threw = false;
    try {
        tips.set_tool_tip(fixture.button, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "content needs a title or a command");
    std::shared_ptr<Button> detached = make_control<Button>(StableId("detached"), "Detached");
    threw = false;
    try {
        tips.set_tool_tip(detached, {.title = "Detached"});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "a detached control is refused");

    tips.set_tool_tip(fixture.button, {.title = "Format Painter"});
    tips.show(fixture.button);
    require(tips.visible(), "an explicit show opens the tooltip");
    require(tips.remove_tool_tip(*fixture.button) && !tips.visible() && !tips.has_tool_tip(*fixture.button),
            "removing the shown mapping hides it");
    require(!tips.remove_tool_tip(*fixture.button), "removing twice reports nothing removed");
    tips.set_item_tool_tip(fixture.strip, "bold", {});
    tips.clear();
    require(!tips.has_tool_tip(*fixture.strip, "bold"), "clear removes every mapping");

    // A visibility_changed handler that clears the mappings while the shown
    // one is removed (A-gf-controls-12-F5; meaningful under the sanitizer
    // tree).
    tips.set_tool_tip(fixture.button, {.title = "Format Painter"});
    tips.show(fixture.button);
    SubscriptionToken clearing = tips.visibility_changed().subscribe(ClearOnHide{&tips});
    require(tips.remove_tool_tip(*fixture.button) && !tips.visible() && !tips.has_tool_tip(*fixture.button),
            "a removal whose hide handler clears the mappings completes");
}

// A mapping without a title takes the thing's plain tooltip as its title,
// and while it exists the window's plain presenter shows nothing for that
// thing; removing it hands the thing back to the plain presenter.
void plain_text_and_covering() {
    Fixture fixture;
    (*fixture.button).tool_tip_text = "Copy formatting to other text.";
    ToolTipSettings settings = fixture.window.tool_tip_settings();
    settings.initial_delay = 1ms;
    settings.auto_pop_delay = 0ms;
    fixture.window.set_tool_tip_settings(settings);
    RibbonToolTip tips(fixture.window);
    tips.initial_delay = 1ms;
    tips.auto_pop_delay = 0ms;
    tips.set_tool_tip(fixture.button, {.description = "Apply the look of one selection to another."});
    require(tips.has_tool_tip(*fixture.button), "a plain tooltip stands in for a title");

    const Point over_button = centre((*fixture.button).absolute_bounds());
    hover(fixture.window, over_button);
    while (!tips.visible() && fixture.window.next_wake().present()) fire_next(fixture.window);
    require(tips.visible() && tips.shown_content().title == "Copy formatting to other text.",
            "the enhanced tooltip's title is the plain text");
    require(!fixture.window.tool_tip_visible(), "the plain presenter shows nothing for a covered control");

    require(tips.remove_tool_tip(*fixture.button), "the mapping is removed");
    hover(fixture.window, {400.0, 220.0});
    hover(fixture.window, over_button);
    fire_next(fixture.window);
    require(fixture.window.tool_tip_visible() && fixture.window.tool_tip_text() == "Copy formatting to other text.",
            "an uncovered control shows its plain tooltip again");
}

} // namespace

// Text of exactly `length` characters: plain English words, repeated.
std::string help_text(std::size_t length) {
    const std::string description =
        "Copies the formatting of the selection and applies it to the next text you click or drag across. ";
    std::string text;
    while (text.size() < length) text.append(description);
    text.resize(length);
    if (text.back() == ' ') text.back() = 'x';
    return text;
}

// The overlay `tips` shows on the fixture's button for `description`.
Rect shown_overlay(Fixture& fixture, RibbonToolTip& tips, const std::string& description) {
    tips.set_tool_tip(fixture.button, {.title = "Format Painter", .description = description});
    tips.show(fixture.button);
    fixture.window.perform_layout();
    require(tips.visible(), "the tooltip shows");
    const Rect overlay = tips.overlay_bounds();
    tips.hide();
    return overlay;
}

// Owner, 2026-11-22 (decision 7): one rule sizes every tooltip, ribbon
// tooltips included; the fixed 318-unit bubble is gone. A one-character
// and a 40-character description stay on one line; a 400-character one
// wraps to a box near the golden ratio.
void width_follows_the_description() {
    Fixture fixture;
    RibbonToolTip tips(fixture.window);
    const Rect short_box = shown_overlay(fixture, tips, help_text(1U));
    const Rect forty_box = shown_overlay(fixture, tips, help_text(40U));
    const Rect long_box = shown_overlay(fixture, tips, help_text(400U));
    require(short_box.width > 0.0 && forty_box.height == short_box.height,
            "a 40-character description stays on one line");
    require(forty_box.width < 318.0, "a short description makes a box narrower than the old fixed width");
    const double aspect = long_box.width / long_box.height;
    require(aspect > 1.618 / 1.25 && aspect < 1.618 * 1.25, "a 400-character description wraps near the golden ratio");
    require(long_box.width <= fixture.window.client_size().width, "the box stays inside the client");
}

int main() {
    content_from_commands_and_hover();
    width_follows_the_description();
    keyboard_and_help();
    validation_and_removal();
    plain_text_and_covering();
    std::cout << "RibbonToolTip: command content, disabled reason, hover, keyboard, F1 help, validation, plain text and covering passed\n";
    return 0;
}
