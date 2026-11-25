// The MCP launch probe (cp37): a GUI.Forms program under Application::run
// with one button, which tools/mcp_launch_check.py starts with --mcp. The
// check says initialize and tools/list over the program's standard input and
// output and then closes the input, the portable shutdown, so a build whose
// MCP has no transport (an STXlib package without stxlib::mcp) fails a test
// instead of shipping a --mcp that only reports it has none.
//
// Its window's ready callback registers one program tool, probe_ready: the
// check requires it in the first tools/list, with the built-in set, however
// soon after launch the client asks (serving starts after the first
// window's ready callback; 1.0.3's start race, macos-arm64).
//
//   gui_forms_mcp_launch_probe --mcp

#include "gui_forms/application.hpp"
#include "gui_forms/automation/tools/model_tools.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/control.hpp"

#include <memory>

using namespace gui_forms;

namespace {

void probe_ready_tool(void*, const automation::ModelToolCall&, automation::ModelToolReply& reply) {
    static const char text[] = "ready";
    static_cast<void>(reply.text(text, sizeof(text) - 1U));
}

// The window's ready callback: the program's tool is registered here, after
// the automation joined the window and before serving starts.
struct RegisterOnReady final {
    void operator()(Window&, ApplicationWindowHandle) const {
        automation::ModelToolSpec spec{};
        spec.name = "probe_ready";
        spec.description = "Registered in the probe window's ready callback.";
        static_cast<void>(automation::register_model_tool(spec, &probe_ready_tool, nullptr));
    }
};

} // namespace

int main() {
    ApplicationCreateResult created = Application::create(ApplicationConfig{});
    if (!created) return 1;
    Application& application = *created.application;
    ApplicationWindowOptions options{};
    options.title = "GUI.Forms MCP launch probe";
    options.initial_size = {320.0, 200.0};
    options.ready = RegisterOnReady{};
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("probe.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("probe.button"), "Probe");
    (*button).set_requested_bounds(Rect{20.0, 20.0, 120.0, 32.0});
    (*root).add_child(button);
    const ApplicationResult result =
        application.run(std::make_unique<Window>(root, Size{320.0, 200.0}), std::move(options));
    const int code = result.accepted() ? 0 : 1;
    return code;
}
