// A mockup's fixture data (src/core/form_language/form_fixture;
// docs/user/mockups.md): values, record sets and trees read with their
// kinds and default stable IDs, and each defect refused with its E8xx code
// at its line.

#include "src/core/form_language/form_fixture/form_fixture.hpp"
#include "src/core/theme_language/diagnostics/theme_diagnostics.hpp"
#include "src/core/theme_language/source_text/theme_source_text.hpp"

#include "gui_forms/binding/value/binding_value.hpp"
#include "gui_forms/binding/value/binding_value_kind/binding_value_kind.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

namespace fl = gui_forms::form_language;
namespace tl = gui_forms::theme_language;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

struct Read final {
    fl::FormFixtureOutcome outcome{fl::FormFixtureOutcome::refused};
    fl::FormFixture fixture{};
    tl::ThemeDiagnosticList diagnostics{};
};

void read(std::string_view text, Read& result) {
    tl::ThemeSource source{};
    source.file = "sample.fixture";
    source.text = text;
    result.outcome = fl::read_form_fixture(source, result.fixture, result.diagnostics);
}

// Whether the first diagnostic has `code` on `line`.
[[nodiscard]] bool first_is(const Read& result, tl::DiagnosticCode code, std::uint32_t line) {
    if (result.outcome != fl::FormFixtureOutcome::refused || result.diagnostics.records().empty()) return false;
    const tl::ThemeDiagnostic& first = result.diagnostics.records()[0];
    return first.code == code && first.position.line.value == line && first.file == "sample.fixture";
}

// Whether the result was refused with `code` first, wherever it is.
[[nodiscard]] bool first_code_is(const Read& result, tl::DiagnosticCode code) {
    if (result.outcome != fl::FormFixtureOutcome::refused || result.diagnostics.records().empty()) return false;
    return result.diagnostics.records()[0].code == code;
}

constexpr std::string_view head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<fixture xmlns=\"urn:gui-forms:fixture:1\">\n";

void test_empty() {
    Read empty{};
    read("", empty);
    require(empty.outcome == fl::FormFixtureOutcome::read && empty.fixture.values.empty() &&
                empty.fixture.records.empty() && empty.fixture.trees.empty(),
            "empty: no text is an empty fixture");
}

void test_values_and_records() {
    const std::string text = std::string(head) +
                             "  <value name=\"autosave\" value=\"true\"/>\n"
                             "  <records name=\"claims\" columns=\"claim amount:number paid:boolean count:integer\"\n"
                             "      shown-in=\"claims.grid claims.list\">\n"
                             "    <row values=\"C-1041 120.50 false 3\"/>\n"
                             "    <row id=\"late\" values=\"'C 1042' 7 true\"/>\n"
                             "  </records>\n"
                             "</fixture>\n";
    Read result{};
    read(text, result);
    require(result.outcome == fl::FormFixtureOutcome::read, "records: a valid fixture is read");
    if (result.outcome != fl::FormFixtureOutcome::read) return;
    require(result.fixture.values.size() == 1U && result.fixture.values[0].name == "autosave" &&
                result.fixture.values[0].text == "true" && result.fixture.values[0].position.line.value == 3U,
            "records: a value's name, text and line");
    require(result.fixture.records.size() == 1U, "records: one record set");
    if (result.fixture.records.size() != 1U) return;
    const fl::FixtureRecords& claims = result.fixture.records[0];
    require(claims.name == "claims" && claims.columns.size() == 4U, "records: the name and the columns");
    if (claims.columns.size() == 4U) {
        require(claims.columns[0].name == "claim" && claims.columns[0].kind == gui_forms::BindingValueKind::text &&
                    claims.columns[1].kind == gui_forms::BindingValueKind::number &&
                    claims.columns[2].kind == gui_forms::BindingValueKind::boolean &&
                    claims.columns[3].kind == gui_forms::BindingValueKind::signed_integer,
                "records: each column's kind");
    }
    require(claims.shown_in.size() == 2U && claims.shown_in[0] == "claims.grid" && claims.shown_in[1] == "claims.list",
            "records: shown-in lists stable IDs");
    require(claims.rows.size() == 2U, "records: two rows");
    if (claims.rows.size() != 2U) return;
    require(claims.rows[0].stable_id == "row-1" && claims.rows[1].stable_id == "late", "records: the rows' stable IDs");
    require(claims.rows[0].values.size() == 4U, "records: a full row has a value per column");
    if (claims.rows[0].values.size() == 4U) {
        const std::string* const claim = std::get_if<std::string>(&claims.rows[0].values[0]);
        const double* const amount = std::get_if<double>(&claims.rows[0].values[1]);
        const bool* const paid = std::get_if<bool>(&claims.rows[0].values[2]);
        require(claim != nullptr && *claim == "C-1041", "records: a text cell");
        require(amount != nullptr && *amount == 120.5, "records: a number cell");
        require(paid != nullptr && !*paid, "records: a boolean cell");
    }
    if (!claims.rows[1].values.empty()) {
        const std::string* const quoted = std::get_if<std::string>(&claims.rows[1].values[0]);
        require(quoted != nullptr && *quoted == "C 1042", "records: a quoted item keeps its space");
    }
}

void test_tree() {
    const std::string text = std::string(head) +
                             "  <tree shown-in=\"files.tree\">\n"
                             "    <item text=\"Documents\" expanded=\"true\">\n"
                             "      <item id=\"claim\" text=\"Claim.pdf\"/>\n"
                             "    </item>\n"
                             "    <item text=\"Pictures\"/>\n"
                             "  </tree>\n"
                             "</fixture>\n";
    Read result{};
    read(text, result);
    require(result.outcome == fl::FormFixtureOutcome::read && result.fixture.trees.size() == 1U, "tree: read");
    if (result.fixture.trees.size() != 1U) return;
    const fl::FixtureTree& tree = result.fixture.trees[0];
    require(tree.shown_in.size() == 1U && tree.shown_in[0] == "files.tree", "tree: shown-in");
    require(tree.items.size() == 3U, "tree: three items, flattened in order");
    if (tree.items.size() != 3U) return;
    require(tree.items[0].text == "Documents" && tree.items[0].depth == 0U && tree.items[0].expandable &&
                tree.items[0].expanded,
            "tree: an open parent");
    require(tree.items[1].stable_id == "claim" && tree.items[1].depth == 1U && !tree.items[1].expandable,
            "tree: a child with its own stable ID");
    require(tree.items[2].text == "Pictures" && tree.items[2].depth == 0U && !tree.items[2].stable_id.empty(),
            "tree: a default stable ID");
}

void test_refusals() {
    Read not_xml{};
    read(std::string(head) + "  <value name=\"a\" value=\"1\">\n", not_xml);
    require(first_code_is(not_xml, tl::DiagnosticCode::form_xml_invalid), "refused: XML that is not well formed is E821");

    Read wrong_namespace{};
    read("<fixture xmlns=\"urn:gui-forms:form:1\"/>\n", wrong_namespace);
    require(first_is(wrong_namespace, tl::DiagnosticCode::form_xml_invalid, 1U), "refused: another namespace is E821");

    Read wrong_root{};
    read("<form xmlns=\"urn:gui-forms:fixture:1\"/>\n", wrong_root);
    require(first_is(wrong_root, tl::DiagnosticCode::form_root_invalid, 1U), "refused: another root is E801");

    Read unknown_element{};
    read(std::string(head) + "  <grid name=\"a\"/>\n</fixture>\n", unknown_element);
    require(first_is(unknown_element, tl::DiagnosticCode::form_type_unknown, 3U), "refused: an unknown element is E802");

    Read unknown_attribute{};
    read(std::string(head) + "  <value name=\"a\" value=\"1\" colour=\"red\"/>\n</fixture>\n", unknown_attribute);
    require(first_is(unknown_attribute, tl::DiagnosticCode::form_property_unknown, 3U),
            "refused: an unknown attribute is E804");

    Read repeated{};
    read(std::string(head) + "  <value name=\"a\" value=\"1\"/>\n  <records name=\"a\" columns=\"x\"/>\n</fixture>\n",
         repeated);
    require(first_is(repeated, tl::DiagnosticCode::form_identity_repeated, 4U), "refused: a repeated name is E803");

    Read bad_name{};
    read(std::string(head) + "  <value name=\"not a name\" value=\"1\"/>\n</fixture>\n", bad_name);
    require(first_is(bad_name, tl::DiagnosticCode::form_value_invalid, 3U), "refused: a name that is no identifier is E807");

    Read bad_kind{};
    read(std::string(head) + "  <records name=\"r\" columns=\"x:colour\"/>\n</fixture>\n", bad_kind);
    require(first_is(bad_kind, tl::DiagnosticCode::form_value_invalid, 3U), "refused: an unknown column kind is E807");

    Read bad_cell{};
    read(std::string(head) + "  <records name=\"r\" columns=\"n:number\">\n    <row values=\"many\"/>\n  </records>\n</fixture>\n",
         bad_cell);
    require(first_is(bad_cell, tl::DiagnosticCode::form_value_invalid, 4U), "refused: a cell its column cannot take is E807");

    Read too_many{};
    read(std::string(head) + "  <records name=\"r\" columns=\"a\">\n    <row values=\"x y\"/>\n  </records>\n</fixture>\n",
         too_many);
    require(first_is(too_many, tl::DiagnosticCode::form_value_invalid, 4U), "refused: more values than columns is E807");

    Read bad_escape{};
    read(std::string(head) + "  <records name=\"r\" columns=\"a\">\n    <row values=\"'\\q'\"/>\n  </records>\n</fixture>\n",
         bad_escape);
    require(first_is(bad_escape, tl::DiagnosticCode::form_escape_invalid, 4U), "refused: a bad escape is E815");

    Read text_inside{};
    read(std::string(head) + "  <value name=\"a\" value=\"1\">text</value>\n</fixture>\n", text_inside);
    require(first_code_is(text_inside, tl::DiagnosticCode::form_xml_invalid), "refused: text inside an element is E821");

    require(repeated.fixture.values.empty(), "refused: the fixture is left unchanged");
}

} // namespace

int main() {
    test_empty();
    test_values_and_records();
    test_tree();
    test_refusals();
    if (failures != 0) {
        std::cerr << failures << " form fixture checks failed\n";
        return 1;
    }
    std::cout << "form fixture: values, records, trees and refusals passed\n";
    return 0;
}
