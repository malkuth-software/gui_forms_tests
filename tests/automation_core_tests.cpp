// The automation core driven headless (notes:planning/studies/
// MCP_AUTOMATION_STUDY.md, G1): a bare Window, a pump that runs its
// dispatcher, frame schedule and damage on a test clock, and the core's
// tools called as a model would call them. Covers the reads (windows,
// snapshot), acts and their diffs, the protected field, the user step, the
// destructive question (one per launch), the dialog rule, batch, the
// public model-tool registry and the event-propagation trace.

#include "gui_forms/automation/marks/automation_marks.hpp"
#include "gui_forms/automation/tools/model_tools.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/controls/modal_layer.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/window.hpp"
#include "src/controls/automation/control/automation_control.hpp"
#include "src/controls/automation/core/automation_core.hpp"
#include "src/controls/automation/presenter/stock_presenter.hpp"
#include "src/controls/automation/tools/automation_tools.hpp"
#include "support/named_callbacks.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace gui_forms;
using namespace gui_forms::automation;
namespace callbacks = gui_forms::test_support;

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "automation core test failed: " << message << '\n';
    std::exit(1);
}

bool contains(const std::string& text, std::string_view part) {
    const bool found = text.find(part) != std::string::npos;
    return found;
}

// The reply's text when it succeeded; empty otherwise.
std::string success_text(const ToolReply& reply) {
    const ToolSuccess* const success = reply.success_value();
    if (success == nullptr) return std::string{};
    return (*success).text;
}

// Whether the reply succeeded; takes a temporary reply, whose success_value()
// is deleted (it would point into the temporary).
bool succeeded(const ToolReply& reply) {
    const bool success = reply.success_value() != nullptr;
    return success;
}

bool failed_with(const ToolReply& reply, ToolError error) {
    const ToolFailure* const failure = reply.failure_value();
    const bool matched = failure != nullptr && (*failure).error == error;
    return matched;
}

// Records what the core showed the user; answers the destructive question
// with `allow` when the pump next runs.
class TestPresenter final : public AutomationPresenter {
public:
    bool allow{};
    std::size_t questions{};
    std::size_t reads{};
    std::size_t acts{};
    std::size_t user_steps{};
    std::size_t finished{};
    std::size_t pointer_moves{};

    void window_attached(Window&) override {}
    void window_detached(Window&) noexcept override {}
    void client_changed(const ClientIdentity&) override {}
    void activity(Window&, AutomationActivity activity) override {
        if (activity == AutomationActivity::reading) {
            ++reads;
            return;
        }
        ++acts;
    }
    void calls_finished() noexcept override { ++finished; }
    void pointer_moved(Window&, Point, bool) noexcept override { ++pointer_moves; }
    void control_changed(ControlState, const ClientIdentity&) override {}
    void ask_consent(const ClientIdentity&) override { ++questions; }
    void user_step_needed(Window&, const SemanticNode&, std::string_view) override { ++user_steps; }
    void disconnected() noexcept override {}
    [[nodiscard]] bool presenter_window(const SurfaceRoot&) const noexcept override { return false; }
};

// The headless window's clock and pump: time moves only when a call waits.
class WindowPump final : public AutomationPump {
public:
    WindowPump(Window& window, AutomationControl& control, TestPresenter& presenter)
        : window_(window), control_(control), presenter_(presenter) {}

    [[nodiscard]] FrameTime now() const noexcept override { return now_; }

    void advance(FrameTime until) override {
        if (until > now_) now_ = until;
        static_cast<void>(window_.poll_frame_schedule(now_));
        static_cast<void>(window_.drain_posted_work());
        window_.perform_layout();
        static_cast<void>(window_.take_damage());
        if (presenter_.questions > answered_) {
            answered_ = presenter_.questions;
            control_.answer_consent(presenter_.allow);
        }
    }

private:
    Window& window_;
    AutomationControl& control_;
    TestPresenter& presenter_;
    FrameTime now_{FrameClock::now()};
    std::size_t answered_{};
};

// The fixture: a button, a check box, a destructive button and a password
// field in one bare window, joined to a core.
struct Fixture final {
    std::shared_ptr<Panel> root{make_control<Panel>(StableId("auto.root"))};
    std::shared_ptr<Button> press{make_control<Button>(StableId("auto.press"), "Apply")};
    std::shared_ptr<CheckBox> check{make_control<CheckBox>(StableId("auto.check"), "Wrap lines")};
    std::shared_ptr<Button> erase{make_control<Button>(StableId("auto.erase"), "Delete all")};
    std::shared_ptr<TextBox> secret{make_control<TextBox>(StableId("auto.secret"), "hunter2")};
    std::unique_ptr<Window> window{};
    TestPresenter presenter{};
    AutomationControl control{ControlSettings{}, &presenter};
    AutomationCore core{control};
    std::uint64_t presses{};
    std::uint64_t erases{};
    SubscriptionToken pressed{};
    SubscriptionToken erased{};
    std::unique_ptr<WindowPump> pump{};

    explicit Fixture(bool allow) {
        presenter.allow = allow;
        (*press).set_requested_bounds(Rect{10.0, 10.0, 100.0, 28.0});
        (*check).set_requested_bounds(Rect{10.0, 50.0, 160.0, 24.0});
        (*erase).set_requested_bounds(Rect{10.0, 90.0, 100.0, 28.0});
        (*secret).set_requested_bounds(Rect{10.0, 130.0, 200.0, 28.0});
        (*secret).use_system_password_character = true;
        (*root).add_child(press);
        (*root).add_child(check);
        (*root).add_child(erase);
        (*root).add_child(secret);
        window = std::make_unique<Window>(root, Size{320.0, 200.0});
        (*window).perform_layout();
        pressed = (*press).clicked().subscribe(callbacks::IncrementCounter<std::uint64_t, ButtonBase&>(presses));
        erased = (*erase).clicked().subscribe(callbacks::IncrementCounter<std::uint64_t, ButtonBase&>(erases));
        require(core.attach_window(*window, "auto.window"), "the window joins the automation");
        pump = std::make_unique<WindowPump>(*window, control, presenter);
    }

    ToolReply call(std::string_view tool, std::string arguments) {
        ToolReply reply = core.run(tool, std::move(arguments), *pump);
        return reply;
    }
};

void reads_name_the_window_and_hide_the_password() {
    Fixture fixture(false);
    const std::string windows = success_text(fixture.call("windows", "{}"));
    require(contains(windows, "auto.window"), "windows names the joined window");
    const std::string outline = success_text(fixture.call("snapshot", "{}"));
    require(contains(outline, "auto.press") && contains(outline, "auto.check") && contains(outline, "auto.secret"),
            "the snapshot names every control by stable ID");
    require(!contains(outline, "hunter2"), "the snapshot never shows a password field's text");
    require(failed_with(fixture.call("text", "{\"target\":\"auto.secret\"}"), ToolError::protected_content),
            "text refuses a password field");
    require(fixture.presenter.reads != 0U, "a read glints");
    require(failed_with(fixture.call("no_such_tool", "{}"), ToolError::unknown_tool), "an unknown tool is refused");
    require(failed_with(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.nothing\"}"),
                        ToolError::not_found),
            "an unknown target is not found");
}

void acts_press_and_reply_with_the_diff() {
    Fixture fixture(false);
    const ToolReply pressed = fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}");
    require(pressed.success_value() != nullptr && fixture.presses == 1U, "act press clicks the button once");
    require(fixture.presenter.acts != 0U && fixture.presenter.finished != 0U,
            "an act lights the border and the border rests when the call ends");
    const std::string checked = success_text(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.check\"}"));
    require(contains(checked, "changed"), "pressing the check box replies with what changed");
}

void a_user_step_needs_the_user() {
    Fixture fixture(false);
    require(require_user_step("auto.press", "sign in"), "the user step is marked");
    require(failed_with(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}"), ToolError::needs_user),
            "a pending user step refuses the model");
    require(fixture.presenter.user_steps == 1U && fixture.presses == 0U, "the step is shown and nothing is pressed");
    complete_user_step("auto.press");
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presses == 1U,
            "once the user did the step the model carries on");
}

void destructive_acts_ask_once_per_launch() {
    require(set_action_destructive("auto.erase", SemanticAction::press, true), "the erase button is destructive");
    {
        Fixture declined(false);
        const std::string arguments = "{\"action\":\"press\",\"target\":\"auto.erase\"}";
        require(failed_with(declined.call("act", arguments), ToolError::declined), "Don't Allow refuses the act");
        require(failed_with(declined.call("act", arguments), ToolError::declined), "the answer holds for the launch");
        require(declined.presenter.questions == 1U && declined.erases == 0U, "the question is asked once");
        require(succeeded(declined.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
                "non-destructive acts still work");
    }
    {
        Fixture allowed(true);
        const std::string arguments = "{\"action\":\"press\",\"target\":\"auto.erase\"}";
        require(succeeded(allowed.call("act", arguments)) && allowed.erases == 1U,
                "Allow performs the act");
        require(succeeded(allowed.call("act", arguments)) && allowed.erases == 2U &&
                    allowed.presenter.questions == 1U,
                "the answer holds without asking again");
    }
    require(set_action_destructive("auto.erase", SemanticAction::press, false), "the mark is cleared");
}

void dialogs_refuse_the_model_until_instrumented() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.modal.root"));
    const std::shared_ptr<Button> behind = make_control<Button>(StableId("auto.modal.behind"), "Behind");
    (*behind).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*root).add_child(behind);
    Window window(root, Size{500.0, 400.0});
    window.perform_layout();
    const std::shared_ptr<ModalLayer> layer = make_control<ModalLayer>(StableId("auto.modal"));
    const std::shared_ptr<Panel> content = make_control<Panel>(StableId("auto.modal.content"));
    (*content).set_requested_bounds(Rect{0.0, 0.0, 200.0, 120.0});
    const std::shared_ptr<Button> inside = make_control<Button>(StableId("auto.modal.inside"), "OK");
    (*inside).set_requested_bounds(Rect{60.0, 80.0, 80.0, 28.0});
    (*content).add_child(inside);
    require((*layer).show(behind, content), "the modal layer shows");
    window.perform_layout();

    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.modal.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    require(failed_with(core.run("act", "{\"action\":\"press\",\"target\":\"auto.modal.inside\"}", pump),
                        ToolError::dialog_open),
            "a dialog the program did not instrument refuses acts");
    require(succeeded(core.run("snapshot", "{}", pump)), "the dialog can still be read");
    set_model_drivable(window, true);
    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.modal.inside\"}", pump)),
            "an instrumented dialog admits the model");
    set_model_drivable(window, false);
}

void batch_runs_its_steps_in_one_call() {
    Fixture fixture(false);
    const ToolReply reply = fixture.call(
        "batch", "{\"steps\":[{\"tool\":\"snapshot\"},{\"tool\":\"act\",\"action\":\"press\",\"target\":\"auto.press\"},"
                 "{\"tool\":\"wait\",\"until\":\"idle\"}]}");
    require(reply.success_value() != nullptr && fixture.presses == 1U, "the batch runs every step");
    const ToolReply stopped = fixture.call(
        "batch", "{\"steps\":[{\"tool\":\"act\",\"action\":\"press\",\"target\":\"auto.nothing\"},"
                 "{\"tool\":\"act\",\"action\":\"press\",\"target\":\"auto.press\"}]}");
    require(stopped.failure_value() != nullptr && fixture.presses == 1U, "the batch stops at the first failure");
}

void echo_tool(void*, const ModelToolCall& call, ModelToolReply& reply) {
    static_cast<void>(reply.text("echoed ", 7U));
    static_cast<void>(reply.text(call.arguments_json, call.arguments_bytes));
}

// Counts its calls; acts as a program's destructive tool would.
void wipe_tool(void* context, const ModelToolCall& call, ModelToolReply& reply) {
    ++*static_cast<std::uint64_t*>(context);
    if (call.window == nullptr) reply.fail(ModelToolError::internal, "a UI tool has its window", nullptr);
}

void note_section(void*, const ModelInspectRequest& request, ModelInspectText& text) {
    static_cast<void>(text.append("note:", 5U));
    if (request.window != nullptr) static_cast<void>(text.append(" window", 7U));
}

void the_public_registry_adds_tools() {
    Fixture fixture(false);
    const std::uint64_t before = fixture.core.tool_generation();
    ModelToolSpec spec{};
    spec.name = "test.echo";
    spec.title = "Echo";
    spec.description = "Replies echoed and its arguments.";
    spec.input_schema = "{\"type\":\"object\",\"properties\":{}}";
    require(register_model_tool(spec, &echo_tool, nullptr) == ModelToolRegistration::added, "a tool is added");
    require(register_model_tool(spec, &echo_tool, nullptr) == ModelToolRegistration::duplicate,
            "a name is added once");
    require(fixture.core.tool_generation() != before, "the tool list changes");
    std::vector<ToolSpec> specs{};
    fixture.core.fill_tool_specs(specs);
    bool listed = false;
    for (const ToolSpec& listed_spec : specs) {
        if (listed_spec.name == "test.echo") listed = true;
    }
    require(listed, "the added tool is listed");
    require(success_text(fixture.call("test.echo", "{}")) == "echoed {}", "the added tool answers");

    ModelToolSpec reserved{};
    reserved.name = "snapshot";
    reserved.description = "Shadows a built-in tool.";
    require(register_model_tool(reserved, &echo_tool, nullptr) == ModelToolRegistration::duplicate,
            "a built-in tool's name is refused");
    ModelToolSpec off_thread{};
    off_thread.name = "test.off_thread";
    off_thread.description = "Acts away from the UI thread.";
    off_thread.access = ModelToolAccess::acting;
    off_thread.thread = ModelToolThread::any;
    require(register_model_tool(off_thread, &echo_tool, nullptr) == ModelToolRegistration::invalid_spec,
            "an acting tool runs on the UI thread");

    std::uint64_t wipes = 0U;
    ModelToolSpec wipe{};
    wipe.name = "test.wipe";
    wipe.description = "Wipes the document.";
    wipe.access = ModelToolAccess::acting;
    wipe.destructive = true;
    require(register_model_tool(wipe, &wipe_tool, &wipes) == ModelToolRegistration::added, "a destructive tool");
    require(failed_with(fixture.call("test.wipe", "{}"), ToolError::declined) && wipes == 0U &&
                fixture.presenter.questions == 1U,
            "a program's destructive tool asks the launch's question, and Don't Allow refuses it");

    require(register_model_inspect_section("test.note", "A note.", &note_section, nullptr) ==
                ModelToolRegistration::added,
            "an inspect section is added");
    const std::string noted = success_text(fixture.call("inspect", "{\"what\":[\"test.note\"]}"));
    require(contains(noted, "note: window"), "the section answers inspect");

    require(unregister_model_tool("test.echo") && unregister_model_tool("test.wipe") &&
                unregister_model_inspect_section("test.note"),
            "the tools and the section are withdrawn");
    require(failed_with(fixture.call("test.echo", "{}"), ToolError::unknown_tool), "a withdrawn tool is unknown");
}

void a_program_tool_meets_the_dialog_rule() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.tool.root"));
    const std::shared_ptr<Button> behind = make_control<Button>(StableId("auto.tool.behind"), "Behind");
    (*behind).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*root).add_child(behind);
    Window window(root, Size{500.0, 400.0});
    window.perform_layout();
    const std::shared_ptr<ModalLayer> layer = make_control<ModalLayer>(StableId("auto.tool.modal"));
    const std::shared_ptr<Panel> content = make_control<Panel>(StableId("auto.tool.content"));
    (*content).set_requested_bounds(Rect{0.0, 0.0, 200.0, 120.0});
    require((*layer).show(behind, content), "the modal layer shows");
    window.perform_layout();
    TestPresenter presenter{};
    presenter.allow = true;
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.tool.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    std::uint64_t wipes = 0U;
    ModelToolSpec wipe{};
    wipe.name = "test.dialog_wipe";
    wipe.description = "Acts on the window.";
    wipe.access = ModelToolAccess::acting;
    require(register_model_tool(wipe, &wipe_tool, &wipes) == ModelToolRegistration::added, "an acting tool");
    require(failed_with(core.run("test.dialog_wipe", "{}", pump), ToolError::dialog_open) && wipes == 0U,
            "a program's acting tool is refused while an uninstrumented dialog is open");
    require(unregister_model_tool("test.dialog_wipe"), "the tool is withdrawn");
}

void inspect_traces_the_routed_events() {
    Fixture fixture(false);
    const ToolReply clicked = fixture.call("pointer", "{\"action\":\"click\",\"target\":\"auto.press\"}");
    require(clicked.success_value() != nullptr && fixture.presses == 1U, "a pointer click clicks the button");
    require(fixture.presenter.pointer_moves != 0U, "the presenter hears where the pointer is (the tag line on hover)");
    const std::string trace =
        success_text(fixture.call("inspect", "{\"target\":\"auto.press\",\"what\":[\"events\"]}"));
    require(contains(trace, "pointer") && contains(trace, "auto.press"),
            "the events section shows the click routed to the button");
}

void the_stock_presenter_shows_the_tag_line_on_hover() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.tag.root"));
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    StockPresenter presenter{};
    presenter.window_attached(window);
    require(window.band_snapshot().declared, "the presenter declares the band the glint runs on");
    ClientIdentity client{};
    client.client_name = "Claude Code";
    client.model_name = "Claude Opus 5.5";
    presenter.client_changed(client);
    // Entering the top border, moving along it, leaving it and entering
    // again: each entry asks for the tag line, the moves along it do not.
    presenter.pointer_moved(window, Point{40.0, 1.0}, false);
    presenter.pointer_moved(window, Point{80.0, 1.0}, false);
    presenter.pointer_moved(window, Point{80.0, 100.0}, false);
    presenter.pointer_moved(window, Point{80.0, 1.0}, false);
    presenter.pointer_moved(window, Point{80.0, 1.0}, true);
    presenter.disconnected();
    presenter.window_detached(window);
    require(client_label(client) == "Claude Opus 5.5 via Claude Code", "the tag line names the model and client");
}

} // namespace

int main() {
    reads_name_the_window_and_hide_the_password();
    acts_press_and_reply_with_the_diff();
    a_user_step_needs_the_user();
    destructive_acts_ask_once_per_launch();
    dialogs_refuse_the_model_until_instrumented();
    batch_runs_its_steps_in_one_call();
    the_public_registry_adds_tools();
    a_program_tool_meets_the_dialog_rule();
    inspect_traces_the_routed_events();
    the_stock_presenter_shows_the_tag_line_on_hover();
    std::cout << "automation core tests passed\n";
    return 0;
}
