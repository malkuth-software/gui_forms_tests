// ToastNotification and the portable desktop notification contract of
// HostServices: identities, validation, the live bound, events, threads and
// shutdown in the base class; the headless reference host; and the
// ToastNotification component's properties, show, hide, replacement, events
// and refusals.

#include "gui_forms/components/toast_notification/toast_notification.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/delegate/delegate.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/host/services/host_services.hpp"
#include "gui_forms/host/types/host_capabilities/host_capabilities.hpp"
#include "gui_forms/host/types/host_capability/host_capability.hpp"
#include "gui_forms/host/types/host_types.hpp"
#include "gui_forms/window/window.hpp"
#include "headless_host.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

HostDesktopNotificationRequest notification(std::string title, std::string text) {
    HostDesktopNotificationRequest request;
    request.title = std::move(title);
    request.text = std::move(text);
    return request;
}

// A host without desktop notifications: every pure service refuses.
class QuietServices final : public HostServices {
public:
    QuietServices() : HostServices({HostCapabilities::current_protocol_version, "quiet", HostCapability::lifecycle}) {}
    ~QuietServices() override { shutdown(); }

protected:
    HostMonitorResult query_monitors_impl() override { return {{HostServiceError::unsupported}, {}}; }
    HostServiceStatus set_cursor_impl(CursorKind) override { return {HostServiceError::unsupported}; }
    HostServiceStatus set_pointer_capture_impl(bool, std::uint64_t) override { return {HostServiceError::unsupported}; }
    HostClipboardTextResult read_clipboard_text_impl() override { return {}; }
    HostServiceStatus write_clipboard_text_impl(std::string_view) override { return {HostServiceError::unsupported}; }
    HostDialogResult show_dialog_impl(const HostDialogRequest&) override { return {}; }
    HostServiceStatus play_sound_cue_impl(const HostSoundCueRequest&) override { return {HostServiceError::unsupported}; }
};

struct EventLog final {
    std::vector<HostDesktopNotificationEvent> events;
    // Whether the identity was still live when its event was raised.
    std::vector<bool> live_during_event;
    HostServices* services{};
    void on_event(const HostDesktopNotificationEvent& event) {
        events.push_back(event);
        live_during_event.push_back((*services).desktop_notification_live(event.notification_id));
    }
};

struct OtherThreadShow final {
    HostServices* services{};
    HostServiceError* error{};
    void operator()() const { *error = (*services).show_desktop_notification(notification("t", "x")).status.error; }
};

void contract() {
    host::HeadlessHostServices services;
    require(services.snapshot().capabilities.supports(HostCapability::desktop_notifications),
            "the headless reference host reports desktop notifications");
    require(services.snapshot().capabilities.to_json().find("desktop_notifications") != std::string::npos,
            "capabilities name the desktop notification flag");
    require(services.show_desktop_notification(notification("Scan", "")).status.error ==
                HostServiceError::invalid_argument,
            "a notification without text is refused");
    require(services.show_desktop_notification(notification(std::string(256U, 'a'), "x")).status.error ==
                HostServiceError::too_large,
            "a title over 255 bytes is refused");
    require(services.show_desktop_notification(notification("t", std::string(1024U, 'a'))).status.error ==
                HostServiceError::too_large,
            "text over 1023 bytes is refused");
    require(services.show_desktop_notification(notification("t", std::string("\xFF"))).status.error ==
                HostServiceError::invalid_utf8,
            "text that is not UTF-8 is refused");
    require(services.show_desktop_notification(notification("t", std::string("a\0b", 3U))).status.error ==
                HostServiceError::invalid_argument,
            "text holding a NUL is refused");
    HostDesktopNotificationRequest odd = notification("t", "x");
    odd.urgency = static_cast<HostDesktopNotificationUrgency>(9U);
    require(services.show_desktop_notification(odd).status.error == HostServiceError::invalid_argument,
            "an urgency outside the enumeration is refused");
    require(services.snapshot().desktop_notifications == 0U && services.desktop_notification_trace().empty(),
            "refused requests reach no platform");

    EventLog log;
    log.services = &services;
    SubscriptionToken token = services.desktop_notification_events().subscribe(
        Delegate<const HostDesktopNotificationEvent&>::bind<EventLog, &EventLog::on_event>(log));
    HostDesktopNotificationRequest first = notification("Backup", "The backup finished.");
    first.urgency = HostDesktopNotificationUrgency::critical;
    first.silent = true;
    const HostDesktopNotificationResult shown = services.show_desktop_notification(first);
    require(shown.status.accepted() && shown.notification_id == HostDesktopNotificationId{1U} &&
                services.desktop_notification_live(HostDesktopNotificationId{1U}) &&
                services.snapshot().desktop_notifications == 1U && services.snapshot().desktop_notifications_live == 1U,
            "a notification is shown with the first identity");
    require(services.desktop_notification(HostDesktopNotificationId{1U}) != nullptr &&
                services.desktop_notification_trace().find(
                    "notification show id=1 urgency=critical silent=1 title=Backup text=The backup finished.") !=
                    std::string::npos,
            "the host receives the request whole");

    services.simulate_desktop_notification_event(
        {HostDesktopNotificationId{1U}, HostDesktopNotificationAction::activated});
    services.simulate_desktop_notification_event(
        {HostDesktopNotificationId{1U}, HostDesktopNotificationAction::dismissed});
    services.simulate_desktop_notification_event({HostDesktopNotificationId{7U}, HostDesktopNotificationAction::failed});
    require(log.events.size() == 1U && log.events[0].action == HostDesktopNotificationAction::activated &&
                !log.live_during_event[0] && !services.desktop_notification_live(HostDesktopNotificationId{1U}) &&
                services.snapshot().desktop_notification_events == 1U,
            "one event per live notification, raised after the identity ends");

    const HostDesktopNotificationResult second = services.show_desktop_notification(notification("", "Plain"));
    require(second.notification_id == HostDesktopNotificationId{2U}, "identities are not reused");
    require(services.withdraw_desktop_notification(HostDesktopNotificationId{2U}).accepted() &&
                !services.desktop_notification_live(HostDesktopNotificationId{2U}) &&
                services.desktop_notification(HostDesktopNotificationId{2U}) == nullptr &&
                services.desktop_notification_trace().find("notification withdraw id=2") != std::string::npos &&
                services.snapshot().desktop_notification_withdrawals == 1U,
            "a withdrawn notification leaves the platform");
    require(services.withdraw_desktop_notification(HostDesktopNotificationId{2U}).error ==
                HostServiceError::invalid_argument,
            "a withdrawn identity is not live");
    services.simulate_desktop_notification_event(
        {HostDesktopNotificationId{2U}, HostDesktopNotificationAction::dismissed});
    require(log.events.size() == 1U, "a withdrawn notification raises nothing");

    services.fail_next_desktop_notification();
    require(services.show_desktop_notification(notification("t", "x")).status.error ==
                HostServiceError::backend_failure,
            "a platform failure is reported");
    const HostDesktopNotificationResult after_failure = services.show_desktop_notification(notification("t", "x"));
    require(after_failure.notification_id == HostDesktopNotificationId{3U},
            "a failed request takes no identity");

    for (std::size_t index = 1U; index < HostServices::maximum_live_desktop_notifications; ++index) {
        require(services.show_desktop_notification(notification("t", "more")).status.accepted(),
                "notifications up to the bound are shown");
    }
    require(services.snapshot().desktop_notifications_live == HostServices::maximum_live_desktop_notifications,
            "the live list reaches its bound");
    const HostDesktopNotificationResult past = services.show_desktop_notification(notification("t", "past"));
    require(past.status.accepted() && !services.desktop_notification_live(HostDesktopNotificationId{3U}) &&
                services.desktop_notification_trace().find("notification withdraw id=3") != std::string::npos &&
                services.snapshot().desktop_notifications_live == HostServices::maximum_live_desktop_notifications,
            "past the bound the oldest is withdrawn");

    HostServiceError other_thread = HostServiceError::none;
    std::thread worker(OtherThreadShow{&services, &other_thread});
    worker.join();
    require(other_thread == HostServiceError::wrong_thread, "another thread is refused");

    services.shutdown();
    require(services.snapshot().desktop_notifications_live == 0U &&
                !services.desktop_notification_live(past.notification_id),
            "shutdown forgets every identity");
    require(services.show_desktop_notification(notification("t", "late")).status.error ==
                HostServiceError::after_shutdown,
            "a shut-down host refuses notifications");

    QuietServices quiet;
    require(quiet.show_desktop_notification(notification("t", "x")).status.error == HostServiceError::unsupported &&
                quiet.withdraw_desktop_notification(HostDesktopNotificationId{1U}).error ==
                    HostServiceError::unsupported,
            "a host without the capability refuses as unsupported");
}

struct ToastLog final {
    int activations{};
    int dismissals{};
    int failures{};
    void on_activated() { ++activations; }
    void on_dismissed() { ++dismissals; }
    void on_failed() { ++failures; }
};

// Shows the notification again from its own activated handler.
struct ShowAgain final {
    ToastNotification* toast{};
    HostServiceStatus status{HostServiceError::backend_failure};
    void on_activated() { status = (*toast).show(); }
};

template <typename Action>
bool throws_invalid(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct SetTitle final {
    ToastNotification* toast{};
    std::string value;
    void operator()() const { (*toast).title = value; }
};

struct SetText final {
    ToastNotification* toast{};
    std::string value;
    void operator()() const { (*toast).text = value; }
};

void component() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("toast.root"));
    Window window(root, {320.0, 200.0});

    ToastNotification unhosted(window);
    unhosted.text = "Nothing hosts this";
    require(!unhosted.supported() && unhosted.show().error == HostServiceError::unsupported && !unhosted.shown(),
            "showing without a host is refused as unsupported");
    unhosted.dispose();

    host::HeadlessHost headless(window);
    host::HeadlessHostServices& services = static_cast<host::HeadlessHostServices&>(headless.services());
    ToastNotification toast(window);
    require(toast.supported() && toast.urgency.read() == HostDesktopNotificationUrgency::normal &&
                !toast.silent.read(),
            "the headless host supports desktop notifications; the defaults are normal and audible");
    require(throws_invalid(SetTitle{&toast, std::string(256U, 't')}) &&
                throws_invalid(SetText{&toast, std::string(1024U, 'x')}) &&
                throws_invalid(SetText{&toast, std::string("a\0b", 3U)}),
            "a title or text the host would refuse throws");
    require(toast.show().error == HostServiceError::invalid_argument && !toast.shown(),
            "a notification without text is refused");

    ToastLog log;
    std::vector<SubscriptionToken> tokens;
    tokens.push_back(toast.activated().subscribe(Delegate<>::bind<ToastLog, &ToastLog::on_activated>(log)));
    tokens.push_back(toast.dismissed().subscribe(Delegate<>::bind<ToastLog, &ToastLog::on_dismissed>(log)));
    tokens.push_back(toast.failed().subscribe(Delegate<>::bind<ToastLog, &ToastLog::on_failed>(log)));
    toast.title = "Backup";
    toast.text = "The backup finished.";
    toast.urgency = HostDesktopNotificationUrgency::low;
    toast.silent = true;
    require(toast.show().accepted() && toast.shown() && toast.status().accepted(),
            "a notification with text is shown");
    require(services.desktop_notification_trace().find(
                "urgency=low silent=1 title=Backup text=The backup finished.") != std::string::npos,
            "the properties reach the host");

    require(toast.show().accepted() && services.snapshot().desktop_notifications_live == 1U &&
                services.desktop_notification_trace().find("notification withdraw id=1") != std::string::npos,
            "showing again replaces the notification");
    services.simulate_desktop_notification_event(
        {HostDesktopNotificationId{1U}, HostDesktopNotificationAction::activated});
    require(log.activations == 0, "the replaced notification raises nothing");
    services.simulate_desktop_notification_event(
        {HostDesktopNotificationId{2U}, HostDesktopNotificationAction::dismissed});
    require(log.dismissals == 1 && !toast.shown(), "a dismissal is raised and ends the notification");

    require(toast.show().accepted(), "the notification shows again");
    services.simulate_desktop_notification_event({HostDesktopNotificationId{3U}, HostDesktopNotificationAction::failed});
    require(log.failures == 1 && !toast.shown(), "a platform refusal is raised as failed");

    require(toast.show().accepted() && toast.hide().accepted() && !toast.shown() &&
                services.snapshot().desktop_notifications_live == 0U,
            "hide withdraws the notification");
    require(toast.hide().accepted(), "hide without a live notification changes nothing");

    ShowAgain again{&toast};
    tokens.push_back(toast.activated().subscribe(Delegate<>::bind<ShowAgain, &ShowAgain::on_activated>(again)));
    require(toast.show().accepted(), "the notification shows before its activation");
    services.simulate_desktop_notification_event({HostDesktopNotificationId{5U}, HostDesktopNotificationAction::activated});
    require(log.activations == 1 && again.status.accepted() && toast.shown() &&
                services.desktop_notification_live(HostDesktopNotificationId{6U}),
            "an activated handler may show the notification again");

    services.fail_next_desktop_notification();
    require(toast.show().error == HostServiceError::backend_failure && !toast.shown(),
            "a platform failure leaves nothing shown");

    toast.dispose();
    bool refused = false;
    try {
        static_cast<void>(toast.show());
    } catch (const std::logic_error&) {
        refused = true;
    }
    require(refused, "a disposed notification refuses show");
}

} // namespace

int main() {
    contract();
    component();
    std::cout << "ToastNotification: the desktop notification contract, the headless host and the component passed\n";
    return 0;
}
