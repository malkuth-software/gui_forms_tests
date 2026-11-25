// The public form documents (gui_forms/forms/form_documents): the registry
// schema, a document's check with the form compiler's diagnostics and their
// positions, and a document loaded into a window and read back.

#include "gui_forms/forms/form_documents/form_documents.hpp"

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <iostream>
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
    const std::string_view text(view.data(), static_cast<std::size_t>(view.count()));
    return text;
}

[[nodiscard]] gui_forms::FormDocumentCheck check(std::string_view document) {
    const gui_forms::FormDocumentSchema& schema = gui_forms::FormDocumentSchema::registry();
    return gui_forms::check_form_document(schema, "main.form", 9U, document.data(), document.size());
}

void test_schema() {
    const gui_forms::FormDocumentSchema& schema = gui_forms::FormDocumentSchema::registry();
    require(&schema == &gui_forms::FormDocumentSchema::registry(), "schema: the registry's is one object");
    require(schema.type_count() > 50U, "schema: the registry's types");
    const stxlib::usize button = schema.find("button", 6U);
    const stxlib::usize panel = schema.find("panel", 5U);
    const stxlib::usize command = schema.find("command", 7U);
    const stxlib::usize form = schema.find("form", 4U);
    require(button < schema.type_count() && panel < schema.type_count() && command < schema.type_count(),
            "schema: button, panel and command are found");
    require(form == schema.type_count() - 1U, "schema: the form root is last");
    require(schema.find("frobnicator", 11U) == schema.type_count(), "schema: an unknown name is not found");
    if (button >= schema.type_count() || panel >= schema.type_count() || command >= schema.type_count()) return;
    const gui_forms::FormDocumentType button_type = schema.type(button);
    require(text_of(button_type.name()) == "button", "schema: the type's name");
    require(schema.type(panel).holds_children() && !schema.type(command).holds_children(),
            "schema: a panel holds children, a command does not");
    bool has_text = false;
    const stxlib::View<gui_forms::PropertyDescriptor> properties = button_type.properties();
    for (stxlib::usize index = 0; index < properties.count(); ++index) {
        if (properties[index].name == "text") has_text = true;
    }
    require(has_text, "schema: a button's text property");
    bool has_clicked = false;
    for (stxlib::usize index = 0; index < button_type.interaction_count(); ++index) {
        if (text_of(button_type.interaction(index)) == "clicked") has_clicked = true;
    }
    require(has_clicked, "schema: a button's clicked interaction");
    require(button_type.interaction(button_type.interaction_count()).count() == 0U,
            "schema: an interaction past the end is empty");
}

void test_check() {
    const gui_forms::FormDocumentCheck valid = check("<form xmlns=\"urn:gui-forms:form:1\" id=\"main\">\n"
                                                     "  <panel id=\"main.root\">\n"
                                                     "    <button id=\"main.ok\" text=\"OK\"/>\n"
                                                     "  </panel>\n"
                                                     "</form>\n");
    require(valid.compiled() && valid.error_count() == 0U, "check: a valid document compiles");

    const gui_forms::FormDocumentCheck unknown = check("<form xmlns=\"urn:gui-forms:form:1\" id=\"main\">\n"
                                                       "  <panel id=\"main.root\">\n"
                                                       "    <frobnicator id=\"main.odd\"/>\n"
                                                       "  </panel>\n"
                                                       "</form>\n");
    require(!unknown.compiled() && unknown.error_count() >= 1U && unknown.count() >= 1U,
            "check: an unknown element is an error");
    bool located = false;
    for (stxlib::usize index = 0; index < unknown.count(); ++index) {
        const gui_forms::FormDocumentDiagnostic found = unknown.diagnostic(index);
        if (found.error && found.line == 3U && found.column > 0U && text_of(found.file) == "main.form" &&
            text_of(found.code_label).starts_with("E") && !text_of(found.code_name).empty() &&
            !text_of(found.detail).empty()) {
            located = true;
        }
    }
    require(located, "check: the error names line 3, a column, the file, its code and what is wrong");
    require(unknown.diagnostic(unknown.count()).line == 0U, "check: a diagnostic past the end is empty");

    const gui_forms::FormDocumentCheck value = check("<form xmlns=\"urn:gui-forms:form:1\" id=\"main\">\n"
                                                     "  <panel id=\"main.root\">\n"
                                                     "    <button id=\"main.ok\" enabled=\"perhaps\"/>\n"
                                                     "  </panel>\n"
                                                     "</form>\n");
    require(!value.compiled() && value.error_count() >= 1U && value.diagnostic(0U).line == 3U,
            "check: a value the property cannot take is an error on its line");

    const gui_forms::FormDocumentCheck broken = check("<form xmlns=\"urn:gui-forms:form:1\" id=\"main\">\n");
    require(!broken.compiled() && broken.count() >= 1U, "check: an unclosed document is refused");
    const std::string not_utf8 = std::string("<form xmlns=\"urn:gui-forms:form:1\" id=\"m\xff\"/>\n");
    const gui_forms::FormDocumentCheck bytes = check(not_utf8);
    require(!bytes.compiled() && bytes.count() >= 1U, "check: bytes that are not UTF-8 are a diagnostic");

    gui_forms::FormDocumentCheck moved = check("<form xmlns=\"urn:gui-forms:form:1\" id=\"main\">\n");
    const gui_forms::FormDocumentCheck taken = static_cast<gui_forms::FormDocumentCheck&&>(moved);
    require(taken.count() >= 1U && moved.count() == 0U && !moved.compiled(), "check: a moved-from check is empty");
}

void test_load() {
    const std::string_view document = "<form xmlns=\"urn:gui-forms:form:1\" id=\"settings.dialog\"\n"
                                      "    text=\"Settings\"\n"
                                      "    client-size=\"480 320\"\n"
                                      "    accept-button=\"settings.ok\">\n"
                                      "  <panel id=\"settings.body\">\n"
                                      "    <button id=\"settings.ok\" text=\"OK\"/>\n"
                                      "  </panel>\n"
                                      "</form>\n";
    const gui_forms::FormDocumentSchema& schema = gui_forms::FormDocumentSchema::registry();
    const gui_forms::FormDocumentWindow loaded =
        gui_forms::load_form_document_window(schema, "settings.form", 13U, document.data(), document.size());
    require(loaded.window() != nullptr && loaded.check().compiled() && loaded.check().error_count() == 0U,
            "load: the document is loaded");
    if (loaded.window() == nullptr) return;
    gui_forms::Window& window = *loaded.window();
    require(text_of(loaded.title()) == "Settings", "load: the title");
    require(window.client_size() == gui_forms::Size{480.0, 320.0}, "load: the client size");
    const gui_forms::Control::Ptr root = window.root();
    require(root != nullptr && (*root).stable_id().value() == "settings.body", "load: the form's control is the root");
    const gui_forms::ControlLookup ok = window.control("settings.ok");
    require(static_cast<bool>(ok) && window.accept_button() == ok.control, "load: the button, found by its ID");
    // The check and the document agree: what loads also compiles.
    require(check(document).compiled(), "load: the same document checks clean");

    const std::string_view bound = "<form xmlns=\"urn:gui-forms:form:1\" id=\"app\">\n"
                                   "  <panel id=\"app.root\">\n"
                                   "    <check-box id=\"app.flag\"><bind property=\"checked\" to=\"flag_\"/></check-box>\n"
                                   "  </panel>\n"
                                   "</form>\n";
    const gui_forms::FormDocumentWindow refused =
        gui_forms::load_form_document_window(schema, "app.form", 8U, bound.data(), bound.size());
    require(refused.window() == nullptr && !refused.check().compiled() && refused.check().error_count() >= 1U &&
                refused.check().diagnostic(0U).line == 3U,
            "load: a binding without a model is refused, located");
    const std::string_view control_root = "<panel xmlns=\"urn:gui-forms:form:1\" id=\"app.root\"/>\n";
    const gui_forms::FormDocumentWindow panel =
        gui_forms::load_form_document_window(schema, "panel.form", 10U, control_root.data(), control_root.size());
    require(panel.window() == nullptr, "load: a control root describes no window");
}

} // namespace

int main() {
    test_schema();
    test_check();
    test_load();
    if (failures != 0) {
        std::cerr << failures << " form documents checks failed\n";
        return 1;
    }
    std::cout << "form documents: schema, check and load passed\n";
    return 0;
}
