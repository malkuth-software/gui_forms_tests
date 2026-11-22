// The Windows Forms converter (stage MCP G7, tools/winforms_converter):
// the C# lexer, the designer and .resx readers, the name mapping, a whole
// form converted and its document validated against the registry schema,
// the project tools on a project written to a temporary directory, and the
// structural diff.

#include "tools/form_compiler/form_catalog.hpp"
#include "tools/winforms_converter/conversion_diagnostics.hpp"
#include "tools/winforms_converter/converter_tools.hpp"
#include "tools/winforms_converter/csharp_classes.hpp"
#include "tools/winforms_converter/csharp_lexer.hpp"
#include "tools/winforms_converter/designer_reader.hpp"
#include "tools/winforms_converter/form_document_diff.hpp"
#include "tools/winforms_converter/resx_reader.hpp"
#include "tools/winforms_converter/winforms_conversion.hpp"
#include "tools/winforms_converter/winforms_mapping.hpp"
#include "tools/winforms_converter/winforms_project.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

namespace wf = gui_forms::winforms_converter;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void print_diagnostics(const wf::ConversionDiagnosticList& diagnostics) {
    for (const wf::ConversionDiagnostic& record : diagnostics.records()) {
        std::cerr << "  " << wf::conversion_diagnostic_line(record) << '\n';
    }
}

[[nodiscard]] bool has_code(const wf::ConversionDiagnosticList& diagnostics, wf::ConversionCode code) {
    for (const wf::ConversionDiagnostic& record : diagnostics.records()) {
        if (record.code == code) return true;
    }
    return false;
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part) {
    const bool found = text.find(part) != std::string_view::npos;
    return found;
}

constexpr std::string_view designer_text = R"cs(namespace Sample.App
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;

        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.okButton = new System.Windows.Forms.Button();
            this.nameLabel = new System.Windows.Forms.Label();
            this.nameBox = new System.Windows.Forms.TextBox();
            this.chart1 = new Vendor.Charts.Chart();
            this.customerSource = new System.Windows.Forms.BindingSource(this.components);
            this.SuspendLayout();
            //
            // okButton
            //
            this.okButton.Location = new System.Drawing.Point(12, 80);
            this.okButton.Name = "okButton";
            this.okButton.Size = new System.Drawing.Size(75, 23);
            this.okButton.TabIndex = 2;
            this.okButton.Text = "&OK";
            this.okButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(0)))), ((int)(((byte)(0)))));
            this.okButton.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point);
            this.okButton.Click += new System.EventHandler(this.okButton_Click);
            //
            // nameLabel
            //
            this.nameLabel.Location = new System.Drawing.Point(12, 15);
            this.nameLabel.Size = new System.Drawing.Size(60, 15);
            this.nameLabel.Text = "Name \"first\"";
            //
            // nameBox
            //
            this.nameBox.DataBindings.Add(new System.Windows.Forms.Binding("Text", this.customerSource, "Name", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged));
            this.nameBox.Location = new System.Drawing.Point(80, 12);
            this.nameBox.Size = new System.Drawing.Size(200, 23);
            //
            // chart1
            //
            this.chart1.Location = new System.Drawing.Point(12, 120);
            //
            // MainForm
            //
            this.AcceptButton = this.okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(300, 160);
            this.Controls.Add(this.chart1);
            this.Controls.Add(this.nameBox);
            this.Controls.Add(this.nameLabel);
            this.Controls.Add(this.okButton);
            this.Name = "MainForm";
            this.Text = "Customer";
            this.ResumeLayout(false);
            this.PerformLayout();
        }

        private System.Windows.Forms.Button okButton;
        private System.Windows.Forms.Label nameLabel;
        private System.Windows.Forms.TextBox nameBox;
        private Vendor.Charts.Chart chart1;
        private System.Windows.Forms.BindingSource customerSource;
    }
}
)cs";

constexpr std::string_view class_text = R"cs(using System;
using System.Windows.Forms;

namespace Sample.App
{
    public partial class MainForm : Form
    {
        public MainForm() { InitializeComponent(); }

        private void okButton_Click(object sender, EventArgs e)
        {
            Close();
        }
    }
}
)cs";

constexpr std::string_view resx_text = R"xml(<?xml version="1.0" encoding="utf-8"?>
<root>
  <resheader name="resmimetype"><value>text/microsoft-resx</value></resheader>
  <data name="okButton.Text" xml:space="preserve"><value>OK</value></data>
  <data name="okButton.Location" type="System.Drawing.Point, System.Drawing"><value>12, 80</value></data>
  <data name="$this.Icon" type="System.Drawing.Icon, System.Drawing" mimetype="application/x-microsoft.net.object.bytearray.base64"><value>AAAB</value></data>
</root>
)xml";

void test_lexer() {
    wf::ConversionDiagnosticList diagnostics{};
    wf::CSharpSource source{};
    source.file = "lex.cs";
    source.text = "x = @\"a\"\"b\" + \"\\u0041\\n\" + 'c' + 0x1F + 9F; // note\n#region r\ny();";
    require(wf::lex_csharp(source, diagnostics) == wf::CSharpLexOutcome::lexed, "lexer: admitted text is lexed");
    bool verbatim = false;
    bool escaped = false;
    for (const wf::CSharpToken& token : source.tokens) {
        if (token.kind == wf::CSharpTokenKind::string && token.value == "a\"b") verbatim = true;
        if (token.kind == wf::CSharpTokenKind::string && token.value == "A\n") escaped = true;
    }
    require(verbatim, "lexer: a verbatim string's doubled quote is one quote");
    require(escaped, "lexer: \\u and \\n escapes are decoded");
    wf::CSharpSource interpolated{};
    interpolated.file = "bad.cs";
    interpolated.text = "x = $\"{a}\";";
    wf::ConversionDiagnosticList refused{};
    require(wf::lex_csharp(interpolated, refused) == wf::CSharpLexOutcome::refused,
            "lexer: an interpolated string is refused");
    require(has_code(refused, wf::ConversionCode::csharp_lexical), "lexer: the refusal is WF101");
}

void test_mapping() {
    require(wf::stable_id_segment_of("textBox1") == "text-box1", "mapping: textBox1 is text-box1");
    require(wf::stable_id_segment_of("btnOK") == "btn-ok", "mapping: btnOK is btn-ok");
    require(wf::stable_id_segment_of("_name") == "name", "mapping: a leading underscore is dropped");
    require(wf::snake_case_of("HScrollBar") == "h_scroll_bar", "mapping: HScrollBar is h_scroll_bar");
    require(wf::member_identifier_of("bindingSource1") == "binding_source1", "mapping: a member identifier");
    require(wf::member_identifier_of("1st") == "source_1st", "mapping: a leading digit gets source_");
    const wf::WinFormsTypeRow* const button = wf::find_winforms_type("System.Windows.Forms.Button");
    require(button != nullptr && (*button).registry_id == "button", "mapping: Button is the registry's button");
    if (button != nullptr) {
        const wf::WinFormsMemberMatch text = wf::find_winforms_member(*button, "Text", wf::WinFormsMemberKind::property);
        require(text.member != nullptr, "mapping: Button's Text is found on a base");
    }
    const wf::WinFormsTypeRow* const flow = wf::find_winforms_type("FlowLayoutPanel");
    require(flow != nullptr && (*flow).registry_id == "flow_layout_panel_compat",
            "mapping: FlowLayoutPanel is the compatibility panel");
    require(wf::find_winforms_type("NoSuchControl") == nullptr, "mapping: an unknown type has no row");
}

[[nodiscard]] bool read_sample(wf::CSharpSource& designer_source, wf::DesignerDocument& designer,
                               wf::CSharpSource& class_source, wf::CSharpClassIndex& classes,
                               wf::ResxDocument& resx, wf::ConversionDiagnosticList& diagnostics) {
    designer_source.file = "MainForm.Designer.cs";
    designer_source.text = std::string(designer_text);
    class_source.file = "MainForm.cs";
    class_source.text = std::string(class_text);
    const bool read = wf::read_designer_file(designer_source, designer, diagnostics) == wf::DesignerReadOutcome::read;
    wf::read_csharp_classes(class_source, classes, diagnostics);
    const bool resources = wf::read_resx("MainForm.resx", resx_text, resx, diagnostics) == wf::ResxReadOutcome::read;
    return read && resources;
}

void test_readers() {
    wf::CSharpSource designer_source{};
    wf::DesignerDocument designer{};
    wf::CSharpSource class_source{};
    wf::CSharpClassIndex classes{};
    wf::ResxDocument resx{};
    wf::ConversionDiagnosticList diagnostics{};
    require(read_sample(designer_source, designer, class_source, classes, resx, diagnostics),
            "readers: the sample designer file and resources are read");
    require(designer.class_name == "MainForm" && designer.namespace_name == "Sample.App",
            "designer: the class and its namespace");
    require(designer.find_field("okButton") != nullptr, "designer: the fields are listed");
    require(designer.statements.size() > 20U, "designer: InitializeComponent's statements are read");
    require(classes.base_of("MainForm") == "Form", "classes: the form class's base");
    require(classes.declares_method("MainForm", "okButton_Click"), "classes: the handler is declared");
    const wf::ResxEntry* const text = resx.find("okButton.Text");
    require(text != nullptr && (*text).value == "OK", "resx: a string entry");
    const wf::ResxEntry* const icon = resx.find("$this.Icon");
    require(icon != nullptr && (*icon).binary(), "resx: a base64 entry is binary");
    if (diagnostics.error_count() != 0U) print_diagnostics(diagnostics);
}

void test_conversion() {
    wf::CSharpSource designer_source{};
    wf::DesignerDocument designer{};
    wf::CSharpSource class_source{};
    wf::CSharpClassIndex classes{};
    wf::ResxDocument resx{};
    wf::ConversionDiagnosticList diagnostics{};
    if (!read_sample(designer_source, designer, class_source, classes, resx, diagnostics)) {
        require(false, "conversion: the sample is read");
        return;
    }
    const gui_forms::form_language::FormSchema schema = gui_forms::form_compiler::registry_form_schema();
    wf::ConversionOptions options{};
    wf::ConvertedForm form{};
    const wf::ConversionOutcome outcome =
        wf::convert_winforms_form(designer, nullptr, classes, schema, options, form, diagnostics);
    require(outcome == wf::ConversionOutcome::converted, "conversion: the form is converted");
    const std::string& document = form.form_document;
    require(form.form_id == "main-form", "conversion: the form ID is the class respelled");
    require(contains(document, "<form xmlns=\"urn:gui-forms:form:1\" id=\"main-form\""), "conversion: the form root");
    require(contains(document, "id=\"main-form.root\""), "conversion: the root panel");
    require(contains(document, "<button id=\"main-form.ok-button\""), "conversion: the button and its stable ID");
    require(contains(document, "bounds=\"12 80 75 23\""), "conversion: Location and Size are one bounds");
    require(contains(document, "text=\"Name &quot;first&quot;\""), "conversion: text is escaped");
    require(contains(document, "client-size=\"300 160\""), "conversion: ClientSize on the root");
    require(contains(document, "accept-button=\"main-form.ok-button\""), "conversion: the accept button's ID");
    require(contains(document, "<bind property=\"text\" to=\"customer_source\" field=\"Name\" update=\"on-change\"/>"),
            "conversion: DataBindings.Add is a bind element");
    require(contains(document, "classes=\"winforms-unmapped\""), "conversion: an unknown type is an unmapped panel");
    // Painter order: the last added (okButton) first, the first (chart1) last.
    require(document.find("main-form.ok-button") < document.find("main-form.chart1"),
            "conversion: the children are in painter order");
    require(has_code(diagnostics, wf::ConversionCode::type_unmapped), "conversion: WF201 for the chart");
    require(has_code(diagnostics, wf::ConversionCode::auto_scale_dropped), "conversion: WF301 once");
    require(has_code(diagnostics, wf::ConversionCode::presentation_to_theme), "conversion: WF206 for the colour");
    require(contains(form.theme_sheet, "#main-form\\.ok-button {"), "theme: the stable-ID selector");
    require(contains(form.theme_sheet, "background-color: #FF0000;"), "theme: FromArgb as a hex colour");
    require(contains(form.theme_sheet, "font-size: 12px;") && contains(form.theme_sheet, "font-weight: 700;"),
            "theme: 9 points are 12 pixels, bold is 700");
    require(contains(form.csharp_binding, "partial class MainForm"), "binding: a partial class of the form class");
    require(contains(form.csharp_binding, "gui_forms_okButton = FormTree.find(root, \"main-form.ok-button\");"),
            "binding: each object found by its stable ID");
    require(contains(form.csharp_binding, "okButton_Click(this, System.EventArgs.Empty);"),
            "binding: the handler called with its arguments");
    require(form.sources.size() == 1U && form.sources[0].name == "customer_source",
            "conversion: the binding source is a model source");
    gui_forms::theme_language::ThemeDiagnosticList validation{};
    const bool valid = wf::validate_form_document(form.form_file, document, schema, validation);
    require(valid, "conversion: the converted document validates against the registry schema");
    if (!valid) {
        std::cerr << document;
        for (const gui_forms::theme_language::ThemeDiagnostic& record : validation.records()) {
            std::cerr << "  " << wf::theme_diagnostic_line(record) << '\n';
        }
    }
    // With the resources: ApplyResources is not called, but GetString is
    // not either; the resx changes nothing here, which is checked by the
    // conversion being the same.
    wf::ConvertedForm again{};
    wf::ConversionDiagnosticList second{};
    static_cast<void>(wf::convert_winforms_form(designer, &resx, classes, schema, options, again, second));
    require(again.form_document == document, "conversion: deterministic, and unused resources change nothing");
}

void test_project_tools() {
    std::error_code error{};
    const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "gui_forms_winforms_converter_tests";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "obj", error);
    wf::ConversionDiagnosticList diagnostics{};
    const std::string base = root.generic_string();
    require(wf::write_text_file(base + "/MainForm.Designer.cs", designer_text, diagnostics), "project: designer written");
    require(wf::write_text_file(base + "/MainForm.cs", class_text, diagnostics), "project: class written");
    require(wf::write_text_file(base + "/MainForm.resx", resx_text, diagnostics), "project: resx written");
    require(wf::write_text_file(base + "/obj/Stale.Designer.cs", "garbage", diagnostics), "project: obj written");
    wf::WinFormsProject project{};
    require(wf::find_winforms_project(base, project, diagnostics) == wf::ProjectOutcome::found, "project: found");
    require(project.forms.size() == 1U, "project: obj is skipped; one form");
    if (project.forms.size() == 1U) {
        require(project.forms[0].name == "MainForm", "project: the form's name");
        require(!project.forms[0].resx_file.empty(), "project: the resx beside it");
    }
    const wf::ConverterToolReply listed = wf::run_list_forms(wf::ListFormsRequest{base});
    require(listed.succeeded && contains(listed.json, "\"name\":\"MainForm\""), "list_forms: the form is listed");
    const gui_forms::form_language::FormSchema schema = gui_forms::form_compiler::registry_form_schema();
    wf::ConvertRequest request{};
    request.project = base;
    request.form = "MainForm";
    request.output_directory = base + "/out";
    const wf::ConverterToolReply converted = wf::run_convert(request, schema);
    require(converted.succeeded, "convert: succeeded");
    require(contains(converted.json, "\"valid\":true"), "convert: the written document validates");
    require(std::filesystem::is_regular_file(root / "out" / "main-form.form", error), "convert: the form is written");
    require(std::filesystem::is_regular_file(root / "out" / "MainForm.GuiForms.g.cs", error),
            "convert: the binding is written");
    wf::ConvertRequest missing = request;
    missing.form = "NoSuchForm";
    const wf::ConverterToolReply refused = wf::run_convert(missing, schema);
    require(!refused.succeeded && contains(refused.text, "WF004"), "convert: an unknown form is WF004");
    const wf::ConverterToolReply invalid =
        wf::run_validate(wf::ValidateRequest{"bad.form", "<form xmlns=\"urn:gui-forms:form:1\" id=\"x\"><nope id=\"x.a\"/></form>"},
                         schema);
    require(contains(invalid.json, "\"valid\":false"), "validate: an unknown element is not valid");
    std::filesystem::remove_all(root, error);
    if (failures != 0) print_diagnostics(diagnostics);
}

void test_diff() {
    const std::string before = "<form id=\"f\"><panel id=\"f.root\"><button id=\"f.a\" text=\"A\"/>"
                               "<label id=\"f.b\" text=\"B\"><bind property=\"text\" to=\"m\"/></label></panel></form>";
    const std::string after = "<form id=\"f\"><panel id=\"f.root\"><label id=\"f.b\" text=\"B2\"/>"
                              "<check-box id=\"f.c\"/><button id=\"f.a\" text=\"A\"/></panel></form>";
    wf::FormDiff diff{};
    require(wf::diff_form_documents(before, after, diff) == wf::FormDiffOutcome::compared, "diff: compared");
    bool added = false;
    bool moved = false;
    bool changed = false;
    bool unbound = false;
    for (const wf::FormDiffEntry& entry : diff.entries) {
        if (entry.kind == wf::FormDiffKind::object_added && entry.id == "f.c") added = true;
        if (entry.kind == wf::FormDiffKind::object_moved && entry.id == "f.a") moved = true;
        if (entry.kind == wf::FormDiffKind::attribute_changed && entry.id == "f.b" && entry.after == "B2") changed = true;
        if (entry.kind == wf::FormDiffKind::binding_removed && entry.id == "f.b") unbound = true;
    }
    require(added && moved && changed && unbound, "diff: added, moved, changed and unbound are reported");
    wf::FormDiff same{};
    require(wf::diff_form_documents(before, before, same) == wf::FormDiffOutcome::compared && same.entries.empty(),
            "diff: a document against itself has no entry");
    wf::FormDiff broken{};
    require(wf::diff_form_documents("<form", after, broken) == wf::FormDiffOutcome::refused && !broken.problem.empty(),
            "diff: a text that is not XML is refused");
}

} // namespace

int main() {
    test_lexer();
    test_mapping();
    test_readers();
    test_conversion();
    test_project_tools();
    test_diff();
    if (failures != 0) {
        std::cerr << failures << " winforms converter checks failed\n";
        return 1;
    }
    std::cout << "winforms converter: lexer, readers, mapping, conversion, project tools and diff passed\n";
    return 0;
}
