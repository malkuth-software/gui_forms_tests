// Form documents' editor (stage FD, phase FD4; XML since cp34; binding
// elements since 2026-11-21). Gate: a scripted edit of one property or one
// binding changes only that attribute's or that `bind` element's bytes;
// adding, removing and moving objects change only their own lines; comments
// and the document's own layout survive every edit; an edit that would make
// an invalid document is refused and leaves the document as it was.

#include "src/core/form_language/form_editor/form_editor.hpp"
#include "src/core/form_language/form_model/form_model.hpp"
#include "src/core/theme_language/diagnostics/theme_diagnostics.hpp"

#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/check_box.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/controls/user_control.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

using gui_forms::form_language::FormBindingText;
using gui_forms::form_language::FormDocument;
using gui_forms::form_language::FormEditOutcome;
using gui_forms::form_language::FormSchema;
using gui_forms::form_language::add_form_object;
using gui_forms::form_language::form_control_type;
using gui_forms::form_language::form_root_type;
using gui_forms::form_language::move_form_object;
using gui_forms::form_language::open_form_document;
using gui_forms::form_language::remove_form_object;
using gui_forms::form_language::reset_form_property;
using gui_forms::form_language::set_form_binding;
using gui_forms::form_language::set_form_property;
using gui_forms::theme_language::DiagnosticCode;
using gui_forms::theme_language::ThemeDiagnosticList;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

FormSchema sample_schema() {
    FormSchema schema{};
    schema.types.push_back(form_root_type());
    schema.types.push_back(form_control_type<gui_forms::UserControl>("user_control"));
    schema.types.push_back(form_control_type<gui_forms::Button>("button"));
    schema.types.push_back(form_control_type<gui_forms::CheckBox>("check_box"));
    schema.types.push_back(form_control_type<gui_forms::TextBox>("text_box"));
    return schema;
}

const std::string_view base =
    "<form xmlns=\"urn:gui-forms:form:1\" id=\"app.window\" text=\"Main\" client-size=\"400 300\">\n"
    "  <!-- the body -->\n"
    "  <user-control id=\"app.body\">\n"
    "    <button id=\"app.ok\" text=\"OK\"/>\n"
    "    <button id=\"app.cancel\"\n"
    "        text=\"Cancel\"/>\n"
    "  </user-control>\n"
    "</form>\n";

FormDocument open(const FormSchema& schema, std::string_view text) {
    FormDocument document{};
    ThemeDiagnosticList diagnostics{};
    const FormEditOutcome opened = open_form_document(document, "test.form", std::string(text), schema, diagnostics);
    require(opened == FormEditOutcome::edited, "the document opens");
    return document;
}

// `text` with `expected` where `original` was: the bytes around it equal.
std::string replaced(std::string_view text, std::string_view original, std::string_view expected) {
    std::string result(text);
    const std::size_t at = result.find(original);
    require(at != std::string::npos, "the expected bytes are in the base");
    if (at != std::string::npos) result.replace(at, original.size(), expected);
    return result;
}

void check_text(const FormDocument& document, const std::string& expected, std::string_view label) {
    if (document.text != expected) std::cerr << "  " << label << ":\n" << document.text << "---\n" << expected;
    require(document.text == expected, label);
}

// Whether the model holds a text property of value `text`.
bool model_has_text(const FormDocument& document, std::string_view text) {
    for (const gui_forms::form_language::FormProperty& property : document.model.properties) {
        const std::string* value = std::get_if<std::string>(&property.value);
        if (value != nullptr && *value == text) return true;
    }
    return false;
}

void test_properties() {
    const FormSchema schema = sample_schema();
    ThemeDiagnosticList diagnostics{};

    FormDocument changed = open(schema, base);
    require(set_form_property(changed, "app.cancel", "text", "Close", schema, diagnostics) == FormEditOutcome::edited,
            "property: a written value replaced");
    check_text(changed, replaced(base, "\"Cancel\"", "\"Close\""), "property: only the value's bytes change");
    require(changed.model.properties.size() == 4U, "property: the model follows the text");

    FormDocument added = open(schema, base);
    require(set_form_property(added, "app.cancel", "tab-index", "2", schema, diagnostics) == FormEditOutcome::edited,
            "property: an unwritten one added");
    check_text(added, replaced(base, "        text=\"Cancel\"/>", "        text=\"Cancel\"\n        tab-index=\"2\"/>"),
               "property: a line of its own after the last attribute, aligned with it");

    FormDocument inline_added = open(schema, base);
    require(set_form_property(inline_added, "app.ok", "tab-index", "1", schema, diagnostics) ==
                FormEditOutcome::edited,
            "property: added to a one-line tag");
    check_text(inline_added, replaced(base, "text=\"OK\"/>", "text=\"OK\" tab-index=\"1\"/>"),
               "property: kept on the tag's line");

    FormDocument reset = open(schema, base);
    require(reset_form_property(reset, "app.cancel", "text", schema, diagnostics) == FormEditOutcome::edited,
            "property: reset");
    check_text(reset, replaced(base, "\n        text=\"Cancel\"/>", "/>"),
               "property: the reset takes the whitespace before it");
    require(reset_form_property(reset, "app.cancel", "text", schema, diagnostics) == FormEditOutcome::unchanged,
            "property: resetting an unwritten property changes nothing");
    require(reset_form_property(reset, "app.cancel", "id", schema, diagnostics) == FormEditOutcome::not_admitted,
            "property: the stable ID is no property");

    // Refusals leave the document as it was.
    FormDocument refused = open(schema, base);
    diagnostics.clear();
    require(set_form_property(refused, "app.ok", "tab-index", "first", schema, diagnostics) ==
                FormEditOutcome::refused,
            "property: a value of the wrong kind refused");
    require(diagnostics.records().size() == 1U &&
                diagnostics.records()[0].code == DiagnosticCode::form_value_invalid,
            "property: the refusal's diagnostic");
    require(set_form_property(refused, "app.ok", "Text", "x", schema, diagnostics) == FormEditOutcome::not_admitted,
            "property: a name that is not kebab-case");
    require(set_form_property(refused, "app.ok", "id", "app.other", schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "property: the stable ID is not set as a property");
    require(set_form_property(refused, "app.ok", "xmlns", "urn:x", schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "property: the namespace is not set as a property");
    require(set_form_property(refused, "app.none", "text", "x", schema, diagnostics) ==
                FormEditOutcome::object_missing,
            "property: an unknown object");
    check_text(refused, std::string(base), "property: the refused document is unchanged");

    // A text is escaped for its attribute: the markup characters as
    // entities, a line feed as a character reference; the model reads the
    // text back. A value cannot end its attribute.
    FormDocument escaped = open(schema, base);
    require(set_form_property(escaped, "app.cancel", "text", "Close\n\"now\" <a> & 'b'", schema, diagnostics) ==
                FormEditOutcome::edited,
            "property: a text with markup characters written");
    check_text(escaped, replaced(base, "\"Cancel\"", "\"Close&#10;&quot;now&quot; &lt;a> &amp; 'b'\""),
               "property: escaped for the double-quoted attribute");
    require(model_has_text(escaped, "Close\n\"now\" <a> & 'b'"), "property: the model reads the text back");
    const std::string single_quoted = replaced(base, "text=\"Cancel\"", "text='Cancel'");
    FormDocument single = open(schema, single_quoted);
    require(set_form_property(single, "app.cancel", "text", "it's \"so\"", schema, diagnostics) ==
                FormEditOutcome::edited,
            "property: a single-quoted attribute set");
    check_text(single, replaced(single_quoted, "'Cancel'", "'it&apos;s \"so\"'"),
               "property: escaped for the single-quoted attribute, its quote kept");
    // An attribute is always a literal value: a text that begins with
    // `bind(` is that text, written as it is (no escape).
    FormDocument literal = open(schema, base);
    require(set_form_property(literal, "app.ok", "text", "bind(x) is a call", schema, diagnostics) ==
                FormEditOutcome::edited,
            "property: a text beginning with bind( written");
    check_text(literal, replaced(base, "\"OK\"", "\"bind(x) is a call\""), "property: written as it is");
    require(model_has_text(literal, "bind(x) is a call"), "property: read back as text, not as a binding");

    // An `on-` handler is refused (owner ruling FD-6 R6), leaving the
    // document as it was.
    FormDocument handler = open(schema, base);
    diagnostics.clear();
    require(set_form_property(handler, "app.ok", "on-clicked", "accept", schema, diagnostics) ==
                FormEditOutcome::refused,
            "property: an on- handler refused");
    require(diagnostics.records().size() == 1U &&
                diagnostics.records()[0].code == DiagnosticCode::form_handler_removed,
            "property: the handler's diagnostic, E816");
    check_text(handler, std::string(base), "property: the refused document is unchanged");
}

// A document with bindings: a comment beside them, a `bind` whose `to`
// comes before its `property`, and one whose options come first.
const std::string_view bound_base =
    "<form xmlns=\"urn:gui-forms:form:1\" id=\"app.window\" text=\"Main\">\n"
    "  <user-control id=\"app.body\">\n"
    "    <check-box id=\"app.flag\" tab-index=\"1\">\n"
    "      <!-- the flag follows the model -->\n"
    "      <bind to=\"flag_\" property=\"checked\" mode=\"one-way\"/>\n"
    "      <bind property=\"text\" to=\"names_\" field=\"title\"/>\n"
    "    </check-box>\n"
    "    <text-box id=\"app.name\"><bind mode=\"two-way\" update=\"on-change\" property=\"text\" to=\"names_\" "
    "field=\"name\"/></text-box>\n"
    "  </user-control>\n"
    "</form>\n";

// Whether the model binds a property to the source `source`.
bool model_binds(const FormDocument& document, std::string_view source) {
    for (const gui_forms::form_language::FormProperty& property : document.model.properties) {
        if (property.binding && (*property.binding).source == source) return true;
    }
    return false;
}

// Bindings are `bind` elements (owner, 2026-11-21): set_form_binding adds
// one or changes only the bytes of the attributes that change; setting a
// property replaces its binding and binding it replaces its attribute;
// a reset removes either; comments and attribute order survive.
void test_bindings() {
    const FormSchema schema = sample_schema();
    ThemeDiagnosticList diagnostics{};

    FormDocument added = open(schema, base);
    require(set_form_binding(added, "app.ok", "command", FormBindingText{.to = "accept_"}, schema, diagnostics) ==
                FormEditOutcome::edited,
            "binding: added to an empty element");
    check_text(added,
               replaced(base, "    <button id=\"app.ok\" text=\"OK\"/>\n",
                        "    <button id=\"app.ok\" text=\"OK\">\n"
                        "      <bind property=\"command\" to=\"accept_\"/>\n"
                        "    </button>\n"),
               "binding: the element opened, the bind two spaces deeper");
    require(model_binds(added, "accept_"), "binding: the model reads the binding");

    FormDocument source = open(schema, bound_base);
    require(set_form_binding(source, "app.flag", "checked", FormBindingText{.to = "other_", .mode = "one-way"}, schema,
                             diagnostics) == FormEditOutcome::edited,
            "binding: the source changed");
    check_text(source, replaced(bound_base, "to=\"flag_\"", "to=\"other_\""),
               "binding: only the source's bytes change, the comment and attribute order kept");
    require(model_binds(source, "other_") && !model_binds(source, "flag_"), "binding: the model follows");

    FormDocument unchanged = open(schema, bound_base);
    require(set_form_binding(unchanged, "app.flag", "checked", FormBindingText{.to = "flag_", .mode = "one-way"},
                             schema, diagnostics) == FormEditOutcome::unchanged,
            "binding: the same binding changes nothing");

    FormDocument widened = open(schema, bound_base);
    require(set_form_binding(widened, "app.flag", "checked", FormBindingText{.to = "flag_"}, schema, diagnostics) ==
                FormEditOutcome::edited,
            "binding: an option removed");
    check_text(widened, replaced(bound_base, " mode=\"one-way\"", ""), "binding: the option takes its space");

    FormDocument field = open(schema, bound_base);
    require(set_form_binding(field, "app.flag", "checked",
                             FormBindingText{.to = "flag_", .field = "done", .mode = "one-way"}, schema,
                             diagnostics) == FormEditOutcome::edited,
            "binding: an attribute added");
    check_text(field, replaced(bound_base, "mode=\"one-way\"/>", "mode=\"one-way\" field=\"done\"/>"),
               "binding: after the element's last attribute");

    FormDocument leading = open(schema, bound_base);
    require(set_form_binding(leading, "app.name", "text", FormBindingText{.to = "names_", .field = "name"}, schema,
                             diagnostics) == FormEditOutcome::edited,
            "binding: the first two attributes removed");
    check_text(leading, replaced(bound_base, "mode=\"two-way\" update=\"on-change\" ", ""),
               "binding: the run up to the attribute after it");

    FormDocument another = open(schema, bound_base);
    require(set_form_binding(another, "app.flag", "visible", FormBindingText{.to = "shown_"}, schema, diagnostics) ==
                FormEditOutcome::edited,
            "binding: a second property bound");
    check_text(another,
               replaced(bound_base, "field=\"title\"/>\n",
                        "field=\"title\"/>\n      <bind property=\"visible\" to=\"shown_\"/>\n"),
               "binding: after the object's last bind, aligned with it");

    // Setting a bound property removes its binding, and binding a set one
    // removes its attribute.
    FormDocument set = open(schema, bound_base);
    require(set_form_property(set, "app.flag", "checked", "true", schema, diagnostics) == FormEditOutcome::edited,
            "binding: a bound property set");
    check_text(set,
               replaced(replaced(bound_base, "tab-index=\"1\">", "tab-index=\"1\" checked=\"true\">"),
                        "      <bind to=\"flag_\" property=\"checked\" mode=\"one-way\"/>\n", ""),
               "binding: the bind's line removed, the comment kept");
    FormDocument rebound = open(schema, base);
    require(set_form_binding(rebound, "app.cancel", "text", FormBindingText{.to = "names_", .field = "label"},
                             schema, diagnostics) == FormEditOutcome::edited,
            "binding: a set property bound");
    check_text(rebound,
               replaced(base, "    <button id=\"app.cancel\"\n        text=\"Cancel\"/>\n",
                        "    <button id=\"app.cancel\">\n"
                        "      <bind property=\"text\" to=\"names_\" field=\"label\"/>\n"
                        "    </button>\n"),
               "binding: the attribute removed, the element opened");

    FormDocument reset = open(schema, bound_base);
    require(reset_form_property(reset, "app.flag", "checked", schema, diagnostics) == FormEditOutcome::edited,
            "binding: reset");
    check_text(reset, replaced(bound_base, "      <bind to=\"flag_\" property=\"checked\" mode=\"one-way\"/>\n", ""),
               "binding: the reset takes the bind's line");
    require(reset_form_property(reset, "app.flag", "checked", schema, diagnostics) == FormEditOutcome::unchanged,
            "binding: resetting it again changes nothing");

    // Refusals leave the document as it was.
    FormDocument refused = open(schema, bound_base);
    diagnostics.clear();
    require(set_form_binding(refused, "app.flag", "tab-index", FormBindingText{.to = "n_"}, schema, diagnostics) ==
                FormEditOutcome::refused,
            "binding: a property that is not bindable refused");
    require(diagnostics.records().size() == 1U &&
                diagnostics.records()[0].code == DiagnosticCode::form_binding_invalid,
            "binding: its diagnostic, E817");
    diagnostics.clear();
    require(set_form_binding(refused, "app.flag", "text",
                             FormBindingText{.to = "names_", .field = "title", .update = "on-change"}, schema,
                             diagnostics) == FormEditOutcome::refused,
            "binding: update on a one-way binding refused");
    require(diagnostics.records().size() == 1U &&
                diagnostics.records()[0].code == DiagnosticCode::form_binding_option_invalid,
            "binding: its diagnostic, E818");
    require(set_form_binding(refused, "app.flag", "checked", FormBindingText{}, schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "binding: a binding without a source");
    require(set_form_binding(refused, "app.flag", "Checked", FormBindingText{.to = "flag_"}, schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "binding: a name that is not kebab-case");
    require(set_form_binding(refused, "app.none", "checked", FormBindingText{.to = "flag_"}, schema, diagnostics) ==
                FormEditOutcome::object_missing,
            "binding: an unknown object");
    check_text(refused, std::string(bound_base), "binding: the refused document is unchanged");

    // A bind is no object: objects are placed among the objects only.
    FormDocument placed = open(schema, bound_base);
    require(add_form_object(placed, "app.flag", 0U, "button", "app.inner", schema, diagnostics) ==
                FormEditOutcome::edited,
            "binding: an object added beside the binds");
    check_text(placed,
               replaced(bound_base, "field=\"title\"/>\n",
                        "field=\"title\"/>\n      <button id=\"app.inner\"/>\n"),
               "binding: the first object after the last bind");
    require(add_form_object(placed, "app.flag", 0U, "bind", "app.bind", schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "binding: a bind is no object");
}

void test_objects() {
    const FormSchema schema = sample_schema();
    ThemeDiagnosticList diagnostics{};

    FormDocument between = open(schema, base);
    require(add_form_object(between, "app.body", 1U, "check-box", "app.check", schema, diagnostics) ==
                FormEditOutcome::edited,
            "object: added between two");
    check_text(between,
               replaced(base, "    <button id=\"app.cancel\"\n",
                        "    <check-box id=\"app.check\"/>\n    <button id=\"app.cancel\"\n"),
               "object: a line of its own before the next");
    require(set_form_property(between, "app.check", "text", "Go", schema, diagnostics) == FormEditOutcome::edited,
            "object: a property on the new element");
    check_text(between,
               replaced(base, "    <button id=\"app.cancel\"\n",
                        "    <check-box id=\"app.check\" text=\"Go\"/>\n    <button id=\"app.cancel\"\n"),
               "object: the new element's line keeps it");

    FormDocument appended = open(schema, base);
    require(add_form_object(appended, "app.body", 2U, "check-box", "app.check", schema, diagnostics) ==
                FormEditOutcome::edited,
            "object: appended");
    check_text(appended,
               replaced(base, "text=\"Cancel\"/>\n", "text=\"Cancel\"/>\n    <check-box id=\"app.check\"/>\n"),
               "object: after the last child");
    require(add_form_object(appended, "app.body", 9U, "check-box", "app.more", schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "object: a position past the children");
    require(add_form_object(appended, "app.body", 0U, "check-box", "app.check", schema, diagnostics) ==
                FormEditOutcome::refused,
            "object: a repeated stable ID refused");
    require(add_form_object(appended, "app.body", 0U, "slider", "app.slide", schema, diagnostics) ==
                FormEditOutcome::refused,
            "object: an unknown type refused");
    require(add_form_object(appended, "app.body", 0U, "span", "app.span", schema, diagnostics) ==
                FormEditOutcome::not_admitted,
            "object: a span is no object");

    // An empty-element parent is opened around its first child.
    const std::string empty_body = "<form xmlns=\"urn:gui-forms:form:1\" id=\"app.window\">\n"
                                   "  <user-control id=\"app.body\"/>\n"
                                   "</form>\n";
    FormDocument opened = open(schema, empty_body);
    require(add_form_object(opened, "app.body", 0U, "button", "app.ok", schema, diagnostics) ==
                FormEditOutcome::edited,
            "object: the first child of an empty element");
    check_text(opened,
               replaced(empty_body, "  <user-control id=\"app.body\"/>\n",
                        "  <user-control id=\"app.body\">\n    <button id=\"app.ok\"/>\n  </user-control>\n"),
               "object: the parent opened, the child two spaces deeper");

    FormDocument removed = open(schema, base);
    require(remove_form_object(removed, "app.ok", schema, diagnostics) == FormEditOutcome::edited, "object: removed");
    check_text(removed, replaced(base, "    <button id=\"app.ok\" text=\"OK\"/>\n", ""),
               "object: the removal takes its line");
    require(remove_form_object(removed, "app.window", schema, diagnostics) == FormEditOutcome::not_admitted,
            "object: the root stays");
    require(remove_form_object(removed, "app.body", schema, diagnostics) == FormEditOutcome::refused,
            "object: a form left without its control refused");

    FormDocument moved = open(schema, base);
    require(move_form_object(moved, "app.ok", "app.body", 1U, schema, diagnostics) == FormEditOutcome::edited,
            "object: moved");
    const std::string expected_move =
        "<form xmlns=\"urn:gui-forms:form:1\" id=\"app.window\" text=\"Main\" client-size=\"400 300\">\n"
        "  <!-- the body -->\n"
        "  <user-control id=\"app.body\">\n"
        "    <button id=\"app.cancel\"\n"
        "        text=\"Cancel\"/>\n"
        "    <button id=\"app.ok\" text=\"OK\"/>\n"
        "  </user-control>\n"
        "</form>\n";
    check_text(moved, expected_move, "object: moved after its sibling, its bytes and the comment kept");
    require(move_form_object(moved, "app.body", "app.ok", 0U, schema, diagnostics) == FormEditOutcome::not_admitted,
            "object: not into itself");
}

// A CRLF document's insertions use CRLF.
void test_line_endings() {
    const FormSchema schema = sample_schema();
    ThemeDiagnosticList diagnostics{};
    std::string crlf{};
    for (const char character : base) {
        if (character == '\n') crlf.push_back('\r');
        crlf.push_back(character);
    }
    FormDocument document = open(schema, crlf);
    require(set_form_property(document, "app.cancel", "tab-index", "2", schema, diagnostics) ==
                FormEditOutcome::edited,
            "line endings: added");
    check_text(document,
               replaced(crlf, "        text=\"Cancel\"/>", "        text=\"Cancel\"\r\n        tab-index=\"2\"/>"),
               "line endings: the document's own");
}

// Comments survive every edit (the editor splices bytes and never writes
// the document again from the model): around a removed element, and
// between the attributes' element and its neighbours.
void test_comments() {
    const FormSchema schema = sample_schema();
    ThemeDiagnosticList diagnostics{};
    const std::string commented =
        replaced(base, "    <button id=\"app.ok\" text=\"OK\"/>\n",
                 "    <!-- before -->\n    <button id=\"app.ok\" text=\"OK\"/> <!-- after -->\n");
    FormDocument document = open(schema, commented);
    require(set_form_property(document, "app.ok", "text", "Yes", schema, diagnostics) == FormEditOutcome::edited,
            "comments: a commented element's value replaced");
    check_text(document, replaced(commented, "\"OK\"", "\"Yes\""), "comments: both comments kept");
    require(remove_form_object(document, "app.cancel", schema, diagnostics) == FormEditOutcome::edited,
            "comments: a neighbour removed");
    const std::string without = replaced(replaced(commented, "\"OK\"", "\"Yes\""),
                                         "    <button id=\"app.cancel\"\n        text=\"Cancel\"/>\n", "");
    check_text(document, without, "comments: every comment kept");
}

} // namespace

int main() {
    test_properties();
    test_bindings();
    test_objects();
    test_line_endings();
    test_comments();
    if (failures != 0) {
        std::cerr << failures << " form editor checks failed\n";
        return 1;
    }
    std::cout << "form editor: attributes set, added and reset, bind elements added, changed and replaced, objects "
                 "added, removed and moved, refusals, line endings and comments passed\n";
    return 0;
}
