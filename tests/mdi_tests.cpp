// The multiple document interface (DK decisions 60 to 63): Window's WinForms
// MDI calls over a bound document area (a fake area, then the docking
// workspace), the main DockSite's document frames (realization, stacking,
// activation, the caption buttons, moving and resizing by the pointer, the
// LayoutMdi arrangements, Ctrl+F4 and Ctrl+F6, the presentation switched
// back to tabs), a maximized frame's buttons on the main menu strip, the
// panels' menu strips merging into it, and its window list.

#include "gui_forms/components/context_menu/context_menu.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/dock_site.hpp"
#include "gui_forms/controls/dock_workspace.hpp"
#include "gui_forms/controls/menu_strip.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/docking/dock_frames/dock_frames.hpp"
#include "gui_forms/docking/dock_layout/dock_layout.hpp"
#include "gui_forms/docking/types/dock_types.hpp"
#include "gui_forms/event/delegate/delegate.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/document_area/window_document_area.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stxlib/optional.hpp>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

bool near(double left, double right) {
    const bool close = std::abs(left - right) < 1e-6;
    return close;
}

bool is(const stxlib::Optional<DockPanelId>& held, std::string_view panel) {
    const DockPanelId* const value = held.value();
    const bool matches = value != nullptr && (*value).value == panel;
    return matches;
}

const SemanticNode* find_node(const std::vector<SemanticNode>& nodes, std::string_view stable_id) {
    for (const SemanticNode& node : nodes) {
        if (node.stable_id == stable_id) return &node;
        if (const SemanticNode* nested = find_node(node.children, stable_id)) return nested;
    }
    return nullptr;
}
const SemanticNode* find_node(std::vector<SemanticNode>&& nodes, std::string_view stable_id) = delete;

const SemanticNode* find_named(const std::vector<SemanticNode>& nodes, std::string_view name) {
    for (const SemanticNode& node : nodes) {
        if (node.name == name) return &node;
        if (const SemanticNode* nested = find_named(node.children, name)) return nested;
    }
    return nullptr;
}
const SemanticNode* find_named(std::vector<SemanticNode>&& nodes, std::string_view name) = delete;

bool key(Window& window, std::uint32_t physical_key, Modifier modifiers = Modifier::none) {
    KeyEvent event{KeyAction::down, physical_key};
    event.modifiers = modifiers;
    const bool handled = window.dispatch_key(event);
    return handled;
}

void key_up(Window& window, std::uint32_t physical_key, Modifier modifiers = Modifier::none) {
    KeyEvent event{KeyAction::up, physical_key};
    event.modifiers = modifiers;
    static_cast<void>(window.dispatch_key(event));
}

void press(Window& window, Point point, std::uint32_t clicks = 1U) {
    PointerEvent event{PointerAction::down, PointerButton::primary, point};
    event.click_count = clicks;
    static_cast<void>(window.dispatch_pointer(event));
}

void move_to(Window& window, Point point) {
    static_cast<void>(window.dispatch_pointer({PointerAction::move, PointerButton::primary, point}));
}

void release(Window& window, Point point) {
    static_cast<void>(window.dispatch_pointer({PointerAction::up, PointerButton::primary, point}));
}

std::vector<DockPanelId> children_of(const Window& window) {
    const BorrowedSpan<DockPanelId> children = window.mdi_children();
    std::vector<DockPanelId> copied{};
    for (std::size_t index = 0; index < children.size(); ++index) copied.push_back(children[index]);
    return copied;
}

// -- the window's facade over a fake area ---------------------------------------

// A document area of three documents that records what the window asks.
class FakeArea final : public WindowDocumentArea {
public:
    [[nodiscard]] bool document_frames_shown() const override { return frames; }
    void show_document_frames(bool shown) override {
        frames = shown;
        ++shows;
    }
    [[nodiscard]] std::size_t document_count() const override { return documents.size(); }
    [[nodiscard]] DockPanelId document_at(std::size_t index) const override {
        if (index >= documents.size()) return DockPanelId{};
        return documents[index];
    }
    [[nodiscard]] BorrowedText document_title(const DockPanelId& document) const override {
        for (const DockPanelId& held : documents) {
            if (held == document) return detail::lend_text(held.value, titles_);
        }
        return BorrowedText{};
    }
    [[nodiscard]] stxlib::Optional<DockPanelId> active_document() const override {
        stxlib::Optional<DockPanelId> held{};
        if (!active.empty()) held.set(DockPanelId(active));
        return held;
    }
    bool activate_document(const DockPanelId& document) override {
        for (const DockPanelId& held : documents) {
            if (!(held == document)) continue;
            MdiChildActivation activation{};
            if (!active.empty()) activation.previous.set(DockPanelId(active));
            active = document.value;
            activation.current.set(DockPanelId(active));
            raise_document_activated(activation);
            return true;
        }
        return false;
    }
    bool arrange_documents(MdiLayout layout) override {
        arranged.push_back(layout);
        return frames;
    }
    void set_icons_at_bottom(bool bottom) override { icons_at_bottom = bottom; }

    std::vector<DockPanelId> documents{DockPanelId("one"), DockPanelId("two"), DockPanelId("three")};
    std::string active{};
    bool frames{};
    int shows{};
    bool icons_at_bottom{true};
    std::vector<MdiLayout> arranged{};

private:
    detail::BorrowLender titles_{};
};

struct ActivationLog final {
    std::vector<std::string> previous{};
    std::vector<std::string> current{};
};

struct RecordActivation final {
    ActivationLog* log{};
    void operator()(const MdiChildActivation& activation) const {
        const DockPanelId* const before = activation.previous.value();
        const DockPanelId* const after = activation.current.value();
        (*log).previous.push_back(before != nullptr ? (*before).value : std::string{});
        (*log).current.push_back(after != nullptr ? (*after).value : std::string{});
    }
};

void window_facade_over_a_fake_area() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    Window window(root, {400.0, 300.0});
    // Unbound, the window keeps its settings and the calls change nothing.
    require(!window.is_mdi_container() && window.document_area() == nullptr && window.mdi_children().size() == 0U &&
                window.active_mdi_child().absent(),
            "an unbound window has no children and is no container");
    require(!window.activate_mdi_child(DockPanelId("one")) && !window.layout_mdi(MdiLayout::cascade),
            "unbound, activating and arranging answer false");
    bool threw = false;
    try {
        static_cast<void>(window.layout_mdi(static_cast<MdiLayout>(9)));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "layout_mdi refuses a value outside MdiLayout");
    window.set_is_mdi_container(true);
    window.set_mdi_children_minimized_anchor_bottom(false);
    require(window.is_mdi_container() && !window.mdi_children_minimized_anchor_bottom(),
            "the written settings are kept while unbound");

    ActivationLog log{};
    const SubscriptionToken token = window.mdi_child_activate().subscribe(RecordActivation{&log});
    {
        FakeArea area{};
        window.bind_document_area(area);
        require(window.document_area() == &area && area.bound_window() == &window,
                "binding ties the window and the area together");
        require(area.frames && area.shows == 1 && !area.icons_at_bottom,
                "binding gives the area the settings written before it came");
        const std::vector<DockPanelId> children = children_of(window);
        require(children.size() == 3U && children[0].value == "one" && children[2].value == "three",
                "the children are the area's documents in order");
        require(window.activate_mdi_child(DockPanelId("two")) && is(window.active_mdi_child(), "two"),
                "activating a child goes through the area");
        require(log.current.size() == 1U && log.previous[0].empty() && log.current[0] == "two",
                "the area's activation raises mdi_child_activate with the old and new child");
        require(!window.activate_mdi_child(DockPanelId("four")), "a document the area does not hold is refused");
        require(window.layout_mdi(MdiLayout::tile_vertical) && area.arranged.size() == 1U &&
                    area.arranged[0] == MdiLayout::tile_vertical,
                "layout_mdi asks the area to arrange");
        window.set_is_mdi_container(false);
        require(!area.frames && !window.is_mdi_container() && !window.layout_mdi(MdiLayout::cascade),
                "bound, the container setting is the area's presentation");
        // A second area replaces the first.
        FakeArea other{};
        window.bind_document_area(other);
        require(window.document_area() == &other && area.bound_window() == nullptr && other.bound_window() == &window,
                "binding another area unbinds the first");
        window.unbind_document_area(area);
        require(window.document_area() == &other, "unbinding an area the window does not hold changes nothing");
    }
    require(window.document_area() == nullptr && window.mdi_children().size() == 0U,
            "an area that goes unbinds itself");
    // The window going first leaves the area unbound.
    FakeArea survivor{};
    {
        std::shared_ptr<Panel> other_root = make_control<Panel>(StableId("other-root"));
        Window short_lived(other_root, {200.0, 100.0});
        short_lived.bind_document_area(survivor);
        require(survivor.bound_window() == &short_lived, "the survivor is bound");
    }
    require(survivor.bound_window() == nullptr, "a window that goes unbinds its area");
    // The main menu strip is held weakly.
    {
        std::shared_ptr<MenuStrip> strip = make_control<MenuStrip>(StableId("menu"));
        window.set_main_menu_strip(strip);
        require(window.main_menu_strip() == strip, "the main menu strip is kept");
    }
    require(!window.main_menu_strip(), "a main menu strip that goes is gone");
}

// -- the docking workspace as the document area ----------------------------------

// Builds each document's content: a panel holding a text box
// (`content.<id>` and `content.<id>.edit`).
class DocumentFactory final {
public:
    void create(DockPanelCreation& creation) {
        made.push_back(creation.panel.value);
        const std::string id = "content." + creation.panel.value;
        std::shared_ptr<Panel> panel = make_control<Panel>(StableId(id));
        std::shared_ptr<TextBox> edit = make_control<TextBox>(StableId(id + ".edit"));
        (*edit).accessible_name = creation.panel.value;
        (*edit).set_requested_bounds({6.0, 6.0, 80.0, 22.0});
        (*panel).add_child(edit);
        creation.content = panel;
    }
    std::vector<std::string> made{};
};

DockPanelRegistration document(DocumentFactory& factory, std::string id, std::string title) {
    DockPanelRegistration registration{};
    registration.descriptor.id = DockPanelId(std::move(id));
    registration.descriptor.kind = DockPanelKind::document;
    registration.descriptor.minimum_size = {120.0, 80.0};
    registration.title = std::move(title);
    registration.factory = Delegate<DockPanelCreation&>::bind<DocumentFactory, &DocumentFactory::create>(factory);
    return registration;
}

DockPanelRegistration tool(DocumentFactory& factory, std::string id, std::string title) {
    DockPanelRegistration registration{};
    registration.descriptor.id = DockPanelId(std::move(id));
    registration.descriptor.kind = DockPanelKind::tool;
    registration.descriptor.minimum_size = {120.0, 80.0};
    registration.descriptor.default_placement.side = DockSide::left;
    registration.descriptor.default_placement.ratio = 0.25;
    registration.title = std::move(title);
    registration.factory = Delegate<DockPanelCreation&>::bind<DocumentFactory, &DocumentFactory::create>(factory);
    return registration;
}

void apply(DockWorkspace& workspace, DockEdit edit) {
    DockTransaction transaction{};
    transaction.edits.push_back(std::move(edit));
    require(workspace.apply(transaction).applied, "a fixture edit applies");
}

MenuStripItemSpec menu(std::string id, std::string text, std::string item_id, std::string item_text) {
    MenuStripItemSpec spec{};
    spec.stable_id = std::move(id);
    spec.text = std::move(text);
    spec.items.push_back(MenuItemSpec(std::move(item_id), MenuItemKind::command, {}, std::move(item_text)));
    return spec;
}

// A window of 800 by 524: a main menu strip (File, Window) along the top and
// a docking site below it, with a tool panel on the left and three
// documents (Third selected) in the document area, shown as frames.
struct Fixture final {
    DocumentFactory factory{};
    std::unique_ptr<DockWorkspace> workspace{};
    std::shared_ptr<Panel> root{};
    std::shared_ptr<MenuStrip> strip{};
    std::shared_ptr<DockSite> site{};
    std::unique_ptr<Window> window{};
};

std::unique_ptr<Fixture> make_fixture(bool frames = true) {
    std::unique_ptr<Fixture> fixture = std::make_unique<Fixture>();
    (*fixture).workspace = std::make_unique<DockWorkspace>("workspace");
    DockWorkspace& workspace = *(*fixture).workspace;
    DocumentFactory& factory = (*fixture).factory;
    require(workspace.register_panel(tool(factory, "files", "Files")) &&
                workspace.register_panel(document(factory, "doc.first", "First.txt")) &&
                workspace.register_panel(document(factory, "doc.second", "Second.txt")) &&
                workspace.register_panel(document(factory, "doc.third", "Third & Co.txt")),
            "the panels register");
    apply(workspace, DockEdit::show(DockPanelId("files")));
    apply(workspace, DockEdit::show(DockPanelId("doc.first")));
    apply(workspace, DockEdit::show(DockPanelId("doc.second")));
    apply(workspace, DockEdit::show(DockPanelId("doc.third")));
    if (frames) {
        require(workspace.set_document_presentation(DockDocumentPresentation::frames).applied,
                "the frames presentation applies");
    }
    (*fixture).root = make_control<Panel>(StableId("root"));
    (*fixture).strip = make_control<MenuStrip>(StableId("menu"));
    std::vector<MenuStripItemSpec> items{};
    items.push_back(menu("file", "&File", "file-new", "&New"));
    MenuStripItemSpec window_menu = menu("window", "&Window", "window-cascade", "&Cascade");
    window_menu.window_list = true;
    items.push_back(std::move(window_menu));
    (*(*fixture).strip).set_items(std::move(items));
    (*(*fixture).strip).set_requested_bounds({0.0, 0.0, 800.0, 24.0});
    (*fixture).site = make_control<DockSite>(StableId("site"), workspace);
    (*(*fixture).site).set_requested_bounds({0.0, 24.0, 800.0, 500.0});
    (*(*fixture).root).add_child((*fixture).strip);
    (*(*fixture).root).add_child((*fixture).site);
    (*fixture).window = std::make_unique<Window>((*fixture).root, Size{800.0, 524.0});
    (*(*fixture).window).perform_layout();
    return fixture;
}

stxlib::Optional<DockFrameMemory> memory_of(const DockWorkspace& workspace, std::string_view panel) {
    return workspace.frame_memory(DockPanelId(std::string(panel)));
}

// A document's frame memory, or a default one for a document without a frame.
DockFrameMemory memory_value(const DockWorkspace& workspace, std::string_view panel) {
    const stxlib::Optional<DockFrameMemory> memory = memory_of(workspace, panel);
    if (memory.absent()) return DockFrameMemory{};
    return *memory.value();
}

DockFrameState state_of(const DockWorkspace& workspace, std::string_view panel) {
    const stxlib::Optional<DockFrameMemory> memory = memory_of(workspace, panel);
    if (memory.absent()) return DockFrameState::normal;
    return (*memory.value()).state;
}

std::uint32_t order_of(const DockWorkspace& workspace, std::string_view panel) {
    const stxlib::Optional<DockFrameMemory> memory = memory_of(workspace, panel);
    if (memory.absent()) return 0U;
    return (*memory.value()).order;
}

std::string frame_id(const DockSite& site, std::string_view panel) {
    const Control::Ptr frame = site.document_frame(DockPanelId(std::string(panel)));
    require(frame != nullptr, "the document has a frame");
    return std::string((*frame).stable_id().value());
}

Rect frame_on_window(const DockSite& site, std::string_view panel) {
    const Control::Ptr frame = site.document_frame(DockPanelId(std::string(panel)));
    require(frame != nullptr, "the document has a frame");
    return (*frame).absolute_bounds();
}

bool inside(Rect outer, Rect inner) {
    const bool contained = inner.x >= outer.x - 1e-6 && inner.y >= outer.y - 1e-6 &&
                           inner.x + inner.width <= outer.x + outer.width + 1e-6 &&
                           inner.y + inner.height <= outer.y + outer.height + 1e-6;
    return contained;
}

void frames_are_realized() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    require(site.document_frames_shown() && workspace.document_presentation() == DockDocumentPresentation::frames,
            "the main site shows the frames presentation");
    require(site.realized_stacks().size() == 1U, "the document area's stack has no view; the tool stack has one");
    const Rect area = site.document_area_bounds();
    for (const std::string_view panel : {std::string_view("doc.first"), std::string_view("doc.second"),
                                         std::string_view("doc.third")}) {
        const Rect bounds = site.document_frame_bounds(DockPanelId(std::string(panel)));
        require(!bounds.empty() && inside(area, bounds), "each frame shows inside the document area");
        require(memory_of(workspace, panel).present(), "each document has a frame memory");
    }
    // Defaults cascade: each frame one step below and right of the one before.
    const Rect first = site.document_frame_bounds(DockPanelId("doc.first"));
    const Rect second = site.document_frame_bounds(DockPanelId("doc.second"));
    require(second.x > first.x && second.y > first.y, "the default frames cascade");
    // Only the shown frames' content was made; each content is the frame's child.
    require((*fixture).factory.made.size() == 4U, "the tool and every frame's document get their content");
    const Control::Ptr edit = window.find("content.doc.first.edit");
    const Control::Ptr frame = site.document_frame(DockPanelId("doc.first"));
    require(edit && frame && (*frame).contains(*edit), "a document's content is inside its frame");
    // Each frame is a pane named by its title, with its caption buttons.
    const SemanticSnapshot snapshot = window.semantic_snapshot();
    const SemanticNode* pane = find_named(snapshot.roots, "First.txt");
    require(pane != nullptr && (*pane).role == SemanticRole::pane, "a frame is a pane named by its document's title");
    const std::string id = frame_id(site, "doc.first");
    const SemanticNode* maximize = find_node(snapshot.roots, id + ".maximize");
    const SemanticNode* minimize = find_node(snapshot.roots, id + ".minimize");
    const SemanticNode* close = find_node(snapshot.roots, id + ".close");
    require(maximize != nullptr && minimize != nullptr && close != nullptr && (*maximize).role == SemanticRole::button &&
                (*maximize).name == "Maximize" && (*minimize).name == "Minimize" && (*close).name == "Close",
            "a frame's caption buttons are its pane's buttons");
}

void window_is_bound_to_the_workspace() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    Window& window = *(*fixture).window;
    require(window.document_area() == &workspace.document_area() && window.is_mdi_container(),
            "the main site binds the workspace as its window's document area");
    const std::vector<DockPanelId> children = children_of(window);
    require(children.size() == 3U && children[0].value == "doc.first" && children[1].value == "doc.second" &&
                children[2].value == "doc.third",
            "the MDI children are the area's documents in tree and tab order");
    require(is(window.active_mdi_child(), "doc.third"), "with nothing activated, the frontmost frame is active");
    const BorrowedText title = workspace.document_area().document_title(DockPanelId("doc.second"));
    require(std::string_view(title.data(), title.size()) == "Second.txt", "the area names a child by its title");
    ActivationLog log{};
    const SubscriptionToken token = window.mdi_child_activate().subscribe(RecordActivation{&log});
    require(window.activate_mdi_child(DockPanelId("doc.first")) && is(window.active_mdi_child(), "doc.first") &&
                is(workspace.area_active_document(), "doc.first"),
            "activate_mdi_child activates the document");
    require(log.current.size() == 1U && log.previous[0] == "doc.third" && log.current[0] == "doc.first",
            "mdi_child_activate reports the old and new child");
    require(order_of(workspace, "doc.first") > order_of(workspace, "doc.second") &&
                order_of(workspace, "doc.first") > order_of(workspace, "doc.third"),
            "the activated frame comes to the front");
    require(window.focused_control() && (*window.focused_control()).stable_id().value() == "content.doc.first.edit",
            "activating a child focuses its content");
    require(!window.activate_mdi_child(DockPanelId("files")), "a tool panel is no MDI child");
    // Focus entering another frame activates its document.
    require(window.request_focus(window.find("content.doc.second.edit")) && is(window.active_mdi_child(), "doc.second") &&
                log.current.back() == "doc.second",
            "focus entering a frame activates its document");
    // Switching back to tabs through the window.
    window.set_is_mdi_container(false);
    window.perform_layout();
    DockSite& site = *(*fixture).site;
    require(!site.document_frames_shown() && workspace.document_presentation() == DockDocumentPresentation::tabs &&
                !site.document_frame(DockPanelId("doc.first")) && site.realized_stacks().size() == 2U,
            "is_mdi_container false shows the documents as tabs again");
    const Control::Ptr edit = window.find("content.doc.second.edit");
    require(edit && (*site.stack_view(workspace.layout().locate(DockPanelId("doc.second")).stack)).contains(*edit),
            "the selected document's content returns to the document stack's view");
    require(memory_of(workspace, "doc.first").present(), "the frame memories wait while tabs show");
    window.set_is_mdi_container(true);
    window.perform_layout();
    require(site.document_frames_shown() && site.document_frame(DockPanelId("doc.first")) &&
                (*site.document_frame(DockPanelId("doc.first"))).contains(*window.find("content.doc.first.edit")),
            "frames again, each content back in its frame");
}

void caption_buttons_and_states() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    const Rect area = site.document_area_bounds();
    // Maximize by the semantic button.
    require(window.perform_semantic_action(frame_id(site, "doc.second") + ".maximize", SemanticAction::press),
            "the maximize button runs");
    window.perform_layout();
    require(state_of(workspace, "doc.second") == DockFrameState::maximized &&
                site.document_frame_bounds(DockPanelId("doc.second")) == area,
            "a maximized frame fills the document area");
    require(is(workspace.area_active_document(), "doc.second"), "maximizing activates the frame");
    // A maximized frame stays maximized across activation: carried to the
    // newly active document.
    require(workspace.activate_panel(DockPanelId("doc.first")), "another document activates");
    require(state_of(workspace, "doc.first") == DockFrameState::maximized &&
                state_of(workspace, "doc.second") == DockFrameState::normal,
            "the maximized state moves to the newly active document");
    // Maximizing one frame restores another.
    require(workspace.set_frame_state(DockPanelId("doc.third"), DockFrameState::maximized).applied &&
                state_of(workspace, "doc.first") == DockFrameState::normal,
            "at most one frame is maximized");
    // Restore by the pointer: the maximize button of the maximized frame.
    window.perform_layout();
    const SemanticSnapshot snapshot = window.semantic_snapshot();
    const SemanticNode* restore = find_node(snapshot.roots, frame_id(site, "doc.third") + ".maximize");
    require(restore != nullptr && (*restore).name == "Restore", "a maximized frame's second button restores");
    const Point centre{(*restore).bounds.x + (*restore).bounds.width * 0.5,
                       (*restore).bounds.y + (*restore).bounds.height * 0.5};
    press(window, centre);
    release(window, centre);
    require(state_of(workspace, "doc.third") == DockFrameState::normal, "pressing and releasing the button restores");
    // Minimize: the frame becomes an icon along the bottom, and the next
    // frame used before it becomes active.
    require(window.perform_semantic_action(frame_id(site, "doc.third") + ".minimize", SemanticAction::press),
            "the minimize button runs");
    window.perform_layout();
    const Rect icon = site.document_frame_bounds(DockPanelId("doc.third"));
    require(state_of(workspace, "doc.third") == DockFrameState::minimized && icon.height < 40.0 &&
                near(icon.y + icon.height, area.y + area.height) && near(icon.x, area.x),
            "a minimized frame shows as an icon in the first slot along the bottom");
    require(!is(workspace.area_active_document(), "doc.third"), "minimizing the active frame activates another");
    const SemanticSnapshot minimized = window.semantic_snapshot();
    const SemanticNode* restore_icon = find_node(minimized.roots, frame_id(site, "doc.third") + ".minimize");
    require(restore_icon != nullptr && (*restore_icon).name == "Restore", "an icon's first button restores");
    // Icons along the top when the window says so.
    window.set_mdi_children_minimized_anchor_bottom(false);
    window.perform_layout();
    require(near(site.document_frame_bounds(DockPanelId("doc.third")).y, area.y) &&
                !workspace.frame_icons_at_bottom(),
            "the icons line the top of the area");
    // A double-click on an icon's caption restores it. Back along the bottom
    // first: along the top, the active frame (doc.first, in front) covers
    // the icon, and the double-click would maximize that frame instead.
    window.set_mdi_children_minimized_anchor_bottom(true);
    window.perform_layout();
    const Rect on_window = frame_on_window(site, "doc.third");
    press(window, {on_window.x + 12.0, on_window.y + on_window.height * 0.5}, 2U);
    release(window, {on_window.x + 12.0, on_window.y + on_window.height * 0.5});
    require(state_of(workspace, "doc.third") == DockFrameState::normal && is(workspace.area_active_document(), "doc.third"),
            "a double-click on a minimized frame restores and activates it");
    // A double-click on a normal caption maximizes.
    window.perform_layout();
    const Rect normal = frame_on_window(site, "doc.third");
    press(window, {normal.x + 20.0, normal.y + 10.0}, 2U);
    release(window, {normal.x + 20.0, normal.y + 10.0});
    require(state_of(workspace, "doc.third") == DockFrameState::maximized, "a double-click on a caption maximizes");
}

void pointer_moves_and_resizes() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    require(workspace.set_frame_bounds(DockPanelId("doc.first"), {60.0, 50.0, 300.0, 200.0}).applied,
            "the program places a frame");
    require(!workspace.set_frame_bounds(DockPanelId("doc.first"), {0.0, 0.0, 0.0, 10.0}).applied,
            "an empty frame is refused");
    require(!workspace.set_frame_bounds(DockPanelId("files"), {0.0, 0.0, 100.0, 100.0}).applied,
            "a tool panel has no frame");
    require(workspace.activate_panel(DockPanelId("doc.first")), "the placed frame comes to the front");
    window.perform_layout();
    // Dragging the caption moves the frame (one user edit at the release).
    Rect bounds = frame_on_window(site, "doc.first");
    const Point grip{bounds.x + 40.0, bounds.y + 12.0};
    press(window, grip);
    move_to(window, {grip.x + 30.0, grip.y + 20.0});
    window.perform_layout();
    require(near(frame_on_window(site, "doc.first").x, bounds.x + 30.0) &&
                near(memory_value(workspace, "doc.first").bounds.x, 60.0),
            "the frame follows the drag; the layout waits for the release");
    release(window, {grip.x + 30.0, grip.y + 20.0});
    const Rect moved = memory_value(workspace, "doc.first").bounds;
    require(near(moved.x, 90.0) && near(moved.y, 70.0) && near(moved.width, 300.0),
            "the release commits the moved frame");
    // Dragging the right border resizes it.
    window.perform_layout();
    bounds = frame_on_window(site, "doc.first");
    const Point edge{bounds.x + bounds.width - 2.0, bounds.y + bounds.height * 0.5};
    press(window, edge);
    move_to(window, {edge.x + 50.0, edge.y});
    release(window, {edge.x + 50.0, edge.y});
    const Rect resized = memory_value(workspace, "doc.first").bounds;
    require(near(resized.width, 350.0) && near(resized.x, 90.0) && near(resized.height, 200.0),
            "dragging the right border widens the frame");
    // A drag past the area's edge keeps the frame inside.
    window.perform_layout();
    bounds = frame_on_window(site, "doc.first");
    const Point again{bounds.x + 40.0, bounds.y + 12.0};
    press(window, again);
    move_to(window, {again.x - 500.0, again.y - 500.0});
    release(window, {again.x - 500.0, again.y - 500.0});
    const Rect kept = memory_value(workspace, "doc.first").bounds;
    require(near(kept.x, 0.0) && near(kept.y, 0.0), "a frame dragged past the area stops at its edge");
}

void arrangements() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    const Rect area = site.document_area_bounds();
    require(window.layout_mdi(MdiLayout::tile_vertical), "tiling vertically arranges");
    window.perform_layout();
    const std::vector<DockPanelId> children = children_of(window);
    double total_width = 0.0;
    for (const DockPanelId& child : children) {
        const Rect bounds = site.document_frame_bounds(child);
        require(near(bounds.y, area.y) && inside(area, bounds), "vertical tiles stand side by side inside the area");
        total_width += bounds.width;
    }
    require(near(total_width, area.width), "vertical tiles share the area's width");
    require(window.layout_mdi(MdiLayout::tile_horizontal), "tiling horizontally arranges");
    window.perform_layout();
    double total_height = 0.0;
    for (const DockPanelId& child : children) {
        const Rect bounds = site.document_frame_bounds(child);
        require(near(bounds.x, area.x) && near(bounds.width, area.width), "horizontal tiles span the area's width");
        total_height += bounds.height;
    }
    require(total_height <= area.height + 1e-6, "horizontal tiles fit the area's height");
    require(window.layout_mdi(MdiLayout::cascade), "cascading arranges");
    window.perform_layout();
    const Rect first = site.document_frame_bounds(children[0]);
    const Rect last = site.document_frame_bounds(children[2]);
    require(!(first == last) && near(first.width, last.width), "cascaded frames step and share a size");
    // Arrange icons closes gaps between minimized frames.
    DockWorkspace& workspace = *(*fixture).workspace;
    require(workspace.set_frame_state(DockPanelId("doc.first"), DockFrameState::minimized).applied &&
                workspace.set_frame_state(DockPanelId("doc.second"), DockFrameState::minimized).applied &&
                workspace.set_frame_state(DockPanelId("doc.first"), DockFrameState::normal).applied,
            "two frames minimize and one returns");
    require(window.layout_mdi(MdiLayout::arrange_icons), "arranging icons arranges");
    require(memory_value(workspace, "doc.second").icon_slot == 0U, "arrange icons renumbers from the first slot");
    // Tabs: nothing to arrange.
    window.set_is_mdi_container(false);
    require(!window.layout_mdi(MdiLayout::cascade), "with tabs shown there is nothing to arrange");
}

void keys_close_and_step() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    Window& window = *(*fixture).window;
    require(window.activate_mdi_child(DockPanelId("doc.first")), "the first document activates");
    require(window.activate_mdi_child(DockPanelId("doc.second")), "the second document activates");
    // Ctrl+F6 steps to the document used before.
    require(key(window, PhysicalKey::f6, Modifier::control), "Ctrl+F6 is handled");
    key_up(window, PhysicalKey::left_control);
    require(is(window.active_mdi_child(), "doc.first"), "Ctrl+F6 steps through the frames");
    // Ctrl+F4 closes the active frame.
    require(key(window, PhysicalKey::f4, Modifier::control), "Ctrl+F4 is handled");
    require(workspace.layout().locate(DockPanelId("doc.first")).placement == DockPlacementKind::absent,
            "Ctrl+F4 closes the active frame's document");
    window.perform_layout();
    require(!(*(*fixture).site).document_frame(DockPanelId("doc.first")) && children_of(window).size() == 2U,
            "its frame goes, and the window has one child fewer");
    // The close button closes too.
    const std::string id = frame_id(*(*fixture).site, "doc.second");
    require(window.perform_semantic_action(id + ".close", SemanticAction::press), "the close button runs");
    require(workspace.layout().locate(DockPanelId("doc.second")).placement == DockPlacementKind::absent,
            "the close button closes its document");
}

void buttons_on_the_main_menu_strip() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    MenuStrip& strip = *(*fixture).strip;
    window.set_main_menu_strip((*fixture).strip);
    require(!strip.document_frame_buttons(), "no frame is maximized: the strip shows no frame buttons");
    require(workspace.set_frame_state(DockPanelId("doc.second"), DockFrameState::maximized).applied,
            "a frame maximizes");
    window.perform_layout();
    require(strip.document_frame_buttons() && !strip.document_frame_button_bounds(MenuStripFrameButton::close).empty(),
            "a maximized frame's buttons show on the main menu strip");
    const SemanticSnapshot snapshot = window.semantic_snapshot();
    require(find_node(snapshot.roots, frame_id(site, "doc.second") + ".maximize") == nullptr,
            "the maximized frame hides its caption and buttons");
    require(find_node(snapshot.roots, "menu.frame-restore") != nullptr &&
                find_node(snapshot.roots, "menu.frame-minimize") != nullptr &&
                find_node(snapshot.roots, "menu.frame-close") != nullptr,
            "the strip publishes the frame's buttons");
    const Rect area = site.document_area_bounds();
    require(site.document_frame_bounds(DockPanelId("doc.second")) == area, "the maximized frame fills the area");
    // The strip's restore button restores the frame and takes the buttons off.
    require(window.perform_semantic_action("menu.frame-restore", SemanticAction::press), "the strip's restore runs");
    window.perform_layout();
    require(state_of(workspace, "doc.second") == DockFrameState::normal && !strip.document_frame_buttons(),
            "restoring takes the buttons off the strip");
    // The strip's close button closes the maximized document, by the pointer.
    require(workspace.set_frame_state(DockPanelId("doc.first"), DockFrameState::maximized).applied,
            "another frame maximizes");
    window.perform_layout();
    const Rect close = strip.document_frame_button_bounds(MenuStripFrameButton::close);
    const Rect absolute = strip.absolute_bounds();
    const Point centre{absolute.x + close.x + close.width * 0.5, absolute.y + close.y + close.height * 0.5};
    press(window, centre);
    release(window, centre);
    require(workspace.layout().locate(DockPanelId("doc.first")).placement == DockPlacementKind::absent,
            "the strip's close button closes the maximized document");
    window.perform_layout();
    require(!strip.document_frame_buttons(), "with no frame maximized the strip's buttons go");
}

void menu_strips_merge() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    Window& window = *(*fixture).window;
    MenuStrip& main = *(*fixture).strip;
    window.set_main_menu_strip((*fixture).strip);
    std::shared_ptr<MenuStrip> first_strip = make_control<MenuStrip>(StableId("first-menu"));
    std::vector<MenuStripItemSpec> first_items{};
    first_items.push_back(menu("edit", "&Edit", "edit-undo", "&Undo"));
    (*first_strip).set_items(std::move(first_items));
    std::shared_ptr<MenuStrip> second_strip = make_control<MenuStrip>(StableId("second-menu"));
    std::vector<MenuStripItemSpec> second_items{};
    second_items.push_back(menu("tools", "&Tools", "tools-run", "&Run"));
    (*second_strip).set_items(std::move(second_items));
    require(workspace.set_panel_menu_strip(DockPanelId("doc.first"), *first_strip) &&
                workspace.set_panel_menu_strip(DockPanelId("doc.second"), *second_strip),
            "the documents' menu strips register");
    require(!workspace.set_panel_menu_strip(DockPanelId("unknown"), *first_strip), "an unknown panel is refused");
    require(workspace.panel_menu_strip(DockPanelId("doc.first")) == first_strip, "a panel's strip is held");
    require(window.activate_mdi_child(DockPanelId("doc.first")), "the first document activates");
    require(main.items().size() == 3U && main.items()[2].stable_id == "edit" && is(workspace.merged_document(), "doc.first"),
            "the active child's menus merge into the main menu strip");
    require(window.activate_mdi_child(DockPanelId("doc.second")), "the second document activates");
    require(main.items().size() == 3U && main.items()[2].stable_id == "tools" && is(workspace.merged_document(), "doc.second"),
            "the merge follows the active child, the old one reverted");
    require((*first_strip).items().size() == 1U && (*first_strip).items()[0].stable_id == "edit",
            "a reverted strip has its menus back");
    require(window.activate_mdi_child(DockPanelId("doc.third")), "a document without a strip activates");
    require(main.items().size() == 2U && workspace.merged_document().absent(),
            "a child without a strip leaves the main strip as it was");
    require(window.activate_mdi_child(DockPanelId("doc.first")) && main.items().size() == 3U,
            "the first strip merges again");
    require(workspace.clear_panel_menu_strip(DockPanelId("doc.first")) && main.items().size() == 2U &&
                workspace.merged_document().absent(),
            "clearing the active child's strip reverts its merge");
}

void window_list() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    Window& window = *(*fixture).window;
    MenuStrip& strip = *(*fixture).strip;
    const stxlib::Optional<std::size_t> list_item = strip.mdi_window_list_item();
    require(list_item.present() && *list_item.value() == 1U,
            "the Window menu is the window list item");
    require(window.activate_mdi_child(DockPanelId("doc.first")), "the first document activates");
    require(strip.open(1U), "the Window menu opens");
    const SemanticSnapshot snapshot = window.semantic_snapshot();
    const SemanticNode* first = find_named(snapshot.roots, "1 First.txt");
    const SemanticNode* second = find_named(snapshot.roots, "2 Second.txt");
    const SemanticNode* third = find_named(snapshot.roots, "3 Third & Co.txt");
    require(first != nullptr && second != nullptr && third != nullptr,
            "the window list shows one numbered row per child after the menu's own rows");
    require(has_semantic_state((*first).states, SemanticState::checked),
            "the active child's row is checked");
    const std::string row = (*second).stable_id;
    require(window.perform_semantic_action(row, SemanticAction::press), "a window list row runs");
    require(is(window.active_mdi_child(), "doc.second"), "choosing a row activates its child");
    strip.close();
}

struct RecordFrameButton final {
    std::vector<MenuStripFrameButton>* seen{};
    void operator()(MenuStripFrameButton button) const { (*seen).push_back(button); }
};

void workspace_frame_calls() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    DockWorkspace& workspace = *(*fixture).workspace;
    DockSite& site = *(*fixture).site;
    Window& window = *(*fixture).window;
    const DockFrameMetrics metrics = site.frame_metrics();
    require(metrics.caption_height > 0.0 && metrics.icon_width > 0.0 && metrics.icons_at_bottom,
            "the site measures its frames by its caption, with the icons at the bottom");
    require(near(workspace.frame_metrics().caption_height, metrics.caption_height),
            "the workspace measures frames as its main site does");
    const BorrowedSpan<DockPanelId> documents = workspace.area_documents();
    require(documents.size() == 3U && documents[0].value == "doc.first" && documents[2].value == "doc.third",
            "the area's documents are listed in tree and tab order");
    require(workspace.arrange_frames(DockFrameArrangement::tile_vertical).applied, "the workspace tiles the frames");
    window.perform_layout();
    const Rect area = site.document_area_bounds();
    require(near(site.document_frame_bounds(DockPanelId("doc.first")).y, area.y) &&
                near(site.document_frame_bounds(DockPanelId("doc.second")).height, area.height),
            "tiled frames stand side by side, each the area's height");
    require(workspace.set_document_presentation(DockDocumentPresentation::frames).applied,
            "setting the presentation it has applies with nothing to change");
    // The strip's frame buttons raise frame_button_pressed.
    MenuStrip& strip = *(*fixture).strip;
    std::vector<MenuStripFrameButton> seen{};
    const SubscriptionToken token = strip.frame_button_pressed().subscribe(RecordFrameButton{&seen});
    strip.set_document_frame_buttons(true);
    window.perform_layout();
    require(window.perform_semantic_action("menu.frame-minimize", SemanticAction::press) && seen.size() == 1U &&
                seen[0] == MenuStripFrameButton::minimize,
            "a frame button's press raises frame_button_pressed");
    strip.set_document_frame_buttons(false);
    require(!window.perform_semantic_action("menu.frame-minimize", SemanticAction::press) && seen.size() == 1U,
            "buttons that are off take no press");
}

void lifetime() {
    std::unique_ptr<Fixture> fixture = make_fixture();
    Window& window = *(*fixture).window;
    DockWorkspace& workspace = *(*fixture).workspace;
    require(window.document_area() == &workspace.document_area(), "the area is bound");
    // The site leaving the window unbinds the area.
    static_cast<void>((*(*fixture).root).remove_child((*(*fixture).site).runtime_id()));
    require(window.document_area() == nullptr && children_of(window).empty(),
            "the main site leaving its window unbinds the area");
    (*(*fixture).root).add_child((*fixture).site);
    window.perform_layout();
    require(window.document_area() == &workspace.document_area(), "the site coming back binds it again");
    // The workspace going unbinds it.
    (*fixture).workspace.reset();
    require(window.document_area() == nullptr, "the workspace going unbinds the area");
}

} // namespace

int main() {
    window_facade_over_a_fake_area();
    frames_are_realized();
    window_is_bound_to_the_workspace();
    caption_buttons_and_states();
    pointer_moves_and_resizes();
    arrangements();
    keys_close_and_step();
    buttons_on_the_main_menu_strip();
    menu_strips_merge();
    window_list();
    workspace_frame_calls();
    lifetime();
    std::cout << "MDI: the window's facade, document frames, buttons, gestures, arrangements, keys, "
                 "main menu strip buttons, menu merging, window list and lifetime passed\n";
    return EXIT_SUCCESS;
}
