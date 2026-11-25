#import <AppKit/AppKit.h>

#include "macos_host.hpp"

#include "gui_forms/host/services/host_services.hpp"
#include "gui_forms/host/types/host_types.hpp"
#include "src/core/host/session/session_directory.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <variant>

// Owner, round 3: a native alert the program runs while a model holds
// control. The user's click on the alert is held back and offers the
// take-back dialog; that dialog (a panel that works when modal, as the
// presenter's is: ApplicationWindow::outside_modality) shows above the alert
// and takes the user's click; taking back leaves the alert open, and the
// alert then answers the user. The model's control is a stand-in gate
// (session_directory.hpp, native_input_held) whose press shows the panel,
// as AutomationControl::native_gate offers the take-back dialog. The clicks
// are synthetic mouse events posted to the application's queue; they reach
// only an active application, so ctest launches this bundle through
// cmake/run_macos_activated_test.cmake.

namespace {

struct TakeBackProbe final {
    bool model_holds{true};
    std::uint32_t presses{};
    __strong NSPanel* panel{};
    bool dialog_finished{};
    bool chain_finished{};
    bool alert_open_after_held_click{};
    bool panel_shown_over_alert{};
    bool panel_click_reached{};
    bool alert_open_after_take_back{};
    bool closed_by_watchdog{};
    std::uint32_t polls_left{};
};

TakeBackProbe* probe_record = nullptr;

bool stand_in_gate(void* context, bool press) noexcept {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    if (probe.model_holds && press) {
        ++probe.presses;
        [probe.panel makeKeyAndOrderFront:nil];
    }
    return probe.model_holds;
}

} // namespace

// The take-back panel's button: taking back ends the stand-in's control.
@interface TakeBackTarget : NSObject
- (void)takeBack:(id)sender;
@end

@implementation TakeBackTarget
- (void)takeBack:(id)sender {
    (void)sender;
    if (probe_record == nullptr) return;
    (*probe_record).panel_click_reached = true;
    (*probe_record).model_holds = false;
    [(*probe_record).panel orderOut:nil];
}
@end

namespace {

constexpr std::int64_t step_nanoseconds = 400 * NSEC_PER_MSEC;

void post_after(std::int64_t delay_nanoseconds, TakeBackProbe* probe, dispatch_function_t function) {
    const dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, delay_nanoseconds);
    dispatch_after_f(when, dispatch_get_main_queue(), probe, function);
}

NSButton* find_button(NSView* view, NSString* title) {
    for (NSView* child in view.subviews) {
        if ([child isKindOfClass:[NSButton class]] && [static_cast<NSButton*>(child).title isEqualToString:title]) {
            return static_cast<NSButton*>(child);
        }
        NSButton* const nested = find_button(child, title);
        if (nested != nil) return nested;
    }
    return nil;
}

NSEvent* mouse_event(NSEventType type, NSPoint location, NSWindow* window) {
    NSEvent* event = [NSEvent mouseEventWithType:type
                                        location:location
                                   modifierFlags:0
                                       timestamp:[NSProcessInfo processInfo].systemUptime
                                    windowNumber:window.windowNumber
                                         context:nil
                                     eventNumber:0
                                      clickCount:1
                                        pressure:1.0F];
    return event;
}

// A click at the centre of `button`, posted as a down and an up.
bool click(NSButton* button) {
    if (button == nil || button.window == nil) return false;
    const NSRect bounds = [button convertRect:button.bounds toView:nil];
    const NSPoint centre = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
    [NSApp postEvent:mouse_event(NSEventTypeLeftMouseDown, centre, button.window) atStart:NO];
    [NSApp postEvent:mouse_event(NSEventTypeLeftMouseUp, centre, button.window) atStart:NO];
    return true;
}

void watch_dialog(void* context) {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    if (probe.dialog_finished) {
        probe.chain_finished = true;
        return;
    }
    if (probe.polls_left == 0U) {
        NSWindow* window = [NSApp modalWindow];
        [window orderOut:nil];
        [NSApp abortModal];
        probe.closed_by_watchdog = true;
        probe.chain_finished = true;
        return;
    }
    --probe.polls_left;
    post_after(20 * NSEC_PER_MSEC, &probe, &watch_dialog);
}

// 4: the user answers the alert, now theirs: No.
void answer_alert(void* context) {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    NSWindow* const alert = [NSApp modalWindow];
    probe.alert_open_after_take_back = !probe.dialog_finished && alert != nil && alert != probe.panel;
    if (alert != nil) static_cast<void>(click(find_button(alert.contentView, @"No")));
    probe.polls_left = 100U;
    post_after(20 * NSEC_PER_MSEC, &probe, &watch_dialog);
}

// 3: the panel is above the alert; the user clicks Take Back Control.
void take_back(void* context) {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    NSWindow* const alert = [NSApp modalWindow];
    probe.alert_open_after_held_click = !probe.dialog_finished && alert != nil;
    // Panels are not among NSApp.orderedWindows: above means a higher level
    // and on screen.
    probe.panel_shown_over_alert = probe.panel.visible && alert != nil && probe.panel.level > alert.level &&
        (probe.panel.occlusionState & NSWindowOcclusionStateVisible) != 0;

    static_cast<void>(click(find_button(probe.panel.contentView, @"Take Back Control")));
    post_after(step_nanoseconds, &probe, &answer_alert);
}

// 2: the user clicks the alert's Yes while the model holds control.
void click_alert(void* context) {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    NSWindow* const alert = [NSApp modalWindow];
    if (alert == nil) {
        // The alert never showed; the watchdog ends the run.
        probe.polls_left = 0U;
        watch_dialog(context);
        return;
    }
    static_cast<void>(click(find_button(alert.contentView, @"Yes")));
    post_after(step_nanoseconds, &probe, &take_back);
}

void drain_probe(const TakeBackProbe& probe) {
    while (!probe.chain_finished) {
        NSDate* until = [NSDate dateWithTimeIntervalSinceNow:0.01];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:until];
    }
}

NSPanel* make_take_back_panel(TakeBackTarget* target) {
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0.0, 0.0, 360.0, 140.0)
                                                styleMask:NSWindowStyleMaskTitled
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    panel.title = @"Take back control?";
    // As the host makes a window outside modality (macos_application_run.mm).
    [panel setWorksWhenModal:YES];
    [panel setLevel:NSModalPanelWindowLevel + 1];
    [panel setReleasedWhenClosed:NO];
    [panel setHidesOnDeactivate:NO];
    NSButton* button = [NSButton buttonWithTitle:@"Take Back Control" target:target action:@selector(takeBack:)];
    button.frame = NSMakeRect(180.0, 20.0, 160.0, 32.0);
    [panel.contentView addSubview:button];
    [panel center];
    return panel;
}

} // namespace

int main() {
    int failures = 0;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        [NSApp activate];
        gui_forms::host::MacHostServiceOptions options;
        std::unique_ptr<gui_forms::HostServices> services = gui_forms::host::make_macos_host_services(options);
        if (!services) return 2;
        TakeBackTarget* target = [[TakeBackTarget alloc] init];
        TakeBackProbe probe;
        probe.panel = make_take_back_panel(target);
        probe_record = &probe;
        gui_forms::detail::install_native_input_gate(&stand_in_gate, &probe);

        gui_forms::HostMessageDialogRequest request;
        request.title = "Save changes?";
        request.message = "Save changes to Untitled?";
        request.buttons = gui_forms::HostMessageButtons::yes_no_cancel;
        request.default_choice = gui_forms::HostDialogChoice::cancel;
        post_after(step_nanoseconds, &probe, &click_alert);
        const gui_forms::HostDialogResult result = (*services).show_dialog({1U, "test.window", std::move(request)});
        probe.dialog_finished = true;
        drain_probe(probe);
        gui_forms::detail::remove_user_input_gate(&probe);
        probe_record = nullptr;

        gui_forms::HostDialogChoice choice = gui_forms::HostDialogChoice::none;
        if (const gui_forms::HostMessageDialogResult* answer =
                std::get_if<gui_forms::HostMessageDialogResult>(&result.payload)) {
            choice = (*answer).choice;
        }
        const bool checks[] = {
            result.status.accepted(),
            !probe.closed_by_watchdog,
            probe.presses == 1U,
            probe.alert_open_after_held_click,
            probe.panel_shown_over_alert,
            probe.panel_click_reached,
            probe.alert_open_after_take_back,
            choice == gui_forms::HostDialogChoice::no,
        };
        const char* const names[] = {
            "the dialog ran", "no watchdog", "the held click offered take-back once",
            "the held click left the alert open", "the take-back panel shows above the alert",
            "the take-back panel takes the click", "taking back leaves the alert open",
            "the alert then answers the user",
        };
        for (std::size_t index = 0U; index < sizeof(checks) / sizeof(checks[0]); ++index) {
            std::cout << (checks[index] ? "ok   " : "FAIL ") << names[index] << '\n';
            if (!checks[index]) ++failures;
        }
        (*services).shutdown();
    }
    std::cout << "failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
