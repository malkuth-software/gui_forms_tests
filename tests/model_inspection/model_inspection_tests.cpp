// Stage MCP G6: the inspect additions (src/core/model_inspection: the state
// summary, layout reasons and the resource summary) and the forms tools'
// registry (src/core/form_language/form_export: export, validate, apply,
// revert).

#include "src/controls/form_document_export/process_form_exports.hpp"
#include "src/core/form_language/form_export/form_export.hpp"
#include "src/core/form_language/form_loader/form_loader.hpp"
#include "src/core/form_language/form_model/form_model.hpp"
#include "src/core/model_inspection/model_inspection.hpp"
#include "src/core/model_inspection/model_tool_specs.hpp"
#include "src/core/theme_language/diagnostics/theme_diagnostics.hpp"
#include "src/core/theme_language/source_text/theme_source_text.hpp"

#include "gui_forms/allocation/statistics/allocation_statistics.hpp"
#include "gui_forms/binding/form_document_export/form_document_export.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/user_control.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/value/value.hpp"
#include "gui_forms/window/window.hpp"

#include <iostream>
#include <optional>
#include <variant>
#include <memory>
#include <string>
#include <string_view>

namespace {

namespace fl = gui_forms::form_language;
namespace mi = gui_forms::model_inspection;
using gui_forms::SemanticNode;
using gui_forms::SemanticRole;
using gui_forms::SemanticSnapshot;
using gui_forms::SemanticState;
using gui_forms::theme_language::ThemeDiagnostic;
using gui_forms::theme_language::ThemeDiagnosticList;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part) {
    const bool found = text.find(part) != std::string_view::npos;
    return found;
}

[[nodiscard]] SemanticState states(std::uint32_t bits) {
    const SemanticState made = static_cast<SemanticState>(bits);
    return made;
}

[[nodiscard]] std::uint32_t bit(SemanticState state) {
    const std::uint32_t value = static_cast<std::uint32_t>(state);
    return value;
}

void test_state_summary() {
    SemanticNode window{};
    window.role = SemanticRole::pane;
    window.name = "Settings";
    window.states = states(bit(SemanticState::visible) | bit(SemanticState::enabled));
    SemanticNode save{};
    save.role = SemanticRole::button;
    save.name = "Save";
    save.states = states(bit(SemanticState::visible));
    save.actions.push_back(gui_forms::SemanticAction::press);
    SemanticNode password{};
    password.role = SemanticRole::text_box;
    password.name = "Password";
    password.value = "hunter2";
    password.states = states(bit(SemanticState::visible) | bit(SemanticState::enabled) |
                             bit(SemanticState::focused) | bit(SemanticState::protected_content));
    SemanticNode autosave{};
    autosave.role = SemanticRole::check_box;
    autosave.name = "Autosave";
    autosave.states = states(bit(SemanticState::visible) | bit(SemanticState::enabled) | bit(SemanticState::checked));
    window.children.push_back(save);
    window.children.push_back(password);
    window.children.push_back(autosave);
    SemanticSnapshot snapshot{};
    snapshot.node_count = 4U;
    snapshot.roots.push_back(window);
    const std::string summary = mi::window_state_summary(snapshot);
    require(contains(summary, "\"Settings\" of 4 nodes"), "summary: the window and its size");
    require(contains(summary, "Focus: ") && contains(summary, "\"Password\""), "summary: what has focus");
    require(contains(summary, "Checked: ") && contains(summary, "\"Autosave\""), "summary: what is checked");
    require(contains(summary, "Disabled: ") && contains(summary, "\"Save\""), "summary: a disabled button");
    require(contains(summary, "Protected fields: 1 filled"), "summary: a protected field is counted");
    require(!contains(summary, "hunter2"), "summary: a protected value is never written");
    const std::string short_summary = mi::window_state_summary(snapshot, 80U);
    require(short_summary.size() <= 80U && contains(short_summary, "..."), "summary: bounded, with an ellipsis");
    require(contains(mi::window_state_summary(SemanticSnapshot{}), "no semantic nodes"), "summary: an empty snapshot");
}

[[nodiscard]] fl::FormSchema sample_schema() {
    fl::FormSchema schema{};
    schema.types.push_back(fl::form_control_type<gui_forms::UserControl>("user_control"));
    schema.types.push_back(fl::form_control_type<gui_forms::Button>("button"));
    schema.types.push_back(fl::form_control_type<gui_forms::CheckBox>("check_box"));
    return schema;
}

// Reads and loads `text`; null when either refuses.
[[nodiscard]] gui_forms::Control::Ptr load(std::string_view text, const fl::FormSchema& schema,
                                           const fl::FormSources& sources, fl::LoadedForm& loaded) {
    gui_forms::theme_language::ThemeSource source{};
    source.file = "test.form";
    source.text = text;
    fl::FormSyntax syntax{};
    fl::FormModel model{};
    ThemeDiagnosticList diagnostics{};
    if (fl::read_form_model(source, syntax, schema, model, diagnostics) != fl::FormModelOutcome::model_built) {
        return gui_forms::Control::Ptr();
    }
    if (fl::load_form(source, schema, model, sources, loaded, diagnostics) != fl::FormLoadOutcome::loaded) {
        return gui_forms::Control::Ptr();
    }
    return loaded.root;
}

void test_layout_and_resources() {
    const fl::FormSchema schema = sample_schema();
    const fl::FormSources sources{};
    fl::LoadedForm loaded{};
    const gui_forms::Control::Ptr root =
        load("<user-control xmlns=\"urn:gui-forms:form:1\" id=\"t\" bounds=\"0 0 100 50\">\n"
             "  <button id=\"t.wide\" text=\"Wide\" bounds=\"60 10 80 20\" minimum-size=\"80 20\"/>\n"
             "</user-control>\n",
             schema, sources, loaded);
    require(root != nullptr, "layout: the sample loads");
    if (root == nullptr) return;
    gui_forms::Window window(root, gui_forms::Size{100.0, 50.0});
    window.perform_layout();
    const gui_forms::Control::Ptr wide = (*root).children()[0];
    const std::string reasons = mi::layout_reasons(*wide);
    require(contains(reasons, "t.wide: arranged") && contains(reasons, " in t"), "layout: the control and its parent");
    require(contains(reasons, "minimum_size = "), "layout: a locally set constraint is listed");
    require(contains(reasons, "tight: the width is held at minimum_size"), "layout: a tight minimum is named");
    require(contains(reasons, "overflows its parent on the right by 40"), "layout: the overflow is measured");
    const std::string resources = mi::resource_summary(window.image_resource_snapshot(), gui_forms::allocation_statistics());
    require(contains(resources, "images: ") && contains(resources, "text_layout: "), "resources: images and subsystems");
}

void test_form_export() {
    const fl::FormSchema schema = sample_schema();
    gui_forms::Value<bool> autosave{true};
    fl::FormSources sources{};
    sources.add("autosave_", autosave);
    const std::string original = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"app\">\n"
                                 "  <check-box id=\"app.autosave\"><bind property=\"checked\" to=\"autosave_\"/></check-box>\n"
                                 "</user-control>\n";
    fl::LoadedForm loaded{};
    const gui_forms::Control::Ptr root = load(original, schema, sources, loaded);
    require(root != nullptr, "forms: the original loads");
    if (root == nullptr) return;
    gui_forms::Window window(root, gui_forms::Size{200.0, 100.0});
    fl::FormExportRegistry registry{};
    std::string exported{};
    require(registry.export_document(*root, exported) == fl::FormExportOutcome::not_registered,
            "forms: an unregistered window is reported");
    require(registry.add(root, "app.form", original, sources), "forms: registered");
    require(registry.export_document(*root, exported) == fl::FormExportOutcome::done && exported == original,
            "forms: export returns the registered document");
    const std::string candidate = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"app\">\n"
                                  "  <check-box id=\"app.autosave\"><bind property=\"checked\" to=\"autosave_\"/></check-box>\n"
                                  "  <button id=\"app.go\" text=\"Go\"/>\n"
                                  "</user-control>\n";
    ThemeDiagnosticList diagnostics{};
    require(registry.apply(*root, schema, "candidate.form", candidate, std::string_view(), diagnostics) ==
                fl::FormExportOutcome::done,
            "forms: a valid candidate applies");
    require((*root).children().size() == 2U, "forms: the window has the candidate's children");
    require(registry.replaced(*root), "forms: the window is marked replaced");
    require(registry.export_document(*root, exported) == fl::FormExportOutcome::done && exported == candidate,
            "forms: export returns the applied document");
    autosave.set(false);
    // The bound check box still follows the program's value.
    const gui_forms::Control::Ptr check = (*root).children()[0];
    const std::optional<gui_forms::BindingValue> checked = (*check).property_value("checked");
    require(checked && std::get_if<bool>(&*checked) != nullptr && !std::get<bool>(*checked),
            "forms: the applied document keeps its binding to the program's value");
    // A candidate naming an unexposed source, a bad value, or another root.
    const std::string_view unsourced = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"app\">\n"
                                       "  <check-box id=\"app.x\"><bind property=\"checked\" to=\"secret_\"/></check-box>\n"
                                       "</user-control>\n";
    ThemeDiagnosticList refused{};
    require(registry.apply(*root, schema, "bad.form", unsourced, std::string_view(), refused) == fl::FormExportOutcome::refused,
            "forms: a binding to an unexposed source is refused");
    require((*root).children().size() == 2U, "forms: a refused candidate leaves the window as it was");
    const std::string_view renamed = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"other\"/>\n";
    ThemeDiagnosticList refused_root{};
    require(registry.apply(*root, schema, "bad.form", renamed, std::string_view(), refused_root) ==
                fl::FormExportOutcome::refused,
            "forms: a candidate whose root has another stable ID is refused");
    // A part named by a stable ID that is not under the window's root.
    ThemeDiagnosticList missing{};
    require(registry.apply(*root, schema, "part.form", candidate, "app.nothing", missing) ==
                fl::FormExportOutcome::target_not_found,
            "forms: a target that names no control");
    ThemeDiagnosticList reverted{};
    require(registry.revert(*root, schema, reverted) == fl::FormExportOutcome::done, "forms: revert");
    require((*root).children().size() == 1U && !registry.replaced(*root), "forms: back to the registered form");
    registry.forget(*root);
    require(!registry.registered(*root), "forms: forgotten");
    require(mi::forms_tool_specs[2].destructive && mi::forms_tool_specs[0].read_only,
            "forms: apply is destructive, export read-only");
}

// The build toggle's entry point: generated code's register_form_document
// lands in the process registry the forms tools read.
void test_register_form_document() {
    const fl::FormSchema schema = sample_schema();
    gui_forms::Value<bool> autosave{true};
    fl::FormSources sources{};
    sources.add("autosave_", autosave);
    const std::string document = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"reg\">\n"
                                 "  <check-box id=\"reg.autosave\"><bind property=\"checked\" to=\"autosave_\"/></check-box>\n"
                                 "</user-control>\n";
    fl::LoadedForm loaded{};
    const gui_forms::Control::Ptr root = load(document, schema, sources, loaded);
    require(root != nullptr, "register: the form loads");
    if (root == nullptr) return;
    const gui_forms::FormDocumentSource registered[] = {gui_forms::form_document_source("autosave_", 9U, autosave)};
    require(gui_forms::register_form_document(root, "reg.form", 8U, document.data(), document.size(), registered, 1U) ==
                gui_forms::FormDocumentRegistration::registered,
            "register: registered");
    std::string exported{};
    require(fl::process_form_exports().export_document(*root, exported) == fl::FormExportOutcome::done &&
                exported == document,
            "register: the process registry exports it");
    ThemeDiagnosticList diagnostics{};
    require(fl::process_form_exports().validate(*root, schema, "reg.form", document, diagnostics) ==
                fl::FormExportOutcome::done,
            "register: its own document validates against its registered sources");
    const gui_forms::FormDocumentSource twice[] = {gui_forms::form_document_source("autosave_", 9U, autosave),
                                                   gui_forms::form_document_source("autosave_", 9U, autosave)};
    require(gui_forms::register_form_document(root, "reg.form", 8U, document.data(), document.size(), twice, 2U) ==
                gui_forms::FormDocumentRegistration::refused,
            "register: a name given twice is refused");
    const char broken[] = {static_cast<char>(0xC3)};
    require(gui_forms::register_form_document(root, "reg.form", 8U, broken, 1U, nullptr, 0U) ==
                gui_forms::FormDocumentRegistration::refused,
            "register: a document that is not UTF-8 is refused");
    fl::process_form_exports().forget(*root);
}

// State hunt S2: a record borrows the program members its sources name,
// which need outlive the form's controls only until those are disposed. A
// root disposed but still held (an inspection, a C ABI handle) is no longer
// registered, so a candidate is never loaded against members the program
// may already have destroyed: here the members end right after the dispose.
void test_disposed_root_ends_registration() {
    const fl::FormSchema schema = sample_schema();
    const std::string document = "<user-control xmlns=\"urn:gui-forms:form:1\" id=\"gone\">\n"
                                 "  <check-box id=\"gone.autosave\"><bind property=\"checked\" to=\"autosave_\"/></check-box>\n"
                                 "</user-control>\n";
    fl::FormExportRegistry registry{};
    gui_forms::Control::Ptr root{};
    {
        std::unique_ptr<gui_forms::Value<bool>> autosave = std::make_unique<gui_forms::Value<bool>>(true);
        std::unique_ptr<fl::FormSources> sources = std::make_unique<fl::FormSources>();
        (*sources).add("autosave_", *autosave);
        fl::LoadedForm loaded{};
        root = load(document, schema, *sources, loaded);
        require(root != nullptr, "disposed root: the form loads");
        if (root == nullptr) return;
        require(registry.add(root, "gone.form", document, *sources), "disposed root: registered");
        require(registry.registered(*root), "disposed root: registered while alive");
        (*root).dispose();
        require(!registry.registered(*root), "disposed root: a disposed root is no longer registered");
        // The record ended with the dispose: the sources and the member they
        // borrow go now, while the root is still held.
    }
    ThemeDiagnosticList diagnostics{};
    require(registry.apply(*root, schema, "gone.form", document, std::string_view(), diagnostics) ==
                fl::FormExportOutcome::not_registered,
            "disposed root: apply loads nothing against the ended members");
    std::string exported{};
    require(registry.export_document(*root, exported) == fl::FormExportOutcome::not_registered,
            "disposed root: export reports it unregistered");
}

} // namespace

int main() {
    test_state_summary();
    test_layout_and_resources();
    test_form_export();
    test_register_form_document();
    test_disposed_root_ends_registration();
    if (failures != 0) {
        std::cerr << failures << " model inspection checks failed\n";
        return 1;
    }
    std::cout << "model inspection: state summary, layout reasons, resources and forms export passed\n";
    return 0;
}
