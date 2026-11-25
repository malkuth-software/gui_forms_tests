// The automation core driven headless (notes:planning/studies/
// MCP_AUTOMATION_STUDY.md, G1): a bare Window, a pump that runs its
// dispatcher, frame schedule and damage on a test clock, and the core's
// tools called as a model would call them. Covers the reads (windows,
// snapshot), acts and their diffs, the protected field, the user step, the
// destructive question (one per launch), the dialog rule, batch, the
// public model-tool registry and the event-propagation trace; and MCP round
// 1: strict arguments, find, per-node actions, rows a list does not show,
// dialogs a model opened (closed as Cancel, never submitted), reset, and
// the appearance-review refusal; and MCP round 2 with the owner's control
// model (2026-11-24): the take question, its countdown, a refusal, the
// grace period, release_control, the take-back dialog a press offers, the
// AI-led launch, AI mode's veil and X cursor, popups without a sign,
// structured batch failures, expectations and waits that name nothing,
// typed pointer arguments, pending state, the program's note, tool names
// without '.', and modal windows read as dialogs; and MCP round 4: still
// stripes that ask for no frames, the band's strips never painted as the
// whole window, idle while the sign moves, program tools that return at
// once, activation that offers no take-back, one question per session,
// tooltips held under control, and a dropped connection that leaves the
// program the user's to close.

#include "gui_forms/automation/marks/automation_marks.hpp"
#include "gui_forms/automation/tools/model_tools.hpp"
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/capture/offscreen_raster/offscreen_raster.hpp"
#include "gui_forms/commands/command/command.hpp"
#include "gui_forms/controls/card.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/controls/modal_layer.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/window.hpp"
#include "src/controls/automation/control/automation_control.hpp"
#include "src/controls/automation/core/automation_core.hpp"
#include "src/controls/automation/presenter/stock_presenter.hpp"
#include "src/controls/automation/tools/automation_tools.hpp"
#include "src/core/surface_root/access/surface_request_access.hpp"
#include "src/core/host/session/session_directory.hpp"
#if defined(GUI_FORMS_TEST_AUTOMATION_RUN)
#include "src/runtime/automation/automation_run.hpp"
#include "src/host/headless/application/headless_application.hpp"
#include "src/host/headless/services/headless_host_services.hpp"
#include "src/host/headless/session/headless_host.hpp"
#endif
#include "support/named_callbacks.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <string>
#include <span>
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
    // The take question: answered with take_allow when the pump next runs,
    // or left to its countdown when take_answers is false.
    bool take_allow{true};
    bool take_answers{true};
    std::size_t takes{};
    std::size_t take_back_offers{};
    ControlState state{ControlState::user_control};
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
    void control_changed(ControlState changed, const ClientIdentity&) override { state = changed; }
    void ask_take(const ClientIdentity&, std::chrono::milliseconds) override { ++takes; }
    void offer_take_back(const ClientIdentity&) override { ++take_back_offers; }
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
        if (presenter_.take_answers && presenter_.takes > takes_answered_) {
            takes_answered_ = presenter_.takes;
            control_.answer_take(presenter_.take_allow);
        }
    }

private:
    Window& window_;
    AutomationControl& control_;
    TestPresenter& presenter_;
    FrameTime now_{FrameClock::now()};
    std::size_t answered_{};
    std::size_t takes_answered_{};
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

// Inside a native alert over the main window: the model's act there is
// refused, and the refusal names the dialog it saw.
class NativeAlertAct final {
public:
    NativeAlertAct(AutomationCore& core, AutomationPump& pump, std::string& seen)
        : core_(&core), pump_(&pump), seen_(&seen) {}

    HostDialogResult operator()(const HostDialogRequest& request, host::HeadlessHostServices&) const {
        const ToolReply reply = (*core_).run(
            "act", "{\"window\":\"auto.alert.window\",\"action\":\"press\",\"target\":\"auto.alert.button\"}",
            *pump_);
        const ToolFailure* const failure = reply.failure_value();
        if (failure != nullptr && (*failure).error == ToolError::dialog_open) *seen_ = (*failure).message;
        const HostDialogResult result{
            {}, request.request_id, HostMessageDialogResult{HostDialogOutcome::cancelled, HostDialogChoice::cancel}};
        return result;
    }

private:
    AutomationCore* core_;
    AutomationPump* pump_;
    std::string* seen_;
};

// A native alert blocks the main window while a tooltip's window stays
// free: the dialog is the native one, never the tooltip (Plan Paint's
// recovery alert was reported as [main/tool-tip.1], 2026-11-24).
void a_native_alert_is_named_native_not_the_tooltip() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.alert.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("auto.alert.button"), "Behind");
    (*button).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*root).add_child(button);
    Window window(root, Size{400.0, 300.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    const std::shared_ptr<Panel> tip_root = make_control<Panel>(StableId("tool-tip"));
    Window tip(tip_root, Size{120.0, 24.0});
    host::HeadlessHost tip_host(tip);

    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.alert.window"), "the main window joins");
    require(core.attach_window(tip, "auto.alert.window/tool-tip.1", true), "the tooltip's window joins as passive");
    require(core.passive_popup_window(tip) && !core.passive_popup_window(window), "only the tooltip is passive");
    WindowPump pump(window, control, presenter);
    std::string seen{};
    window_host.headless_services().set_dialog_handler(NativeAlertAct(core, pump, seen));
    HostMessageDialogRequest message{};
    message.title = "Recover unfinished artwork";
    message.message = "Recover Untitled?";
    message.buttons = HostMessageButtons::ok_cancel;
    message.icon = HostMessageIcon::information;
    message.default_choice = HostDialogChoice::ok;
    const HostDialogResult shown = window_host.services().show_dialog(
        HostDialogRequest{1, "auto.alert.window", std::move(message)});
    require(shown.status.accepted(), "the native alert ran");
    if (seen.find("native") == std::string::npos || seen.find("tool-tip") != std::string::npos) {
        std::cerr << "refusal: " << seen << '\n';
    }
    require(seen.find("native") != std::string::npos && seen.find("tool-tip") == std::string::npos,
            "the refusal names the native dialog, not the tooltip's window");
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
    spec.name = "test_echo";
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
        if (listed_spec.name == "test_echo") listed = true;
    }
    require(listed, "the added tool is listed");
    require(success_text(fixture.call("test_echo", "{}")) == "echoed {}", "the added tool answers");

    ModelToolSpec reserved{};
    reserved.name = "snapshot";
    reserved.description = "Shadows a built-in tool.";
    require(register_model_tool(reserved, &echo_tool, nullptr) == ModelToolRegistration::duplicate,
            "a built-in tool's name is refused");
    ModelToolSpec off_thread{};
    off_thread.name = "test_off_thread";
    off_thread.description = "Acts away from the UI thread.";
    off_thread.access = ModelToolAccess::acting;
    off_thread.thread = ModelToolThread::any;
    require(register_model_tool(off_thread, &echo_tool, nullptr) == ModelToolRegistration::invalid_spec,
            "an acting tool runs on the UI thread");

    std::uint64_t wipes = 0U;
    ModelToolSpec wipe{};
    wipe.name = "test_wipe";
    wipe.description = "Wipes the document.";
    wipe.access = ModelToolAccess::acting;
    wipe.destructive = true;
    require(register_model_tool(wipe, &wipe_tool, &wipes) == ModelToolRegistration::added, "a destructive tool");
    require(failed_with(fixture.call("test_wipe", "{}"), ToolError::declined) && wipes == 0U &&
                fixture.presenter.questions == 1U,
            "a program's destructive tool asks the launch's question, and Don't Allow refuses it");

    require(register_model_inspect_section("test.note", "A note.", &note_section, nullptr) ==
                ModelToolRegistration::added,
            "an inspect section is added");
    const std::string noted = success_text(fixture.call("inspect", "{\"what\":[\"test.note\"]}"));
    require(contains(noted, "note: window"), "the section answers inspect");

    require(unregister_model_tool("test_echo") && unregister_model_tool("test_wipe") &&
                unregister_model_inspect_section("test.note"),
            "the tools and the section are withdrawn");
    require(failed_with(fixture.call("test_echo", "{}"), ToolError::unknown_tool), "a withdrawn tool is unknown");
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
    wipe.name = "test_dialog_wipe";
    wipe.description = "Acts on the window.";
    wipe.access = ModelToolAccess::acting;
    require(register_model_tool(wipe, &wipe_tool, &wipes) == ModelToolRegistration::added, "an acting tool");
    require(failed_with(core.run("test_dialog_wipe", "{}", pump), ToolError::dialog_open) && wipes == 0U,
            "a program's acting tool is refused while an uninstrumented dialog is open");
    require(unregister_model_tool("test_dialog_wipe"), "the tool is withdrawn");
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

// A call's failure detail, empty when it succeeded.
std::string failure_text(const ToolReply& reply) {
    const ToolFailure* const failure = reply.failure_value();
    if (failure == nullptr) return std::string{};
    return (*failure).message + " / " + (*failure).hint;
}

// Every tool schema stays inside the MCP server's JSON Schema subset
// (stxlib/mcp.hpp): a keyword outside it leaves the tool unbound, unseen by
// the model (MCP round 1: batch's first published schema used anyOf).
void tool_schemas_stay_in_the_served_subset() {
    Fixture fixture(false);
    std::vector<ToolSpec> specs{};
    fixture.core.fill_tool_specs(specs);
    const std::string_view refused[] = {"\"anyOf\"", "\"oneOf\"", "\"allOf\"", "\"$ref\"", "\"not\"",
                                        "\"pattern\"", "\"if\"", "\"prefixItems\"", "\"uniqueItems\""};
    bool batch = false;
    bool find = false;
    bool reset = false;
    for (const ToolSpec& spec : specs) {
        for (const std::string_view keyword : refused) {
            if (contains(spec.input_schema, keyword)) {
                std::cerr << spec.name << " uses " << keyword << '\n';
                require(false, "a tool schema uses a keyword the MCP server refuses");
            }
        }
        batch = batch || spec.name == "batch";
        find = find || spec.name == "find";
        reset = reset || spec.name == "reset";
    }
    require(batch && find && reset, "batch, find and reset are served");
}

void arguments_are_strict_and_find_finds() {
    Fixture fixture(false);
    const ToolReply unknown = fixture.call("snapshot", "{\"limt\":5}");
    require(failed_with(unknown, ToolError::invalid_arguments), "an unknown argument is refused");
    require(contains(failure_text(unknown), "takes no limt"), "the refusal names the unknown argument");
    require(failed_with(fixture.call("batch", "{\"steps\":[{\"expect\":\"node\",\"target\":\"auto.press\","
                                              "\"colour\":\"red\"}]}"),
                        ToolError::invalid_arguments),
            "an expect step with an unknown member is refused");

    const std::string found = success_text(fixture.call("find", "{\"role\":\"button\",\"name\":\"apply\"}"));
    require(contains(found, "auto.press") && !contains(found, "auto.erase"),
            "find matches the role and a part of the name, in any case");
    require(failed_with(fixture.call("find", "{\"role\":\"spaceship\"}"), ToolError::invalid_arguments),
            "find refuses a role that does not exist");
    require(failed_with(fixture.call("find", "{}"), ToolError::invalid_arguments), "find names something");
    require(contains(success_text(fixture.call("find", "{\"name\":\"no such name\"}")), "no node matches"),
            "find says when nothing matches");

    const std::string outline = success_text(fixture.call("snapshot", "{}"));
    require(contains(outline, "actions="), "the snapshot lists each node's actions");
    require(contains(success_text(fixture.call("text", "{\"target\":\"auto.press\"}")), "Apply"),
            "text reads a button's whole name");
    require(succeeded(fixture.call("act", "{\"action\":\"scroll_into_view\",\"target\":\"auto.press\"}")),
            "scroll_into_view brings a control into view");
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.check\",\"terse\":true}")),
            "a terse act runs");
}

void rows_a_list_does_not_show_are_reached_by_id() {
    const std::shared_ptr<ListBox> list = make_control<ListBox>(StableId("auto.rows"));
    std::vector<std::string> names{};
    for (int index = 0; index < 40; ++index) names.push_back("Row " + std::to_string(index));
    (*list).items = names;
    (*list).set_requested_bounds(Rect{0.0, 0.0, 180.0, 86.0});
    Window window(list, Size{180.0, 86.0});
    window.perform_layout();
    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.rows.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    const std::string outline = success_text(core.run("snapshot", "{}", pump));
    require(contains(outline, "more rows not shown"), "the snapshot says the list holds rows it does not show");
    const ToolReply pressed = core.run("act", "{\"action\":\"select\",\"target\":\"auto.rows.item.31\"}", pump);
    require(succeeded(pressed), "a row the list does not show is acted on by its ID");
    require((*list).selected_index() == stxlib::Optional<std::size_t>::of(31U), "the row is selected");
    require(failed_with(core.run("act", "{\"action\":\"select\",\"target\":\"auto.rows.item.99\"}", pump),
                        ToolError::unsupported_action) ||
                failed_with(core.run("act", "{\"action\":\"select\",\"target\":\"auto.rows.item.99\"}", pump),
                            ToolError::not_found),
            "a row the list does not have is refused");
}

// Opens the modal layer over its owner, as a dialog command would.
class ShowLayer final {
public:
    ShowLayer(ModalLayer& layer, Control::Ptr owner, Control::Ptr content) noexcept
        : layer_(layer), owner_(std::move(owner)), content_(std::move(content)) {}

    void operator()(ButtonBase&) const { static_cast<void>(layer_.show(owner_, content_)); }

private:
    ModalLayer& layer_;
    Control::Ptr owner_;
    Control::Ptr content_;
};

// The dialog ID a reply names after "a dialog opened: [".
std::string opened_dialog(const std::string& text) {
    const std::string_view lead = "a dialog opened: [";
    const std::size_t start = text.find(lead);
    if (start == std::string::npos) return std::string{};
    const std::size_t first = start + lead.size();
    const std::size_t end = text.find(']', first);
    if (end == std::string::npos) return std::string{};
    return text.substr(first, end - first);
}

void a_model_closes_the_dialog_it_opened_but_never_submits_it() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.opener.root"));
    const std::shared_ptr<Button> opener = make_control<Button>(StableId("auto.opener"), "Settings...");
    (*opener).set_requested_bounds(Rect{10.0, 10.0, 100.0, 28.0});
    (*root).add_child(opener);
    Window window(root, Size{500.0, 400.0});
    window.perform_layout();
    const std::shared_ptr<ModalLayer> layer = make_control<ModalLayer>(StableId("auto.opener.modal"));
    const std::shared_ptr<Panel> content = make_control<Panel>(StableId("auto.opener.content"));
    (*content).set_requested_bounds(Rect{0.0, 0.0, 200.0, 120.0});
    const std::shared_ptr<Button> submit = make_control<Button>(StableId("auto.opener.ok"), "OK");
    (*submit).set_requested_bounds(Rect{60.0, 80.0, 80.0, 28.0});
    (*content).add_child(submit);
    SubscriptionToken opened = (*opener).clicked().subscribe(ShowLayer(*layer, opener, content));
    require(set_opens_dialog("auto.opener", true, "auto.settings"), "the opener is marked");
    require(opens_dialog("auto.opener"), "the mark reads back");

    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.opener.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    const std::string outline = success_text(core.run("snapshot", "{}", pump));
    require(contains(outline, "needs_user") && contains(outline, "press=opens_dialog instead=auto.settings"),
            "the snapshot marks the opener and names its dialog-free tool");

    const std::string first = success_text(core.run("act", "{\"action\":\"press\",\"target\":\"auto.opener\"}", pump));
    const std::string dialog = opened_dialog(first);
    require(!dialog.empty() && contains(first, "never submits"), "the reply says the dialog needs the user");
    require(contains(first, "auto.settings"), "the reply names the tool that does this without a dialog");
    require(failed_with(core.run("act", "{\"action\":\"press\",\"target\":\"auto.opener.ok\"}", pump),
                        ToolError::dialog_open),
            "the model never submits the dialog it opened");
    const std::string close = "{\"action\":\"close\",\"target\":\"" + dialog + "\"}";
    const ToolReply closed = core.run("act", close, pump);
    if (!succeeded(closed)) std::cerr << "close [" << dialog << "]: " << failure_text(closed) << '\n';
    require(succeeded(closed), "the model closes the dialog it opened");
    require(!(*layer).is_shown(), "the dialog closed as Cancel");

    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.opener\"}", pump)) &&
                (*layer).is_shown(),
            "the dialog opens again");
    const std::string reset = success_text(core.run("reset", "{}", pump));
    require(contains(reset, "closed the dialog") && !(*layer).is_shown(), "reset closes the model's dialog");
    require(contains(success_text(core.run("reset", "{}", pump)), "nothing needed resetting"),
            "a second reset finds nothing to do");
    require(set_opens_dialog("auto.opener", false, nullptr), "the mark is cleared");
}

void the_sign_shows_ai_mode_and_review_mode_shows_none() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.wash.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("auto.wash.button"), "Apply");
    (*button).set_requested_bounds(Rect{10.0, 10.0, 100.0, 28.0});
    (*root).add_child(button);
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    require(window.content_wash() == 1.0, "no wash without a model");
    bool refused = false;
    try {
        window.set_content_wash(std::nan(""));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a wash that is not a number is refused");
    window.set_content_wash(4.0);
    require(window.content_wash() == 1.0, "the wash is clamped to 0..1");

    ClientIdentity client{};
    client.client_name = "Claude Code";
    client.model_name = "Claude Opus 5.5";
    {
        StockPresenter presenter{};
        presenter.window_attached(window);
        presenter.client_changed(client);
        require(window.content_wash() == 1.0 && !window.model_veil(),
                "a connected model that does not hold control neither washes nor stripes the window");
        require(window.title_note() != nullptr && contains(window.title_note(), "is connected"),
                "a connected model is named in the title");
        presenter.control_changed(ControlState::model_control, client);
        require(window.model_veil() && window.content_wash() == 1.0,
                "AI mode stripes the control surfaces and draws no greyscale wash");
        require(contains(window.title_note(), "is in control"), "AI mode says the model is in control");
        CursorImagesPtr images{};
        CursorKind fallback = CursorKind::arrow;
        require(window.model_cursor(images, fallback) && images && fallback == CursorKind::forbidden,
                "AI mode shows the X cursor, with a host fallback");
        presenter.control_changed(ControlState::user_control, client);
        require(!window.model_veil() && !window.model_cursor(images, fallback),
                "control back with the user ends AI mode");
        presenter.control_changed(ControlState::model_control, client);
        presenter.disconnected();
        require(!window.model_veil() && (window.title_note() == nullptr || window.title_note()[0] == '\0'),
                "every sign ends on disconnect");
        presenter.window_detached(window);
        window.undeclare_band();
    }
    {
        StockPresenter presenter{};
        presenter.set_unsigned(true);
        presenter.window_attached(window);
        presenter.client_changed(client);
        presenter.control_changed(ControlState::model_control, client);
        require(!window.band_snapshot().declared && !window.model_veil(),
                "appearance review shows no band and no veil");
        presenter.disconnected();
        presenter.window_detached(window);
    }
#if defined(GUI_FORMS_TEST_AUTOMATION_RUN)
    AutomationLaunch launch{};
    launch.mcp = true;
    require(automation_launch_refusal(launch, false) == nullptr, "a plain --mcp launch is not refused");
    launch.appearance_review = true;
    require(automation_launch_refusal(launch, false) != nullptr, "a release build refuses appearance review");
    require(automation_launch_refusal(launch, true) == nullptr, "a test build takes appearance review");
#endif
}


// The owner's control model (2026-11-24): a default launch's first act asks
// the user, waits on the answer, and once allowed the model holds control.
void a_first_act_asks_to_take_control() {
    Fixture fixture(false);
    require(success_text(fixture.call("snapshot", "{}")).size() != 0U && fixture.presenter.takes == 0U,
            "a read needs no control and asks nothing");
    const std::string first = success_text(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}"));
    require(fixture.presenter.takes == 1U && fixture.presses == 1U, "the first act asks once and then runs");
    require(fixture.control.state() == ControlState::model_control &&
                fixture.presenter.state == ControlState::model_control,
            "the model holds control and the presenter shows AI mode");
    require(contains(first, "You now hold control") && contains(first, "auto.check"),
            "the first allowed act's reply carries the window's snapshot");
    const std::string second = success_text(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}"));
    require(fixture.presenter.takes == 1U && fixture.presses == 2U && !contains(second, "You now hold control"),
            "later acts run at once without asking");
    require(contains(success_text(fixture.call("windows", "{}")), "control: you hold it"),
            "windows says the model holds control");
}

void the_take_countdown_allows_when_it_ends() {
    Fixture fixture(false);
    fixture.presenter.take_answers = false;
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presses == 1U && fixture.presenter.takes == 1U,
            "an unanswered take question allows when its countdown ends");
    require(fixture.control.state() == ControlState::model_control, "the countdown's end gives the model control");
}

void a_refused_take_blocks_the_act() {
    Fixture fixture(false);
    fixture.presenter.take_allow = false;
    const ToolReply refused = fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}");
    require(failed_with(refused, ToolError::declined) && contains(failure_text(refused), "declined to let you take"),
            "a refused take refuses the act and says the user declined");
    require(fixture.presses == 0U && fixture.control.state() == ControlState::user_control,
            "nothing ran and the user keeps control");
    require(failed_with(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}"), ToolError::declined) &&
                fixture.presenter.takes == 2U,
            "the next act asks the user again");
}

void control_returns_after_the_grace_period() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    fixture.control.tick((*fixture.pump).now() + std::chrono::seconds(29));
    require(fixture.control.state() == ControlState::model_control, "29 s without a call keeps control");
    fixture.control.tick((*fixture.pump).now() + std::chrono::seconds(31));
    require(fixture.control.state() == ControlState::user_control &&
                fixture.presenter.state == ControlState::user_control,
            "30 s without a call returns control and ends AI mode");
    require(contains(success_text(fixture.call("windows", "{}")), "note: control returned to the user after 30 s"),
            "the model's next reply says control returned");
    // The user allowed this session once (round 4: one question per
    // session), so the next act takes control again without asking.
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presenter.takes == 1U && fixture.control.state() == ControlState::model_control,
            "the next act takes control again without a second question");
}

void release_control_gives_control_back() {
    Fixture fixture(false);
    require(contains(success_text(fixture.call("release_control", "{}")), "did not hold control"),
            "releasing without control says so");
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    require(contains(success_text(fixture.call("release_control", "{}")), "control returned to the user"),
            "release_control gives control back");
    require(fixture.control.state() == ControlState::user_control &&
                fixture.presenter.state == ControlState::user_control,
            "AI mode ends on release");
}

// A user's press while the model holds control goes to the take-back
// dialog, never to the program; taking back interrupts the model.
void a_press_offers_take_back_and_taking_back_interrupts() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.back.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("auto.back.button"), "Apply");
    (*button).set_requested_bounds(Rect{10.0, 10.0, 100.0, 28.0});
    (*root).add_child(button);
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    require(window_host.dispatch(HostAttachEvent{{300.0, 200.0}, 1.0}, 500U).accepted(), "the host attaches");
    std::uint64_t presses = 0U;
    SubscriptionToken pressed =
        (*button).clicked().subscribe(callbacks::IncrementCounter<std::uint64_t, ButtonBase&>(presses));
    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.back.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.back.button\"}", pump)) &&
                presses == 1U,
            "the model takes control and presses");
    PointerEvent down{};
    down.action = PointerAction::down;
    down.button = PointerButton::primary;
    down.position = Point{40.0, 20.0};
    down.click_count = 1U;
    PointerEvent up = down;
    up.action = PointerAction::up;
    const std::uint64_t held_before = window_host.session().snapshot().user_input_held;
    static_cast<void>(window_host.dispatch(down, 1000U));
    static_cast<void>(window_host.dispatch(up, 2000U));
    static_cast<void>(window.drain_posted_work());
    require(presses == 1U && window_host.session().snapshot().user_input_held >= held_before + 2U,
            "the user's click never reaches the program while the model holds control");
    require(presenter.take_back_offers == 1U, "the press offers the take-back dialog once");
    control.user_took_back();
    require(control.state() == ControlState::user_control && presenter.state == ControlState::user_control,
            "taking back ends the model's control and AI mode");
    require(contains(success_text(core.run("windows", "{}", pump)), "the user took back control"),
            "the model's next reply says the user took control back");
    static_cast<void>(window_host.dispatch(down, 3000U));
    static_cast<void>(window_host.dispatch(up, 4000U));
    require(presses == 2U, "the user's clicks reach the program again");
}

void an_ai_led_launch_holds_control_without_asking() {
    Fixture fixture(false);
    ControlSettings settings{};
    settings.ai_led = true;
    TestPresenter presenter{};
    AutomationControl control(settings, &presenter);
    AutomationCore core(control);
    require(core.attach_window(*fixture.window, "auto.led.window"), "the window joins the AI-led automation");
    WindowPump pump(*fixture.window, control, presenter);
    control.begin_ai_led(pump.now());
    require(control.state() == ControlState::model_control, "an AI-led launch starts with the model in control");
    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.press\"}", pump)) &&
                presenter.takes == 0U && fixture.presses == 1U,
            "an AI-led act runs with no countdown");
    control.tick(pump.now() + std::chrono::minutes(10));
    require(control.state() == ControlState::model_control, "an AI-led launch never returns control after idle");
    control.user_took_back();
    require(control.state() == ControlState::user_control, "the user can always take back an AI-led launch");
#if defined(GUI_FORMS_TEST_AUTOMATION_RUN)
    AutomationLaunch shared{};
    shared.shared = true;
    require(shared.requested() && automation_launch_refusal(shared, false) == nullptr,
            "--mcp-shared is a launch of its own");
#endif
}

// A tooltip's window gets no band and no tag line: the tag line on a
// tooltip window opened another tooltip window, 65 deep (round 2).
// The modal turns of take_back_answers_over_model_modals: the owner's
// round-3 finding, the take-back question dead while the model's Resize
// dialog ran modally.
struct TakeBackTurnState final {
    host::HeadlessApplication* application{};
    AutomationControl* control{};
    // How deep the model's modal windows go before the user takes back.
    std::size_t depth{1U};
    bool blocked{true};
    bool taken{};
    bool dialog_left{};
    std::size_t depth_seen{};
};

struct TakeBackTurn final {
    TakeBackTurnState* state{};
    bool operator()(HostWindowId window) const;
};

ApplicationWindow modal_entry(const char* id, const char* owner) {
    ApplicationWindow entry{};
    entry.stable_id = id;
    entry.owner_id = owner;
    entry.model = std::make_unique<Window>(make_control<Panel>(StableId(std::string(id) + ".root")),
                                           Size{240.0, 160.0});
    entry.options.title = id;
    entry.options.initial_size = Size{240.0, 160.0};
    entry.options.minimum_size = Size{100.0, 80.0};
    return entry;
}

bool TakeBackTurn::operator()(HostWindowId window) const {
    TakeBackTurnState& run = *state;
    host::HeadlessApplication& application = *run.application;
    const std::size_t depth = application.windowing().modal_depth();
    if (depth < run.depth) {
        // The model's dialog opens one of its own (a nested modal).
        static_cast<void>(application.handle("resize").windows().show_modal(modal_entry("resize.inner", "resize")));
        application.windowing().set_modal_turn(*this);
        application.simulate_user_close(window);
        return true;
    }
    if (run.depth_seen == 0U) {
        run.depth_seen = depth;
        const HostWindowId question = application.id("automation.take-back");
        run.blocked = !question.valid() || application.windowing().input_blocked(question);
        Window* const model = question.valid() ? application.model(question) : nullptr;
        const std::shared_ptr<ButtonBase> accept =
            model == nullptr ? nullptr
                             : std::dynamic_pointer_cast<ButtonBase>((*model).find("automation.take-back.accept"));
        if (accept && !run.blocked) static_cast<void>((*accept).perform_click());
        run.taken = (*run.control).state() == ControlState::user_control;
        // Taking back leaves the model's dialog for the user.
        run.dialog_left = application.windowing().modal_depth() == depth && application.model(window) != nullptr;
    }
    // The user closes the dialog.
    application.simulate_user_close(window);
    return true;
}

void take_back_answers_over_model_modals() {
    for (std::size_t depth = 1U; depth <= 2U; ++depth) {
        std::vector<ApplicationWindow> windows{};
        windows.push_back(modal_entry("main", ""));
        host::HeadlessApplication application(std::move(windows));
        StockPresenter presenter{};
        ControlSettings settings{};
        settings.ai_led = true;
        AutomationControl control(settings, &presenter);
        presenter.bind_control(&control);
        presenter.set_windowing(application.handle("main"), "main");
        control.begin_ai_led(FrameClock::now());
        ClientIdentity client{};
        client.model_name = "Claude Opus 5.5";
        presenter.client_changed(client);
        // The user clicked: the question is up before the model's dialog.
        presenter.offer_take_back(client);
        require(application.id("automation.take-back").valid(), "the take-back question opens");
        TakeBackTurnState state{};
        state.application = &application;
        state.control = &control;
        state.depth = depth;
        application.windowing().set_modal_turn(TakeBackTurn{&state});
        const ApplicationModalResult shown =
            application.handle("main").windows().show_modal(modal_entry("resize", "main"));
        require(shown.status.accepted(), "the model's dialog runs modally");
        require(state.depth_seen == depth, "the user answers at the model's deepest modal");
        require(!state.blocked, "no modal of the model's blocks the take-back question");
        require(state.taken, "Take Back Control works while the model's modal dialog is up");
        require(state.dialog_left, "taking back leaves the model's dialog open for the user");
        presenter.disconnected();
        presenter.bind_control(nullptr);
    }
}

// A program's Close: with unsaved work it asks "Save changes?" through its
// window's host services, as Plan Paint's quit does (owner, round 3).
struct UnsavedWork final {
    bool dirty{true};
    bool closed{};
    HostDialogChoice answered{HostDialogChoice::none};
    std::uint64_t next_request{1U};
};

class AskToSave final {
public:
    AskToSave(Window& window, UnsavedWork& work) noexcept : window_(window), work_(work) {}
    void operator()(ButtonBase&) const {
        if (!work_.dirty) {
            work_.closed = true;
            return;
        }
        HostMessageDialogRequest question{};
        question.title = "Save changes?";
        question.message = "Save changes to Untitled?";
        question.buttons = HostMessageButtons::yes_no_cancel;
        question.default_choice = HostDialogChoice::cancel;
        HostDialogRequest request{};
        request.request_id = work_.next_request++;
        request.payload = question;
        const HostDialogResult result = (*window_.host_services()).show_dialog(request);
        const HostMessageDialogResult* const answer = std::get_if<HostMessageDialogResult>(&result.payload);
        work_.answered = answer == nullptr ? HostDialogChoice::none : (*answer).choice;
        if (work_.answered == HostDialogChoice::yes) work_.dirty = false;
        work_.closed = work_.answered == HostDialogChoice::yes || work_.answered == HostDialogChoice::no;
    }

private:
    Window& window_;
    UnsavedWork& work_;
};

class SaveWork final {
public:
    explicit SaveWork(UnsavedWork& work) noexcept : work_(work) {}
    void operator()(ButtonBase&) const { work_.dirty = false; }

private:
    UnsavedWork& work_;
};

// Owner, round 3: a question the program raises during the model's close
// is the model's. It is never shown; with no answer the program stays as
// it was and nothing is left waiting; on_unsaved answers it; save, then
// close, needs no question; the user's own close still shows it.
void a_models_close_answers_the_programs_question() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.unsaved.root"));
    const std::shared_ptr<Button> close = make_control<Button>(StableId("auto.unsaved.close"), "Close");
    (*close).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*root).add_child(close);
    const std::shared_ptr<Button> save = make_control<Button>(StableId("auto.unsaved.save"), "Save");
    (*save).set_requested_bounds(Rect{100.0, 10.0, 80.0, 28.0});
    (*root).add_child(save);
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    require(window_host.dispatch(HostAttachEvent{{300.0, 200.0}, 1.0}, 500U).accepted(), "the host attaches");
    UnsavedWork work{};
    SubscriptionToken asking = (*close).clicked().subscribe(AskToSave(window, work));
    SubscriptionToken saving = (*save).clicked().subscribe(SaveWork(work));
    TestPresenter presenter{};
    ControlSettings settings{};
    settings.ai_led = true;
    AutomationControl control(settings, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.unsaved.window"), "the window joins");
    WindowPump pump(window, control, presenter);
    control.begin_ai_led(pump.now());
    const std::string press_close = "{\"action\":\"press\",\"target\":\"auto.unsaved.close\"";

    const std::string unanswered = success_text(core.run("act", press_close + "}", pump));
    require(contains(unanswered, "question: the program asked \"Save changes?: Save changes to Untitled?\" (Yes, No, "
                                 "Cancel), answered Cancel; not shown to the user") &&
                contains(unanswered, "stays as it was") && !work.closed && work.dirty,
            "an unanswered question cancels the close and comes back in the reply");
    require(window_host.headless_services().dialog_trace().empty() && !gui_forms::detail::model_questions_open(),
            "nothing was shown and nothing is left waiting");
    require(succeeded(core.run("snapshot", "{}", pump)), "other tools work at once");

    const std::string discarded =
        success_text(core.run("act", press_close + ",\"on_unsaved\":\"discard\"}", pump));
    require(work.closed && work.answered == HostDialogChoice::no && contains(discarded, "answered No"),
            "on_unsaved discard answers No");
    work = UnsavedWork{};
    const std::string saved = success_text(core.run("act", press_close + ",\"on_unsaved\":\"save\"}", pump));
    require(work.closed && !work.dirty && contains(saved, "answered Yes"), "on_unsaved save answers Yes");
    require(failed_with(core.run("act", press_close + ",\"on_unsaved\":\"maybe\"}", pump),
                        ToolError::invalid_arguments),
            "on_unsaved takes save or discard");

    work = UnsavedWork{};
    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.unsaved.save\"}", pump)),
            "the model saves first");
    const std::string after_save = success_text(core.run("act", press_close + "}", pump));
    require(work.closed && !contains(after_save, "question:"), "save, then close, needs no question");

    // The user's own close shows the question.
    work = UnsavedWork{};
    HostDialogResult users{};
    users.request_id = 1U;
    HostMessageDialogResult cancelled{};
    cancelled.outcome = HostDialogOutcome::cancelled;
    users.payload = cancelled;
    window_host.headless_services().queue_dialog_result(users);
    static_cast<void>((*close).perform_click());
    require(contains(window_host.headless_services().dialog_trace(), "dialog="),
            "a close the user starts shows the program's question");
}

// A program window whose closing handler asks "Save changes?" through the
// window's host services while its work is unsaved, as Plan Paint's does:
// Yes saves, No discards, anything else keeps the window open.
struct ClosingQuestionState final {
    host::HeadlessApplication* application{};
    bool dirty{true};
    std::size_t asked{};
    bool shown{};
};

class AskOnClosing final {
public:
    explicit AskOnClosing(ClosingQuestionState& state) noexcept : state_(&state) {}
    void operator()(HostCloseRequest& request) const {
        ClosingQuestionState& run = *state_;
        if (!run.dirty) return;
        host::HeadlessHost* const window_host = (*run.application).host((*run.application).id("main"));
        HostMessageDialogRequest question{};
        question.title = "Save changes?";
        question.message = "Save changes to Untitled?";
        question.buttons = HostMessageButtons::yes_no_cancel;
        question.default_choice = HostDialogChoice::cancel;
        HostDialogRequest asked{};
        asked.request_id = ++run.asked;
        asked.payload = question;
        const HostDialogResult result = (*window_host).headless_services().show_dialog(asked);
        const HostMessageDialogResult* const answer = std::get_if<HostMessageDialogResult>(&result.payload);
        run.shown = !(*window_host).headless_services().dialog_trace().empty();
        const HostDialogChoice choice = answer == nullptr ? HostDialogChoice::none : (*answer).choice;
        if (choice == HostDialogChoice::yes) run.dirty = false;
        request.cancel = choice != HostDialogChoice::yes && choice != HostDialogChoice::no;
    }

private:
    ClosingQuestionState* state_;
};

// One program window with unsaved work, served AI-led.
class DisconnectRig final {
public:
    DisconnectRig() {
        std::vector<ApplicationWindow> windows{};
        windows.push_back(modal_entry("main", ""));
        windows.back().options.closing = AskOnClosing(state);
        application = std::make_unique<host::HeadlessApplication>(std::move(windows));
        state.application = application.get();
        settings.ai_led = true;
        control = std::make_unique<AutomationControl>(settings, &presenter);
        core = std::make_unique<AutomationCore>(*control);
        Window& window = *(*application).model((*application).id("main"));
        require((*core).attach_window(window, "main", false, (*application).handle("main")), "the window joins");
        pump = std::make_unique<WindowPump>(window, *control, presenter);
        (*control).begin_ai_led((*pump).now());
    }
    [[nodiscard]] bool open() const { return (*application).id("main").valid(); }
    [[nodiscard]] ToolReply run(const char* arguments) { return (*core).run("disconnect", arguments, *pump); }
    void user_cancels() {
        HostDialogResult users{};
        users.request_id = state.asked + 1U;
        HostMessageDialogResult cancelled{};
        cancelled.outcome = HostDialogOutcome::cancelled;
        cancelled.choice = HostDialogChoice::cancel;
        users.payload = cancelled;
        (*(*application).host((*application).id("main"))).headless_services().queue_dialog_result(users);
    }

    ClosingQuestionState state{};
    std::unique_ptr<host::HeadlessApplication> application{};
    TestPresenter presenter{};
    ControlSettings settings{};
    std::unique_ptr<AutomationControl> control{};
    std::unique_ptr<AutomationCore> core{};
    std::unique_ptr<WindowPump> pump{};
};

// Owner, round 3: disconnect. Soft (the default) never closes over unsaved
// work and shows nothing: it balks and names the question; save, then soft
// again goes through. Hard ends the session: the program's question goes to
// the user, or discard_unsaved discards with no question. A transport drop
// with no disconnect is a hard disconnect with the default.
void disconnect_soft_balks_and_hard_ends() {
    {
        DisconnectRig rig{};
        const std::string balked = success_text(rig.run("{}"));
        require(contains(balked, "not disconnected: the program has unsaved work") &&
                    contains(balked, "\"Save changes?: Save changes to Untitled?\"") &&
                    contains(balked, "then disconnect again"),
                "a soft disconnect over unsaved work balks and names the question");
        require(rig.state.asked == 1U && !rig.state.shown && rig.open() && !gui_forms::detail::model_questions_open(),
                "nothing closed, nothing was shown, nothing is left waiting");
        require(!(*rig.core).session_ended() && succeeded((*rig.core).run("snapshot", "{}", *rig.pump)),
                "the session goes on");
        require(failed_with(rig.run("{\"discard_unsaved\":true}"), ToolError::invalid_arguments),
                "discard_unsaved goes with mode hard");
        rig.state.dirty = false;
        const std::string done = success_text(rig.run("{\"mode\":\"soft\"}"));
        require(contains(done, "ok: disconnected") && !rig.open() && (*rig.core).session_ended(),
                "save, then a soft disconnect, closes the program");
        require(failed_with((*rig.core).run("windows", "{}", *rig.pump), ToolError::needs_user),
                "a call after the disconnect is refused");
    }
    {
        DisconnectRig rig{};
        rig.user_cancels();
        const std::string hard = success_text(rig.run("{\"mode\":\"hard\"}"));
        require(contains(hard, "ok: disconnected (hard)") && rig.state.asked == 1U && rig.state.shown,
                "a hard disconnect shows the program's question to the user");
        require((*rig.control).state() == ControlState::user_control && rig.open() && (*rig.core).session_ended(),
                "the user holds the program and may keep it open");
    }
    {
        DisconnectRig rig{};
        const std::string discarded = success_text(rig.run("{\"mode\":\"hard\",\"discard_unsaved\":true}"));
        require(contains(discarded, "ok: disconnected (hard); unsaved changes discarded") && rig.state.asked == 1U &&
                    !rig.state.shown && !rig.open(),
                "hard with discard_unsaved discards and asks no one");
    }
    {
        DisconnectRig rig{};
        rig.user_cancels();
        // AutomationRun::close_all, as the transport ends.
        (*rig.core).hard_disconnect();
        require(rig.state.asked == 1U && rig.state.shown && rig.open(),
                "a transport drop shows the program's question to the user");
    }
}

void popup_windows_carry_no_sign() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.popup.root"));
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    const std::shared_ptr<Panel> tip_root = make_control<Panel>(StableId("tool-tip"));
    Window tip(tip_root, Size{120.0, 24.0});
    StockPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    presenter.bind_control(&control);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.popup.window"), "the main window joins");
    require(core.attach_window(tip, "auto.popup.window/tool-tip.1", true), "the tooltip's window joins as passive");
    ClientIdentity client{};
    client.model_name = "Claude Opus 5.5";
    presenter.client_changed(client);
    require(window.band_snapshot().declared && !tip.band_snapshot().declared,
            "the program's window carries the band, the tooltip's window none");
    require(tip.title_note() == nullptr || tip.title_note()[0] == '\0', "the tooltip's window carries no title note");
    TestPresenter reads{};
    WindowPump pump(window, control, reads);
    const std::string windows = success_text(core.run("windows", "{}", pump));
    require(contains(windows, "auto.popup.window") && !contains(windows, "tool-tip.1") &&
                !contains(windows, "popup window"),
            "windows lists the real windows and neither lists nor counts the popups");
    presenter.disconnected();
    presenter.bind_control(nullptr);
}

// Windows field test: select on an item that also presses moves the
// selection and opens nothing, so the reply says so.
void select_on_a_pressable_item_says_it_only_selects() {
    Fixture fixture(true);
    const std::shared_ptr<Card> card = make_control<Card>(StableId("auto.card"));
    (*card).interactive = true;
    (*card).accessible_name = "Recent file";
    (*card).set_requested_bounds(Rect{200.0, 10.0, 100.0, 40.0});
    (*fixture.root).add_child(card);
    (*fixture.window).perform_layout();
    const std::string selected =
        success_text(fixture.call("act", "{\"action\":\"select\",\"target\":\"auto.card\"}"));
    require(contains(selected, "select only selects; press opens it"),
            "select on an item that also presses says select only selects");
    const std::string pressed =
        success_text(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.card\"}"));
    require(!contains(pressed, "select only selects"), "press carries no select hint");
}

void round_two_replies_are_structured_and_strict() {
    Fixture fixture(false);
    const ToolReply stopped = fixture.call(
        "batch", "{\"steps\":[{\"tool\":\"snapshot\"},{\"tool\":\"act\",\"action\":\"press\",\"target\":"
                 "\"auto.nothing\"},{\"tool\":\"act\",\"action\":\"press\",\"target\":\"auto.press\"}]}");
    const ToolFailure* const failure = stopped.failure_value();
    require(failure != nullptr && contains((*failure).message, "step 2 act failed") &&
                contains((*failure).detail_json, "{\"steps\":[{\"step\":1,\"tool\":\"snapshot\",\"ok\":true") &&
                contains((*failure).detail_json, "\"step\":2,\"tool\":\"act\",\"ok\":false"),
            "a failed batch names the failing step and gives every step as structured JSON");
    require(failed_with(fixture.call("batch", "{\"steps\":[{\"expect\":\"visual\",\"target\":\"auto.press\"}]}"),
                        ToolError::invalid_arguments),
            "expect visual with nothing to check is refused, not passed");
    require(failed_with(fixture.call("batch", "{\"steps\":[{\"expect\":\"node\",\"target\":\"auto.press\"}]}"),
                        ToolError::invalid_arguments),
            "expect node with nothing to check is refused");
    // Windows field test M5: a value that does not match is a mismatch, not
    // a timeout.
    require(failed_with(fixture.call("batch", "{\"steps\":[{\"expect\":\"node\",\"target\":\"auto.press\","
                                              "\"exists\":false}]}"),
                        ToolError::expectation_failed),
            "an unmet expectation fails as expectation_failed");
    require(failed_with(fixture.call("wait", "{\"until\":\"user_step\",\"step\":\"auto.never\"}"),
                        ToolError::not_found),
            "waiting on a user step the program never asked for is refused");
    require(failed_with(fixture.call("pointer", "{\"action\":\"click\",\"target\":\"auto.press\",\"position\":"
                                                "{\"x\":1,\"y\":1}}"),
                        ToolError::invalid_arguments),
            "pointer takes a target or a position, not both");
    require(succeeded(fixture.call("pointer", "{\"action\":\"drag\",\"target\":\"auto.press\",\"offset\":{\"x\":5,"
                                              "\"y\":5},\"path\":[{\"x\":20,\"y\":5},{\"x\":40,\"y\":5}]}")),
            "a drag's path is read in the target's space");
    const std::string typed = success_text(fixture.call("keyboard", "{\"target\":\"auto.check\",\"keys\":\"Space\"}"));
    require(contains(typed, "ok: keyboard into") && contains(typed, "frames:"),
            "keyboard replies with what it did and changed, as act does");

    require(set_model_pending_state("shape", "a rectangle floats at 10,10 to 90,60",
                                    "double-click commits, Escape cancels") &&
                model_pending_state_set(),
            "a pending state is declared");
    require(contains(success_text(fixture.call("snapshot", "{}")),
                     "pending: a rectangle floats at 10,10 to 90,60 (double-click commits, Escape cancels)"),
            "every reply names the pending state and how to settle it");
    clear_model_pending_state("shape");
    require(!model_pending_state_set() && !contains(success_text(fixture.call("snapshot", "{}")), "pending:"),
            "a cleared pending state is no longer named");
    require(set_model_app_note("Shapes float until committed.") &&
                std::string_view(model_app_note()) == "Shapes float until committed.",
            "the program's note is kept");
    const std::string long_note(5000U, 'x');
    require(!set_model_app_note(long_note.c_str()) &&
                std::string_view(model_app_note()) == "Shapes float until committed.",
            "a note over 4096 bytes is refused and the old one kept");
    require(set_model_app_note(nullptr) && model_app_note()[0] == '\0', "the note clears");

    ModelToolSpec dotted{};
    dotted.name = "test.dotted";
    dotted.description = "A dotted name.";
    require(register_model_tool(dotted, &echo_tool, nullptr) == ModelToolRegistration::invalid_name,
            "a tool name with a dot is refused (clients rename it)");
    std::vector<ToolSpec> specs{};
    fixture.core.fill_tool_specs(specs);
    bool release = false;
    for (const ToolSpec& spec : specs) {
        require(spec.name.find('.') == std::string::npos, "no served tool name has a dot");
        release = release || spec.name == "release_control";
        require(spec.name != "control", "the control tool is retired");
    }
    require(release, "release_control is served");
}

// A window run modally is a dialog whatever its root's role: Plan Paint's
// Resize dialog (a plain group in a modal window) was driven and submitted
// (round 2).
// Shows a window modally, as a program's command opens its dialog.
class ShowModally final {
public:
    explicit ShowModally(const Window& dialog) noexcept : dialog_(dialog) {}
    void operator()(ButtonBase&) const { gui_forms::detail::note_shown_modally(dialog_, true); }

private:
    const Window& dialog_;
};

// Ends a window's modal showing, as a dialog closes once its nested loop
// has ended: after the key that cancelled it.
class EndModally final {
public:
    explicit EndModally(const Window& dialog) noexcept : dialog_(dialog) {}
    void operator()() const { gui_forms::detail::note_shown_modally(dialog_, false); }

private:
    const Window& dialog_;
};

void a_modal_window_is_a_dialog() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.owner.root"));
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    const std::shared_ptr<Panel> dialog_root = make_control<Panel>(StableId("auto.resize"));
    const std::shared_ptr<Button> ok = make_control<Button>(StableId("auto.resize.ok"), "OK");
    (*ok).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*dialog_root).add_child(ok);
    Window dialog(dialog_root, Size{200.0, 120.0});
    dialog.perform_layout();
    host::HeadlessHost dialog_host(dialog);
    std::uint64_t submits = 0U;
    SubscriptionToken submitted =
        (*ok).clicked().subscribe(callbacks::IncrementCounter<std::uint64_t, ButtonBase&>(submits));
    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.owner.window"), "the owner joins");
    require(core.attach_window(dialog, "auto.resize.window"), "the dialog's window joins");
    gui_forms::detail::note_shown_modally(dialog, true);
    WindowPump pump(window, control, presenter);
    const std::string windows = success_text(core.run("windows", "{}", pump));
    require(contains(windows, "dialog (modal window)"), "windows reads the modal window as a dialog");
    require(failed_with(core.run("act", "{\"window\":\"auto.resize.window\",\"action\":\"press\",\"target\":"
                                        "\"auto.resize.ok\"}",
                                 pump),
                        ToolError::dialog_open) &&
                submits == 0U,
            "a model never submits a modal window's dialog");
    gui_forms::detail::note_shown_modally(dialog, false);
}

// Round 3: a modal window the model opened is closed by its own ID, with no
// window named, and reset waits for it to close after its nested loop.
void a_model_opened_modal_window_closes_by_its_id() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.opener.root"));
    const std::shared_ptr<Button> open = make_control<Button>(StableId("auto.opener.resize"), "Resize");
    (*open).set_requested_bounds(Rect{10.0, 10.0, 80.0, 28.0});
    (*root).add_child(open);
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    const std::shared_ptr<Panel> dialog_root = make_control<Panel>(StableId("auto.sized"));
    Window dialog(dialog_root, Size{200.0, 120.0});
    dialog.perform_layout();
    host::HeadlessHost dialog_host(dialog);
    require(window_host.dispatch(HostAttachEvent{{300.0, 200.0}, 1.0}, 500U).accepted() &&
                dialog_host.dispatch(HostAttachEvent{{200.0, 120.0}, 1.0}, 500U).accepted(),
            "the hosts attach");
    SubscriptionToken opened = (*open).clicked().subscribe(ShowModally(dialog));
    TestPresenter presenter{};
    ControlSettings settings{};
    settings.ai_led = true;
    AutomationControl control(settings, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.opener.window"), "the owner joins");
    require(core.attach_window(dialog, "auto.sized.window"), "the dialog's window joins");
    WindowPump pump(window, control, presenter);
    control.begin_ai_led(pump.now());
    const std::string pressed =
        success_text(core.run("act", "{\"action\":\"press\",\"target\":\"auto.opener.resize\"}", pump));
    require(contains(pressed, "a dialog opened: [auto.sized.window]"), "the press opens the modal window");
    // The act runs on the dialog's own window, whose work its pump drains.
    WindowPump dialog_pump(dialog, control, presenter);
    const ToolReply closed =
        core.run("act", "{\"action\":\"close\",\"target\":\"auto.sized.window\"}", dialog_pump);
    const ToolFailure* const refused = closed.failure_value();
    require(refused == nullptr && contains(success_text(closed), "as Cancel: the dialog you opened"),
            refused == nullptr ? "act close names the dialog by its ID alone"
                               : ("act close names the dialog by its ID alone: " + (*refused).message).c_str());
    // The dialog closes on a later turn, as a modal window does.
    static_cast<void>(window.begin_invoke(EndModally(dialog)));
    const std::string reset = success_text(core.run("reset", "{}", pump));
    require(contains(reset, "closed the dialog [auto.sized.window] as Cancel") && !contains(reset, "did not close"),
            "reset waits for the dialog to close");
}


// Round 2, item 1: the model's drag and the user's pointer share one input
// stream. The user's idle pointer never feeds the model's gesture; the
// user's press ends it, and the rest of it is dropped until its release.
void the_users_pointer_never_feeds_a_model_gesture() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.stream.root"));
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    host::HeadlessHost window_host(window);
    require(window_host.dispatch(HostAttachEvent{{300.0, 200.0}, 1.0}, 500U).accepted(), "the host attaches");
    using gui_forms::detail::GestureVerdict;
    require(gui_forms::detail::admit_user_input(window, true, false) == GestureVerdict::deliver,
            "with no model gesture the user's pointer is delivered");
    require(gui_forms::detail::admit_model_pointer(window, true, false, false) == GestureVerdict::deliver,
            "the model's press starts its gesture");
    require(gui_forms::detail::admit_user_input(window, true, false) == GestureVerdict::drop,
            "the user's idle pointer never feeds the model's gesture");
    require(gui_forms::detail::admit_model_pointer(window, false, false, true) == GestureVerdict::deliver,
            "the model's drag goes on");
    require(gui_forms::detail::admit_user_input(window, false, true) == GestureVerdict::cancel_model_gesture,
            "the user's press ends the model's gesture");
    const std::uint64_t dropped = gui_forms::detail::model_events_dropped(window);
    require(gui_forms::detail::admit_model_pointer(window, false, false, true) == GestureVerdict::drop &&
                gui_forms::detail::admit_model_pointer(window, false, true, false) == GestureVerdict::drop &&
                gui_forms::detail::model_events_dropped(window) == dropped + 2U,
            "the rest of the cancelled gesture is dropped and counted");
    require(gui_forms::detail::admit_model_pointer(window, true, false, false) == GestureVerdict::deliver &&
                gui_forms::detail::admit_model_pointer(window, false, true, false) == GestureVerdict::deliver,
            "the model's next gesture is its own again");
    require(gui_forms::detail::model_pointer_id != 0U, "injected pointer events carry their own identity");
}

// Round 2, item 2: the in-use sign's motion (the stripes, the band's march)
// never keeps a window from being idle.
void idle_ignores_the_signs_motion() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    (*fixture.window).set_model_veil(true);
    (*fixture.pump).advance((*fixture.pump).now() + std::chrono::milliseconds(20));
    const stxlib::Optional<FrameTime> settling = (*fixture.window).next_settling_wake();
    require(settling.value() == nullptr && !(*fixture.window).needs_settling_frame(),
            "the stripes' motion is no settling work");
    const std::string pressed = success_text(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}"));
    require(contains(pressed, "idle after") && fixture.presses == 2U,
            "an act under moving stripes settles at once");
    (*fixture.window).set_model_veil(false);
}

// Paints nothing: the host's paint pass, for the settling tests.
class QuietPainter final : public Painter {
public:
    void save() override {}
    void restore() override {}
    void translate(Point) override {}
    void clip_rect(Rect) override {}
    void fill_rect(Rect, Color) override {}
    void stroke_rect(Rect, Color, double) override {}
    void fill_linear_gradient_spread(Rect, Point, Point, std::span<const GradientStop>, GradientSpreadMode) override {}
    void draw_line(Point, Point, Color, double) override {}
    void draw_text_utf8(Point, std::string_view, FontSpec, Color) override {}
    void draw_image(ImageId, Rect, double) override {}
};

// Round 3, item 2: a paint completes the settling work it began with while
// the stripes keep damage coming, so the damage never drains (every generic
// act waited out its 5 s with "a frame is wanted").
void settling_survives_moving_stripes() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    (*fixture.window).set_model_veil(true);
    (*fixture.pump).advance((*fixture.pump).now() + std::chrono::milliseconds(20));
    (*fixture.press).invalidate(Dirty::paint);
    require((*fixture.window).needs_settling_frame(), "the program's own paint is settling work");
    QuietPainter painter{};
    static_cast<void>((*fixture.window).paint(painter));
    // The stripes step again before the host takes the damage.
    static_cast<void>((*fixture.window).poll_frame_schedule((*fixture.pump).now() + std::chrono::milliseconds(200)));
    require(!(*fixture.window).needs_settling_frame(),
            "the painted work is settled although the stripes' damage is still waiting");
    (*fixture.press).invalidate(Dirty::paint);
    require((*fixture.window).needs_settling_frame(), "new work after the paint wants a frame again");
    (*fixture.window).set_model_veil(false);
}

// Round 3, item 1: a tooltip's window holding the activation is never the
// default window; the program window reads as active.
void the_default_window_is_a_program_window() {
    Fixture fixture(false);
    const std::shared_ptr<Panel> tip_root = make_control<Panel>(StableId("auto.tip.root"));
    Window tip(tip_root, Size{1.0, 1.0});
    tip.perform_layout();
    host::HeadlessHost tip_host(tip);
    require(tip_host.dispatch(HostAttachEvent{{1.0, 1.0}, 1.0}, 500U).accepted(), "the tooltip window attaches");
    require(tip_host.dispatch(HostActivationEvent{true}, 600U).accepted() && tip.active(),
            "the tooltip window holds the activation");
    require(fixture.core.attach_window(tip, "auto.window/tool-tip.1", true), "the tooltip window joins");
    const std::string outline = success_text(fixture.call("snapshot", "{}"));
    require(contains(outline, "[auto.window] active") && !contains(outline, "tool-tip"),
            "snapshot {} reads the program window, active, and never the tooltip");
    const std::string windows = success_text(fixture.call("windows", "{}"));
    require(contains(windows, "[auto.window] active"), "windows names the program window active");
    // A read by target (text; this fixture's control captures are not drawn
    // headless, so capture would test the painter, not the lookup).
    require(contains(success_text(fixture.call("text", "{\"target\":\"auto.press\"}")), "Apply"),
            "a target in the program window is found without naming the window");
    fixture.core.detach_window(tip);
}

// Round 3, item 3: a dialog's default and cancel buttons are marked.
void dialog_buttons_are_marked() {
    Fixture fixture(false);
    const std::shared_ptr<Button> ok = make_control<Button>(StableId("auto.ok"), "OK");
    const std::shared_ptr<Button> cancel = make_control<Button>(StableId("auto.cancel"), "Cancel");
    (*ok).set_requested_bounds(Rect{10.0, 170.0, 80.0, 24.0});
    (*cancel).set_requested_bounds(Rect{100.0, 170.0, 80.0, 24.0});
    (*ok).default_button = true;
    (*fixture.root).add_child(ok);
    (*fixture.root).add_child(cancel);
    (*fixture.window).set_cancel_button(cancel);
    (*fixture.window).perform_layout();
    const std::string outline = success_text(fixture.call("snapshot", "{}"));
    require(contains(outline, "[auto.ok]") && contains(outline, " default"), "the default button is marked default");
    require(contains(outline, "[auto.cancel]") && contains(outline, " cancel"), "the cancel button is marked cancel");
    require(!contains(outline, "[auto.press] default"), "an ordinary button is not");
}

// Round 3, item 4: an act that changes nothing says so.
void an_act_without_effect_says_so() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    const std::string escaped = success_text(fixture.call("keyboard", "{\"target\":\"auto.press\",\"keys\":\"Escape\"}"));
    require(contains(escaped, "effect: none") || contains(escaped, "changed:") || contains(escaped, "focus:"),
            "a key's reply names its effect or that it had none");
    require(!contains(escaped, "still busy"), "the key settles at once");
}

// Round 3, item 5: a batch takes more than 64 steps.
void large_batches_run() {
    Fixture fixture(false);
    std::string steps{};
    for (int index = 0; index < 100; ++index) {
        if (!steps.empty()) steps += ',';
        steps += "{\"tool\":\"find\",\"name\":\"Apply\"}";
    }
    const ToolReply batch = fixture.call("batch", "{\"steps\":[" + steps + "]}");
    require(succeeded(batch) && contains(success_text(batch), "100 steps"), "a 100-step batch runs");
}

// Round 3, item 6: capture points answers colours instead of an image.
void capture_samples_points() {
    Fixture fixture(false);
#if defined(GUI_FORMS_TEST_FONT_DIR)
    // A capture draws through a framebuffer painter: the bundled faces'
    // offscreen raster, attached for this check (offscreen_capture_tests).
    OffscreenRasterOptions options{};
    options.font_directory = GUI_FORMS_TEST_FONT_DIR;
    OffscreenRasterCreateResult made = OffscreenRaster::create(options);
    require(made.succeeded(), "the bundled faces make a raster");
    const std::unique_ptr<OffscreenRaster> raster = made.take_raster();
    OffscreenAttachment attached(*raster, *fixture.window);
#else
    // No raster in this build: a headless window captures nothing.
    require(!succeeded(fixture.call("capture", "{\"points\":[[1,1]]}")), "a capture without a raster is refused");
    return;
#endif
    const ToolReply sampled = fixture.call("capture", "{\"points\":[[1,1],[5000,5000]]}");
    require(succeeded(sampled), "a capture with points succeeds");
    const ToolSuccess* const success = sampled.success_value();
    require(success != nullptr && (*success).images.empty(), "no image comes with the colours");
    require(contains((*success).text, "1,1 #") && contains((*success).text, "5000,5000 outside"),
            "each point has its colour, or outside");
    require(!succeeded(fixture.call("capture", "{\"points\":[[1]]}")), "a malformed point is refused");
    // 1.0.5 review: {x, y} is the canonical point, and pairs still work.
    const ToolReply objects = fixture.call("capture", "{\"points\":[{\"x\":1,\"y\":1},[2,2]]}");
    require(succeeded(objects) && contains(success_text(objects), "1,1 #") && contains(success_text(objects), "2,2 #"),
            "capture takes points as {x, y} and as [x, y]");
}

// 1.0.5 review: a program tool's own reply says what it did; a drawing
// changes pixels, not nodes, and must not reply "effect: none".
void a_program_tool_reply_is_an_effect() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    ModelToolSpec draw{};
    draw.name = "test_draw";
    draw.description = "Draws without changing a node.";
    draw.access = ModelToolAccess::acting;
    require(register_model_tool(draw, &echo_tool, nullptr) == ModelToolRegistration::added, "an acting tool");
    static_cast<void>(fixture.call("test_draw", "{}"));
    const ToolReply again = fixture.call("test_draw", "{}");
    require(succeeded(again) && contains(success_text(again), "echoed {}") &&
                !contains(success_text(again), "effect: none"),
            "a second drawing replies with the tool's text and no effect: none");
    require(unregister_model_tool("test_draw"), "the tool is withdrawn");
}

// Round 2, item 3: the served tool list is the same all launch long, so a
// client is sent no list_changed for state the program goes through.
void the_tool_list_is_stable_for_the_launch() {
    Fixture fixture(false);
    std::vector<ToolSpec> before{};
    fixture.core.fill_tool_specs(before);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    (*fixture.check).enabled = false;
    (*fixture.pump).advance((*fixture.pump).now() + std::chrono::milliseconds(20));
    std::vector<ToolSpec> after{};
    fixture.core.fill_tool_specs(after);
    require(before.size() == after.size(), "taking control and disabling a control change no tool");
    for (std::size_t index = 0U; index < before.size(); ++index) {
        require(before[index].name == after[index].name && before[index].description == after[index].description &&
                    before[index].input_schema == after[index].input_schema,
                "every served spec stays the same");
        require(!contains(after[index].description, "Disabled now"), "no spec carries the moment's state");
    }
    (*fixture.check).enabled = true;

    // Two live commands with one stable ID (a menu's Save and the Quick
    // Access Toolbar's) are one tool, so the list binds (round 2).
    const std::shared_ptr<Command> menu_save = std::make_shared<Command>("test-save", "Save");
    const std::shared_ptr<Command> toolbar_save = std::make_shared<Command>("test-save", "Save");
    std::vector<ToolSpec> commands{};
    fixture.core.fill_tool_specs(commands);
    std::size_t named = 0U;
    for (const ToolSpec& spec : commands) named += spec.name == "command_test-save" ? 1U : 0U;
    require(named == 1U, "commands sharing a stable ID are served as one tool");
}


// take_mcp_arguments edits argv, and on macOS argv is where the launch is
// read: the taken arguments still make the launch (round 3: Plan Paint
// called it and served nothing). Last, since what it takes stays taken.
void taken_arguments_still_launch() {
    std::string program = "program";
    std::string led = "--mcp";
    std::string file = "picture.png";
    std::array<char*, 4> argv{program.data(), led.data(), file.data(), nullptr};
    int argc = 3;
    take_mcp_arguments(argc, argv.data());
    require(argc == 2 && std::string_view(argv[1]) == "picture.png" && argv[2] == nullptr,
            "take_mcp_arguments leaves the program its own arguments");
#if defined(GUI_FORMS_TEST_AUTOMATION_RUN)
    require(read_automation_launch().mcp, "the taken --mcp still launches the automation");
#endif
}

// Round 4, item 1: the stripes stand still, so showing them costs one
// paint and then no frame at all.
void the_stripes_ask_for_no_frames() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    (*fixture.window).set_model_veil(true);
    QuietPainter painter{};
    static_cast<void>((*fixture.window).paint(painter));
    const std::uint64_t passes = (*fixture.window).metrics_snapshot().paint_passes;
    for (int step = 1; step <= 60; ++step) {
        const FrameTime later = (*fixture.pump).now() + std::chrono::milliseconds(33 * step);
        static_cast<void>((*fixture.window).poll_frame_schedule(later));
        require(!(*fixture.window).needs_frame(), "no stripe frame wants a paint");
    }
    require((*fixture.window).next_wake().absent(), "the still stripes ask for no wake");
    require((*fixture.window).metrics_snapshot().paint_passes == passes && (*fixture.window).model_veil_frames() == 0U,
            "two seconds of stripes paint the program window no more");
    (*fixture.window).set_model_veil(false);
}

// Round 4, item 1: the band's four strips meet at the corners; their glint
// paints the strips, never the whole window (it was a full-window paint at
// 30 frames a second).
void the_bands_strips_paint_apart() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.strips.root"));
    Window window(root, Size{400.0, 300.0});
    WindowBandOptions options{};
    options.width = 5.0;
    window.declare_band(options);
    window.perform_layout();
    QuietPainter painter{};
    static_cast<void>(window.paint(painter));
    WindowBandAppearance appearance{};
    appearance.state = WindowBandState::sweeping;
    appearance.rest_colour = stxlib::Optional<Color>::of(Color{38U, 114U, 185U, 255U});
    appearance.lit_colour = stxlib::Optional<Color>::of(Color{255U, 255U, 255U, 255U});
    appearance.frequency = 0.6;
    window.set_band_appearance(appearance);
    const MetricsSnapshot before = window.metrics_snapshot();
    FrameTime now = FrameClock::now();
    for (int step = 0; step < 10; ++step) {
        now += std::chrono::milliseconds(34);
        static_cast<void>(window.poll_frame_schedule(now));
        // The hosts' paint: the pending damage, rectangle by rectangle.
        static_cast<void>(window.paint(painter, std::span<const Rect>{}));
    }
    const MetricsSnapshot after = window.metrics_snapshot();
    require(after.paint_passes > before.paint_passes, "the glint repaints the band");
    require(after.full_window_paints == before.full_window_paints, "no glint frame paints the whole window");
}

// Round 4, item 2: an idle wait returns while the sign moves (the band's
// glint, the stripes).
void an_idle_wait_returns_while_the_sign_moves() {
    Fixture fixture(false);
    WindowBandOptions options{};
    options.width = 5.0;
    (*fixture.window).declare_band(options);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    WindowBandAppearance appearance{};
    appearance.state = WindowBandState::sweeping;
    appearance.frequency = 0.6;
    (*fixture.window).set_band_appearance(appearance);
    (*fixture.window).set_model_veil(true);
    const FrameTime started = (*fixture.pump).now();
    const ToolReply waited = fixture.call("wait", "{\"until\":\"idle\",\"timeout_ms\":2000}");
    require(succeeded(waited) && (*fixture.pump).now() - started < std::chrono::milliseconds(500),
            "the wait for idle returns at once although the band sweeps");
    (*fixture.window).set_model_veil(false);
}

// A timer that ticks for as long as it is scheduled.
struct Ticks final {
    std::uint64_t* count{};
    void operator()(FrameTime) const { ++*count; }
};

// The context of busy_draw_tool: the window's timer it leaves running.
struct BusyDraw final {
    FrameRequestToken timer{};
    std::uint64_t ticks{};
};

// A drawing tool that leaves its window busy: a timer every 20 ms.
void busy_draw_tool(void* context, const ModelToolCall& call, ModelToolReply& reply) {
    BusyDraw& busy = *static_cast<BusyDraw*>(context);
    if (call.window != nullptr && (*call.window).root()) {
        busy.timer = (*call.window).schedule_ui_timer(*(*call.window).root(), std::chrono::milliseconds(20),
                                                       FrameClock::now() + std::chrono::milliseconds(20),
                                                       Ticks{&busy.ticks});
    }
    static_cast<void>(reply.text("drew", 4U));
}

// Round 4, item 3: a program tool whose reply is complete returns at once,
// whatever the window goes on doing (plan_paint_draw timed out).
void a_program_tool_returns_without_waiting() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    BusyDraw busy{};
    ModelToolSpec draw{};
    draw.name = "test_busy_draw";
    draw.description = "Draws and leaves a timer running.";
    draw.access = ModelToolAccess::acting;
    draw.settles = true;
    require(register_model_tool(draw, &busy_draw_tool, &busy) == ModelToolRegistration::added, "a settling tool");
    const FrameTime started = (*fixture.pump).now();
    const ToolReply drawn = fixture.call("test_busy_draw", "{}");
    const std::string text = success_text(drawn);
    require(succeeded(drawn) && contains(text, "drew") && !contains(text, "still busy"),
            "the drawing replies with its text and never as still busy");
    require((*fixture.pump).now() - started <= std::chrono::milliseconds(150),
            "the drawing returns within the program tool's short settle");
    busy.timer.disconnect();
    require(unregister_model_tool("test_busy_draw"), "the tool is withdrawn");
}

// Round 4, item 5: activating the window offers no take-back (Alt-Tab and
// Cmd-Tab did); a click or key aimed at the content does.
void activation_offers_no_take_back() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.activate.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("auto.activate.button"), "Go");
    (*button).set_requested_bounds(Rect{20.0, 20.0, 100.0, 28.0});
    (*root).add_child(button);
    Window window(root, Size{300.0, 200.0});
    WindowBandOptions options{};
    options.width = 5.0;
    window.declare_band(options);
    window.perform_layout();
    host::HeadlessHost window_host(window);
    require(window_host.dispatch(HostAttachEvent{{300.0, 200.0}, 1.0}, 500U).accepted(), "the host attaches");
    TestPresenter presenter{};
    AutomationControl control(ControlSettings{}, &presenter);
    AutomationCore core(control);
    require(core.attach_window(window, "auto.activate.window"), "the window joins the automation");
    WindowPump pump(window, control, presenter);
    require(succeeded(core.run("act", "{\"action\":\"press\",\"target\":\"auto.activate.button\"}", pump)),
            "the model takes control");
    std::uint64_t at = 1000U;
    KeyEvent meta{};
    meta.action = KeyAction::down;
    meta.physical_key = PhysicalKey::left_meta;
    static_cast<void>(window_host.dispatch(meta, at++));
    KeyEvent alt{};
    alt.action = KeyAction::down;
    alt.physical_key = PhysicalKey::left_alt;
    static_cast<void>(window_host.dispatch(alt, at++));
    KeyEvent switcher{};
    switcher.action = KeyAction::down;
    switcher.physical_key = PhysicalKey::tab;
    switcher.modifiers = Modifier::alt;
    static_cast<void>(window_host.dispatch(switcher, at++));
    PointerEvent on_band{};
    on_band.action = PointerAction::down;
    on_band.button = PointerButton::primary;
    on_band.position = Point{150.0, 1.0};
    on_band.click_count = 1U;
    static_cast<void>(window_host.dispatch(on_band, at++));
    PointerEvent released = on_band;
    released.action = PointerAction::up;
    static_cast<void>(window_host.dispatch(released, at++));
    static_cast<void>(window.drain_posted_work());
    require(presenter.take_back_offers == 0U,
            "a modifier alone, a switcher's chord and a press on the band offer no take-back");
    require(control.state() == ControlState::model_control, "the model still holds control");
    KeyEvent letter{};
    letter.action = KeyAction::down;
    letter.physical_key = PhysicalKey::a;
    static_cast<void>(window_host.dispatch(letter, at++));
    static_cast<void>(window.drain_posted_work());
    require(presenter.take_back_offers == 1U, "a key aimed at the content offers it");
}

// Round 4, item 4: one question per session. The connection question's
// Allow is the consent to control; an allowed take holds for the session,
// an idle return included; release_control or a take-back asks again; a
// new session asks again.
void one_question_per_session() {
    Fixture fixture(false);
    fixture.control.grant_session_consent();
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presenter.takes == 0U && fixture.control.state() == ControlState::model_control,
            "after the connection question the first act takes control with no countdown");
    require(succeeded(fixture.call("release_control", "{}")), "the model releases control");
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presenter.takes == 1U,
            "after release_control a new act asks (the countdown)");
    fixture.control.tick((*fixture.pump).now() + std::chrono::seconds(31));
    require(fixture.control.state() == ControlState::user_control, "control returns after the grace period");
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")) &&
                fixture.presenter.takes == 1U && fixture.control.state() == ControlState::model_control,
            "within the session an idle return asks nothing more");
    fixture.control.user_took_back();
    require(!fixture.control.session_consent(), "the user's take-back ends the session's consent");
    fixture.control.grant_session_consent();
    fixture.control.begin_session((*fixture.pump).now());
    require(!fixture.control.session_consent(), "a new session (a connection without the token) asks again");
}

// Round 4, item 7: while a model holds control no tooltip shows; one that
// shows closes at once; they come back on release.
void tooltips_rest_while_a_model_holds_control() {
    const std::shared_ptr<Panel> root = make_control<Panel>(StableId("auto.tips.root"));
    const std::shared_ptr<Button> button = make_control<Button>(StableId("auto.tips.button"), "Go");
    (*button).set_requested_bounds(Rect{20.0, 20.0, 100.0, 28.0});
    (*button).tool_tip_text = "Goes";
    (*root).add_child(button);
    Window window(root, Size{300.0, 200.0});
    window.perform_layout();
    window.show_tool_tip(button);
    const bool shown_before = window.tool_tip_visible();
    StockPresenter presenter{};
    presenter.window_attached(window);
    ClientIdentity client{};
    client.client_name = "Claude Code";
    presenter.client_changed(client);
    require(!window.tool_tip_visible() || shown_before, "connecting shows no tag line");
    presenter.control_changed(ControlState::model_control, client);
    require(!window.tool_tip_visible() && !window.tool_tips_shown(),
            "taking control closes the tooltip and holds them all");
    window.show_tool_tip(button);
    require(!window.tool_tip_visible(), "not even the program's own shows under control");
    require(window.tool_tip_target() == nullptr, "no tooltip waits");
    presenter.control_changed(ControlState::user_control, client);
    require(window.tool_tips_shown(), "the tooltips resume on release");
    presenter.disconnected();
    presenter.window_detached(window);
}

// Round 4, the close bug: a client stopped or killed mid-control leaves the
// program the user's to close: the model's question scope closes (the save
// question was answered Cancel for a model that was gone) and control
// returns at once; stdin's end (the hard disconnect) the same.
void a_dropped_client_leaves_the_program_closable() {
    Fixture fixture(false);
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the model takes control");
    gui_forms::detail::begin_model_questions(77U, gui_forms::detail::ModelQuestionAnswer::none);
    fixture.core.connection_dropped();
    HostDialogRequest question{};
    question.request_id = 9U;
    HostMessageDialogRequest message{};
    message.title = "Save changes?";
    message.buttons = HostMessageButtons::yes_no_cancel;
    question.payload = message;
    HostDialogResult answered{};
    require(!gui_forms::detail::model_questions_open() &&
                !gui_forms::detail::answer_model_question(question, answered),
            "after the drop the program's save question is the user's");
    require(fixture.control.state() == ControlState::user_control, "control returns to the user at once");
    require(!fixture.core.session_ended(), "the session waits for its resume");
    fixture.core.connection_resumed((*fixture.pump).now());
    require(succeeded(fixture.call("act", "{\"action\":\"press\",\"target\":\"auto.press\"}")),
            "the resumed session acts again");
    gui_forms::detail::begin_model_questions(78U, gui_forms::detail::ModelQuestionAnswer::none);
    fixture.core.hard_disconnect();
    require(!gui_forms::detail::model_questions_open() && fixture.control.state() == ControlState::user_control,
            "stdin's end leaves the questions the user's and control with the user");
}

} // namespace

int main() {
    reads_name_the_window_and_hide_the_password();
    acts_press_and_reply_with_the_diff();
    a_user_step_needs_the_user();
    destructive_acts_ask_once_per_launch();
    dialogs_refuse_the_model_until_instrumented();
    a_native_alert_is_named_native_not_the_tooltip();
    batch_runs_its_steps_in_one_call();
    the_public_registry_adds_tools();
    a_program_tool_meets_the_dialog_rule();
    inspect_traces_the_routed_events();
    the_stock_presenter_shows_the_tag_line_on_hover();
    tool_schemas_stay_in_the_served_subset();
    arguments_are_strict_and_find_finds();
    rows_a_list_does_not_show_are_reached_by_id();
    a_model_closes_the_dialog_it_opened_but_never_submits_it();
    the_sign_shows_ai_mode_and_review_mode_shows_none();
    a_first_act_asks_to_take_control();
    the_take_countdown_allows_when_it_ends();
    a_refused_take_blocks_the_act();
    control_returns_after_the_grace_period();
    release_control_gives_control_back();
    a_press_offers_take_back_and_taking_back_interrupts();
    an_ai_led_launch_holds_control_without_asking();
    popup_windows_carry_no_sign();
    round_two_replies_are_structured_and_strict();
    select_on_a_pressable_item_says_it_only_selects();
    a_modal_window_is_a_dialog();
    a_model_opened_modal_window_closes_by_its_id();
    take_back_answers_over_model_modals();
    a_models_close_answers_the_programs_question();
    disconnect_soft_balks_and_hard_ends();
    the_users_pointer_never_feeds_a_model_gesture();
    idle_ignores_the_signs_motion();
    settling_survives_moving_stripes();
    the_default_window_is_a_program_window();
    dialog_buttons_are_marked();
    an_act_without_effect_says_so();
    large_batches_run();
    capture_samples_points();
    a_program_tool_reply_is_an_effect();
    the_tool_list_is_stable_for_the_launch();
    taken_arguments_still_launch();
    the_stripes_ask_for_no_frames();
    the_bands_strips_paint_apart();
    an_idle_wait_returns_while_the_sign_moves();
    a_program_tool_returns_without_waiting();
    activation_offers_no_take_back();
    one_question_per_session();
    tooltips_rest_while_a_model_holds_control();
    a_dropped_client_leaves_the_program_closable();
    std::cout << "automation core tests passed\n";
    return 0;
}
