// MCP G6's registrations with the automation's tool registry: the inspect
// sections (summary, layout_reasons, resources) beside the core's own, and
// the forms.* tools with their marks; and a forms call on a window whose
// form is not registered is refused, naming the build toggle.

#include "src/controls/automation/model_extensions/model_form_tools.hpp"
#include "src/controls/automation/model_extensions/model_inspect_sections.hpp"
#include "src/controls/automation/tools/automation_tools.hpp"

#include "gui_forms/automation/tools/model_tools.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/user_control.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace automation = gui_forms::automation;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void test_inspect_sections() {
    automation::ToolRegistry tools{};
    require(automation::register_model_inspect_sections(tools) == 3U, "sections: three are added");
    require(automation::register_model_inspect_sections(tools) == 0U, "sections: a second call adds none");
    automation::InspectSectionEntry entry{};
    require(tools.find_inspect_section("summary", entry), "sections: summary");
    require(tools.find_inspect_section("layout_reasons", entry), "sections: layout_reasons");
    require(tools.find_inspect_section("resources", entry), "sections: resources");
    require(!tools.find_inspect_section("layout", entry), "sections: the core's layout is not taken");
    automation::InspectRequest request{};
    require(tools.find_inspect_section("layout_reasons", entry), "sections: layout_reasons again");
    const std::string text = automation::call_inspect_section(entry, request);
    require(text.find("name a target") != std::string::npos, "sections: layout_reasons without a target says so");
}

void test_forms_tools() {
    automation::ToolRegistry tools{};
    require(automation::register_model_form_tools(tools) == 4U, "forms tools: four are added");
    require(automation::register_model_form_tools(tools) == 0U, "forms tools: a second call adds none");
    const char* const names[] = {"forms_export", "forms_validate", "forms_apply", "forms_revert"};
    for (const char* const name : names) {
        automation::ToolEntry entry{};
        require(tools.find(name, entry), std::string("forms tools: ") + name);
        require(entry.spec.thread == automation::ToolThread::ui, std::string(name) + " runs on the UI thread");
    }
    automation::ToolEntry apply{};
    require(tools.find("forms_apply", apply), "forms tools: apply");
    require(apply.spec.destructive && apply.spec.settles && apply.spec.access == automation::ToolAccess::acting,
            "forms tools: apply is destructive, acting and settles");
    automation::ToolEntry exported{};
    require(tools.find("forms_export", exported), "forms tools: export");
    require(exported.spec.access == automation::ToolAccess::read_only && !exported.spec.settles,
            "forms tools: export reads only");

    // A window whose form no build registered.
    const std::shared_ptr<gui_forms::UserControl> root =
        gui_forms::make_control<gui_forms::UserControl>(gui_forms::StableId("unregistered.root"));
    gui_forms::Window window(root, gui_forms::Size{200.0, 100.0});
    const std::string arguments = "{}";
    automation::ModelToolCall call{};
    call.arguments_json = arguments.data();
    call.arguments_bytes = arguments.size();
    call.window = &window;
    const automation::ToolReply refused = automation::call_tool_handler(exported, call);
    const automation::ToolFailure* const failure = refused.failure_value();
    require(failure != nullptr && (*failure).error == automation::ToolError::unsupported_action &&
                (*failure).hint.find("GUI_FORMS_FORM_EXPORT") != std::string::npos,
            "forms tools: an unregistered form is refused, naming the toggle");
    call.window = nullptr;
    const automation::ToolReply gone = automation::call_tool_handler(exported, call);
    const automation::ToolFailure* const no_window = gone.failure_value();
    require(no_window != nullptr && (*no_window).error == automation::ToolError::window_gone,
            "forms tools: no window is window_gone");
}

} // namespace

int main() {
    test_inspect_sections();
    test_forms_tools();
    if (failures != 0) {
        std::cerr << failures << " model tool registration checks failed\n";
        return 1;
    }
    std::cout << "model tool registration: inspect sections and forms tools passed\n";
    return 0;
}
