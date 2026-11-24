// The MCP launch probe (cp37): a GUI.Forms program under Application::run
// with one button, which tools/mcp_launch_check.py starts with --mcp. The
// check says initialize and tools/list over the program's standard input and
// output and then closes the input, the portable shutdown, so a build whose
// MCP has no transport (an STXlib package without stxlib::mcp) fails a test
// instead of shipping a --mcp that only reports it has none.
//
//   gui_forms_mcp_launch_probe --mcp

#include "gui_forms/application.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/control.hpp"

#include <memory>

using namespace gui_forms;

int main() {
    ApplicationCreateResult created = Application::create(ApplicationConfig{});
    if (!created) return 1;
    Application& application = *created.application;
    ApplicationWindowOptions options{};
    options.title = "GUI.Forms MCP launch probe";
    options.initial_size = {320.0, 200.0};
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("probe.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("probe.button"), "Probe");
    (*button).set_requested_bounds(Rect{20.0, 20.0, 120.0, 32.0});
    (*root).add_child(button);
    const ApplicationResult result =
        application.run(std::make_unique<Window>(root, Size{320.0, 200.0}), std::move(options));
    const int code = result.accepted() ? 0 : 1;
    return code;
}
