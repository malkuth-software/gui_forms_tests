// Mockups (gui_forms/forms/form_mockup; docs/user/mockups.md): a document
// and its fixture loaded at run time into a host control, the fixture bound
// and shown, declared handlers and commands recorded and never refused, a
// refused reload that keeps the shown tree, a good one that replaces it in
// one step, TabPages made pages, masks set, and a theme sheet on the host.

#include "gui_forms/forms/form_mockup/form_mockup.hpp"
#include "gui_forms/forms/form_documents/form_documents.hpp"

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/data_grid_view.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/controls/masked_text_box.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/tab_control.hpp"
#include "gui_forms/controls/tab_page.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/controls/tree_view.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] std::string_view text_of(stxlib::View<char> view) {
    return std::string_view(view.data(), static_cast<std::size_t>(view.count()));
}

[[nodiscard]] gui_forms::FormMockupText mockup_text(std::string_view file, std::string_view text) {
    gui_forms::FormMockupText result{};
    result.file = stxlib::View<char>(file.data(), file.size());
    result.text = stxlib::View<char>(text.data(), text.size());
    return result;
}

[[nodiscard]] gui_forms::Control::Ptr find(const gui_forms::Control::Ptr& from, std::string_view stable_id) {
    if (!from) return gui_forms::Control::Ptr();
    if ((*from).stable_id().value() == stable_id) return from;
    for (std::size_t index = 0U; index < (*from).child_count(); ++index) {
        gui_forms::Control::Ptr found = find((*from).child_at(index), stable_id);
        if (found) return found;
    }
    return gui_forms::Control::Ptr();
}

template <typename Type>
[[nodiscard]] std::shared_ptr<Type> find_as(const gui_forms::Control::Ptr& from, std::string_view stable_id) {
    return std::dynamic_pointer_cast<Type>(find(from, stable_id));
}

// A host panel in a window, and a mockup over it, declared in that order.
struct Stage final {
    std::shared_ptr<gui_forms::Panel> host = gui_forms::make_control<gui_forms::Panel>(gui_forms::StableId("host"));
    gui_forms::Window window{host, gui_forms::Size{640.0, 480.0}};
    gui_forms::FormMockup mockup{*host};
};

[[nodiscard]] gui_forms::FormMockupLoadOutcome load(Stage& stage, std::string_view document, std::string_view fixture) {
    return stage.mockup.load(gui_forms::FormDocumentSchema::registry(), mockup_text("claim.form", document),
                             mockup_text("claim.fixture", fixture));
}

constexpr std::string_view claim_form = R"(<?xml version="1.0" encoding="UTF-8"?>
<form xmlns="urn:gui-forms:form:1" id="claim.window" text="Claim" client-size="400 300"
    accept-button="claim.submit">
  <panel id="claim.root">
    <check-box id="claim.urgent" text="Urgent" bounds="8 8 200 28" on-checked-changed="urgency_changed">
      <bind property="checked" to="urgent"/>
    </check-box>
    <text-box id="claim.holder" bounds="8 40 200 28">
      <bind property="text" to="claims" field="holder"/>
    </text-box>
    <list-box id="claim.items" bounds="8 72 200 60"/>
    <data-grid-view id="claim.grid" bounds="8 136 380 80"/>
    <tree-view id="claim.files" bounds="216 8 172 120" on-key-press="key_pressed"/>
    <button id="claim.save" text="Save" bounds="8 260 88 32" on-click="save_claim"/>
    <button id="claim.submit" text="Submit" bounds="104 260 88 32">
      <bind property="command" to="submit_claim"/>
    </button>
  </panel>
</form>
)";

constexpr std::string_view claim_fixture = R"(<?xml version="1.0" encoding="UTF-8"?>
<fixture xmlns="urn:gui-forms:fixture:1">
  <value name="urgent" value="true"/>
  <records name="claims" columns="holder amount:number" shown-in="claim.items claim.grid">
    <row values="'Ada Marsh' 640.00"/>
    <row values="'Ben Hale' 1250.00"/>
  </records>
  <tree shown-in="claim.files">
    <item text="Documents" expanded="true">
      <item text="Claim.pdf"/>
    </item>
  </tree>
</fixture>
)";

int sink_calls = 0;

void count_event(void* context, const gui_forms::FormMockupEvent& event) {
    static_cast<void>(event);
    int* const calls = static_cast<int*>(context);
    ++*calls;
}

void test_load_binds_and_shows() {
    Stage stage{};
    require(!stage.mockup.shown() && stage.mockup.generation() == 0U, "load: nothing is shown before a load");
    require(load(stage, claim_form, claim_fixture) == gui_forms::FormMockupLoadOutcome::loaded,
            "load: the mockup loads");
    require(stage.mockup.shown() && stage.mockup.generation() == 1U, "load: the first generation is shown");
    require(text_of(stage.mockup.title()) == "Claim", "load: the form's title");
    require(stage.mockup.client_size().width == 400.0 && stage.mockup.client_size().height == 300.0,
            "load: the form's client size");
    const gui_forms::Control::Ptr root = stage.host;
    const std::shared_ptr<gui_forms::CheckBox> urgent = find_as<gui_forms::CheckBox>(root, "claim.urgent");
    require(urgent && (*urgent).checked.read(), "load: a fixture value is bound");
    const std::shared_ptr<gui_forms::TextBox> holder = find_as<gui_forms::TextBox>(root, "claim.holder");
    require(holder && (*holder).text.read() == "Ada Marsh", "load: a record field reads the first row");
    const std::shared_ptr<gui_forms::ListBox> items = find_as<gui_forms::ListBox>(root, "claim.items");
    require(items && (*items).items.read().size() == 2U && (*items).items.read()[0] == "Ada Marsh",
            "load: a list box shows the first column");
    const std::shared_ptr<gui_forms::DataGridView> grid = find_as<gui_forms::DataGridView>(root, "claim.grid");
    require(grid && (*grid).row_count() == 2U, "load: a grid shows the rows");
    const std::shared_ptr<gui_forms::TreeView> files = find_as<gui_forms::TreeView>(root, "claim.files");
    require(files && (*files).items().size() == 2U && (*files).items()[1].depth == 1U,
            "load: a tree view shows the nested items");
    require(stage.mockup.check().compiled() && stage.mockup.check().error_count() == 0U,
            "load: the check says nothing is wrong");
}

void test_handlers_record() {
    Stage stage{};
    if (load(stage, claim_form, claim_fixture) != gui_forms::FormMockupLoadOutcome::loaded) {
        require(false, "handlers: the mockup loads");
        return;
    }
    // Commands first, then the declared handlers in document order.
    require(stage.mockup.handler_count() == 4U, "handlers: a command and three declared handlers");
    if (stage.mockup.handler_count() == 4U) {
        const gui_forms::FormMockupHandler command = stage.mockup.handler(0U);
        require(text_of(command.handler) == "submit_claim" && text_of(command.interaction) == "invoked" &&
                    command.observed,
                "handlers: the command is listed first");
        const gui_forms::FormMockupHandler changed = stage.mockup.handler(1U);
        require(text_of(changed.handler) == "urgency_changed" && text_of(changed.stable_id) == "claim.urgent" &&
                    text_of(changed.interaction) == "checked_changed" && changed.observed,
                "handlers: a property's change is observed");
        const gui_forms::FormMockupHandler key = stage.mockup.handler(2U);
        require(text_of(key.handler) == "key_pressed" && !key.observed,
                "handlers: an interaction the mockup does not observe is listed, not refused");
        const gui_forms::FormMockupHandler click = stage.mockup.handler(3U);
        require(text_of(click.handler) == "save_claim" && click.observed, "handlers: a click is observed");
    }
    require(text_of(stage.mockup.handler(99U).handler).empty(), "handlers: past the end is empty");

    stage.mockup.set_event_sink(&count_event, &sink_calls);
    const gui_forms::Control::Ptr root = stage.host;
    const std::shared_ptr<gui_forms::Button> save = find_as<gui_forms::Button>(root, "claim.save");
    const std::shared_ptr<gui_forms::Button> submit = find_as<gui_forms::Button>(root, "claim.submit");
    const std::shared_ptr<gui_forms::CheckBox> urgent = find_as<gui_forms::CheckBox>(root, "claim.urgent");
    if (!save || !submit || !urgent) {
        require(false, "handlers: the buttons and the check box are shown");
        return;
    }
    static_cast<void>((*save).perform_click());
    bool clicked = false;
    for (stxlib::usize index = 0U; index < stage.mockup.event_count(); ++index) {
        const gui_forms::FormMockupEvent event = stage.mockup.event(index);
        if (text_of(event.handler) == "save_claim" && text_of(event.stable_id) == "claim.save") clicked = true;
    }
    require(clicked, "handlers: the click is recorded");
    const stxlib::usize after_click = stage.mockup.event_count();
    static_cast<void>((*submit).perform_click());
    require(stage.mockup.event_count() > after_click &&
                text_of(stage.mockup.event(stage.mockup.event_count() - 1U).handler) == "submit_claim" &&
                text_of(stage.mockup.event(stage.mockup.event_count() - 1U).interaction) == "invoked",
            "handlers: the bound command records its invocation");
    (*urgent).checked = false;
    require(text_of(stage.mockup.event(stage.mockup.event_count() - 1U).handler) == "urgency_changed",
            "handlers: the property change is recorded");
    require(sink_calls == static_cast<int>(stage.mockup.event_count()), "handlers: the sink hears every event");
    const stxlib::u64 first_sequence = stage.mockup.event(0U).sequence;
    require(first_sequence == 1U, "handlers: sequences count from 1");
    stage.mockup.clear_events();
    require(stage.mockup.event_count() == 0U, "handlers: clear_events drops them");
    stage.mockup.set_event_sink(nullptr, nullptr);
}

void test_reload() {
    Stage stage{};
    require(load(stage, claim_form, claim_fixture) == gui_forms::FormMockupLoadOutcome::loaded, "reload: first load");
    const gui_forms::Control::Ptr first_save = find(stage.host, "claim.save");

    // A broken document: refused on its line, the shown tree kept.
    const std::string broken = std::string(claim_form).replace(std::string(claim_form).find("<list-box"), 9U, "<list-bx");
    require(load(stage, broken, claim_fixture) == gui_forms::FormMockupLoadOutcome::refused,
            "reload: a broken document is refused");
    require(stage.mockup.check().error_count() >= 1U, "reload: the refusal is reported");
    if (stage.mockup.check().count() >= 1U) {
        const gui_forms::FormDocumentDiagnostic first = stage.mockup.check().diagnostic(0U);
        require(first.line == 11U && first.column == 5U && text_of(first.code_label) == "E802" &&
                    text_of(first.file) == "claim.form",
                "reload: the refusal names the file, line, column and code");
    }
    require(stage.mockup.generation() == 1U && find(stage.host, "claim.save") == first_save,
            "reload: the shown tree stays");

    // A fixture value of the wrong kind: E819 in the fixture.
    const std::string wrong_kind = std::string(claim_fixture).replace(std::string(claim_fixture).find("\"true\""), 6U,
                                                                      "\"maybe\"");
    require(load(stage, claim_form, wrong_kind) == gui_forms::FormMockupLoadOutcome::refused,
            "reload: a fixture value of the wrong kind is refused");
    if (stage.mockup.check().count() >= 1U) {
        const gui_forms::FormDocumentDiagnostic first = stage.mockup.check().diagnostic(0U);
        require(text_of(first.code_label) == "E819" && text_of(first.file) == "claim.fixture" && first.line == 3U,
                "reload: the fixture refusal names the fixture's line");
    }

    // The repaired document replaces the tree in one step.
    require(load(stage, claim_form, claim_fixture) == gui_forms::FormMockupLoadOutcome::loaded,
            "reload: the repaired document loads");
    require(stage.mockup.generation() == 2U, "reload: the second generation is shown");
    const gui_forms::Control::Ptr second_save = find(stage.host, "claim.save");
    require(second_save && second_save != first_save && !(*first_save).parent(),
            "reload: the new tree replaces the old one");
    require((*stage.host).child_count() == 1U, "reload: the host holds one tree");

    stage.mockup.clear();
    require(!stage.mockup.shown() && (*stage.host).child_count() == 0U, "reload: clear removes the tree");
}

void test_compiler_still_refuses_handlers() {
    const gui_forms::FormDocumentCheck check = gui_forms::check_form_document(
        gui_forms::FormDocumentSchema::registry(), "claim.form", 10U, claim_form.data(), claim_form.size());
    bool removed = false;
    for (stxlib::usize index = 0U; index < check.count(); ++index) {
        if (text_of(check.diagnostic(index).code_label) == "E816") removed = true;
    }
    require(!check.compiled() && removed, "compiler: a handler attribute is still E816 outside a mockup");
}

void test_tab_pages_and_masks() {
    constexpr std::string_view tabs = R"(<?xml version="1.0" encoding="UTF-8"?>
<panel xmlns="urn:gui-forms:form:1" id="tabs.root">
  <tab-control id="tabs.control" bounds="0 0 300 200">
    <tab-page id="tabs.holder" text="Policyholder">
      <masked-text-box id="tabs.phone" mask="(000) 000-0000" text="5551234567" bounds="8 8 160 28"/>
    </tab-page>
    <tab-page id="tabs.incident" text="Incident"/>
  </tab-control>
</panel>
)";
    Stage stage{};
    require(load(stage, tabs, {}) == gui_forms::FormMockupLoadOutcome::loaded, "tabs: a control root loads, no fixture");
    const std::shared_ptr<gui_forms::TabControl> control = find_as<gui_forms::TabControl>(stage.host, "tabs.control");
    require(control && (*control).page_count() == 2U, "tabs: each TabPage child is a page");
    if (control && (*control).page_count() == 2U) {
        require((*(*control).page_at(0U)).text.read() == "Policyholder" &&
                    (*(*control).page_at(1U)).text.read() == "Incident",
                "tabs: a page's text is its tab's");
    }
    const std::shared_ptr<gui_forms::MaskedTextBox> phone = find_as<gui_forms::MaskedTextBox>(stage.host, "tabs.phone");
    require(phone && (*phone).mask.read() == "(000) 000-0000", "tabs: a masked text box's mask");
    require(phone && (*phone).mask_completed(), "tabs: its text is placed through the mask");
    require(text_of(stage.mockup.title()).empty() && stage.mockup.client_size().width == 0.0,
            "tabs: a control root has no title or client size");
}

void test_theme() {
    Stage stage{};
    require(load(stage, claim_form, claim_fixture) == gui_forms::FormMockupLoadOutcome::loaded, "theme: load");
    const gui_forms::ThemeCompileResult plain =
        stage.mockup.set_theme(gui_forms::ThemeBase::windows_professional, mockup_text("claim.theme.css", {}));
    require(plain.succeeded(), "theme: the base alone compiles");
    const gui_forms::ThemeCompileResult sheet = stage.mockup.set_theme(
        gui_forms::ThemeBase::gui_forms, mockup_text("claim.theme.css", ".status { font-weight: 600; }"));
    require(sheet.succeeded(), "theme: a sheet compiles onto the base");
    const gui_forms::ThemeCompileResult refused = stage.mockup.set_theme(
        gui_forms::ThemeBase::gui_forms, mockup_text("claim.theme.css", ".status { font-weight: ; "));
    require(!refused.succeeded() && !refused.diagnostics().empty(), "theme: a broken sheet is refused with diagnostics");
}

} // namespace

int main() {
    test_load_binds_and_shows();
    test_handlers_record();
    test_reload();
    test_compiler_still_refuses_handlers();
    test_tab_pages_and_masks();
    test_theme();
    if (failures != 0) {
        std::cerr << failures << " form mockup checks failed\n";
        return 1;
    }
    std::cout << "form mockup: load, handlers, reload, tabs and theme passed\n";
    return 0;
}
