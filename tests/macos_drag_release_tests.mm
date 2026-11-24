// The button release a drag from the program gives back to its window when
// AppKit's dragging session ends (appkit_drag_release.hpp): the session
// consumes the mouse-up, so the host rebuilds it for the button the drag
// started with, at the session's end point, and sends it to the view. A
// real session needs a held button, so the drag itself is a hand check (the
// integration queue entry for host-drag-and-drop); this drives the release
// the host sends at its end.
//
// Hostile handlers under a drag (house style section 5): a timer closes the
// source window, or destroys its services, while a drag runs. The drag ends
// once, cancelled, and AppKit's later session end touches nothing that is
// gone (the sanitizer tree reports any use of freed state). The drag runs
// through the test seam macos_start_drag_for_testing, without a session.

#import <AppKit/AppKit.h>
#include "src/surface/macos/services/appkit_drag_release.hpp"
#include "src/surface/macos/compositor/macos_compositor.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/platform/macos_host.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

// Records the last release each button sent it.
@interface GuiFormsReleaseRecorder : NSView
@property(nonatomic, assign) NSEventType lastType;
@property(nonatomic, assign) NSPoint lastLocation;
@property(nonatomic, assign) int releases;
@end

@implementation GuiFormsReleaseRecorder
- (void)mouseUp:(NSEvent*)event {
    [self record:event];
}
- (void)rightMouseUp:(NSEvent*)event {
    [self record:event];
}
- (void)otherMouseUp:(NSEvent*)event {
    [self record:event];
}
- (void)record:(NSEvent*)event {
    self.lastType = event.type;
    self.lastLocation = event.locationInWindow;
    self.releases = self.releases + 1;
}
@end

// The hostile timer's target: closes the window, telling its services as
// the view's notifyClosedWithReason: does, or destroys the services.
@interface GuiFormsHostileDragHandler : NSObject
@property(nonatomic, assign) std::unique_ptr<gui_forms::HostServices>* services;
@property(nonatomic, strong) NSWindow* window;
@property(nonatomic, assign) BOOL destroyServices;
@property(nonatomic, assign) BOOL fired;
- (void)fire:(NSTimer*)timer;
@end

@implementation GuiFormsHostileDragHandler
- (void)fire:(NSTimer*)timer {
    (void)timer;
    self.fired = YES;
    if (self.destroyServices) {
        (*self.services).reset();
        return;
    }
    gui_forms::host::detail::macos_drag_source_window_closed(**self.services);
    [self.window close];
}
@end

namespace {
// AppKit's dragging session is never made by a test; the source is told of
// its end with none, as AppKit does for a session it has already dropped.
NSDraggingSession* no_session() {
    return nil;
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double a, double b) {
    return std::fabs(a - b) < 0.5;
}

// Records each drag end.
struct RecordEnds final {
    // The test's record (observing).
    std::vector<gui_forms::HostDragSourceEnd>& ends;
    void operator()(const gui_forms::HostDragSourceEnd& end) const { ends.push_back(end); }
};

// Runs the run loop until the hostile timer has fired, for at most two
// seconds.
void run_until_fired(GuiFormsHostileDragHandler* handler) {
    NSDate* const give_up = [NSDate dateWithTimeIntervalSinceNow:2.0];
    while (handler.fired == NO && [give_up timeIntervalSinceNow] > 0.0) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    require(handler.fired == YES, "the hostile timer fires");
}

// A timer closes the source window during its drag: the drag ends then,
// cancelled, and AppKit's later end reports nothing and sends the closed
// window's view no release.
void drag_survives_window_close() {
    NSWindow* const window = [[NSWindow alloc] initWithContentRect:NSMakeRect(200.0, 300.0, 320.0, 180.0)
                                                         styleMask:NSWindowStyleMaskBorderless
                                                           backing:NSBackingStoreBuffered
                                                             defer:NO];
    window.releasedWhenClosed = NO;
    GuiFormsReleaseRecorder* const view =
        [[GuiFormsReleaseRecorder alloc] initWithFrame:NSMakeRect(0.0, 0.0, 320.0, 180.0)];
    window.contentView = view;
    std::unique_ptr<gui_forms::HostServices> services =
        gui_forms::host::make_macos_host_services({"gui_forms.drag.hostile.close", false});
    gui_forms::host::detail::bind_macos_backdrop_view(*services, view);
    std::vector<gui_forms::HostDragSourceEnd> ends;
    gui_forms::SubscriptionToken observation = (*services).drag_source_ended().subscribe(RecordEnds{ends});
    const gui_forms::HostDragSourceRequest request{{gui_forms::DragTextData{"dragged"}},
                                                   gui_forms::DragEffect::copy | gui_forms::DragEffect::move};
    id<NSDraggingSource> const source = gui_forms::host::detail::macos_start_drag_for_testing(*services, request);
    require(source != nil && (*services).drag_source_active(), "a drag starts");
    GuiFormsHostileDragHandler* const handler = [[GuiFormsHostileDragHandler alloc] init];
    handler.services = &services;
    handler.window = window;
    [NSTimer scheduledTimerWithTimeInterval:0.02 target:handler selector:@selector(fire:) userInfo:nil repeats:NO];
    run_until_fired(handler);
    require(ends.size() == 1U && ends[0].performed_effect == gui_forms::DragEffect::none &&
                !(*services).drag_source_active(),
            "a drag whose window closed ends once, cancelled");
    [source draggingSession:no_session() endedAtPoint:NSMakePoint(240.0, 325.0) operation:NSDragOperationMove];
    require(ends.size() == 1U, "AppKit's later end reports nothing");
    require(view.releases == 0, "AppKit's later end sends the closed window's view no release");
    observation.disconnect();
}

// A timer destroys the services during their drag: AppKit's later end
// reaches a live source (it holds itself until then) that reports to
// nothing.
void drag_survives_services_destroyed() {
    std::unique_ptr<gui_forms::HostServices> services =
        gui_forms::host::make_macos_host_services({"gui_forms.drag.hostile.services", false});
    const gui_forms::HostDragSourceRequest request{{gui_forms::DragTextData{"dragged"}},
                                                   gui_forms::DragEffect::copy};
    __weak id<NSDraggingSource> source = nil;
    @autoreleasepool {
        source = gui_forms::host::detail::macos_start_drag_for_testing(*services, request);
    }
    require(source != nil, "a drag starts");
    GuiFormsHostileDragHandler* const handler = [[GuiFormsHostileDragHandler alloc] init];
    handler.services = &services;
    handler.destroyServices = YES;
    [NSTimer scheduledTimerWithTimeInterval:0.02 target:handler selector:@selector(fire:) userInfo:nil repeats:NO];
    run_until_fired(handler);
    require(services == nullptr, "the timer destroyed the services");
    id<NSDraggingSource> const ending = source;
    require(ending != nil, "the source outlives its services until AppKit's end");
    [ending draggingSession:no_session() endedAtPoint:NSZeroPoint operation:NSDragOperationCopy];
}
} // namespace

int main() {
    @autoreleasepool {
        try {
            [NSApplication sharedApplication];
            NSWindow* const window = [[NSWindow alloc] initWithContentRect:NSMakeRect(200.0, 300.0, 320.0, 180.0)
                                                                 styleMask:NSWindowStyleMaskBorderless
                                                                   backing:NSBackingStoreBuffered
                                                                     defer:NO];
            // ARC owns the window: closing it must not release it as well.
            window.releasedWhenClosed = NO;
            GuiFormsReleaseRecorder* const view =
                [[GuiFormsReleaseRecorder alloc] initWithFrame:NSMakeRect(0.0, 0.0, 320.0, 180.0)];
            window.contentView = view;
            const NSRect frame = window.frame;
            const NSPoint end = NSMakePoint(frame.origin.x + 40.0, frame.origin.y + 25.0);

            gui_forms::host::detail::deliver_drag_release(view, NSEventTypeLeftMouseDragged, end);
            require(view.releases == 1 && view.lastType == NSEventTypeLeftMouseUp,
                    "a left-button drag gives the view a left release");
            require(near(view.lastLocation.x, 40.0) && near(view.lastLocation.y, 25.0),
                    "the release lands at the session's end, in the window's coordinates");

            gui_forms::host::detail::deliver_drag_release(view, NSEventTypeRightMouseDown, end);
            require(view.releases == 2 && view.lastType == NSEventTypeRightMouseUp,
                    "a right-button drag gives the view a right release");
            gui_forms::host::detail::deliver_drag_release(view, NSEventTypeOtherMouseDragged, end);
            require(view.releases == 3 && view.lastType == NSEventTypeOtherMouseUp,
                    "an other-button drag gives the view an other release");

            gui_forms::host::detail::deliver_drag_release(view, NSEventTypeKeyDown, end);
            require(view.releases == 3, "a start that was no button's sends nothing");
            require(gui_forms::host::detail::drag_release_event(nil, NSEventTypeLeftMouseDown, end) == nil,
                    "no window, no release");
            [window close];
            drag_survives_window_close();
            drag_survives_services_destroyed();
        } catch (const std::exception& error) {
            std::cerr << "gui_forms_macos_drag_release_tests: " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << "gui_forms_macos_drag_release_tests: all tests passed\n";
    return 0;
}
