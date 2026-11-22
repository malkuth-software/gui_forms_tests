// The button release a drag from the program gives back to its window when
// AppKit's dragging session ends (appkit_drag_release.hpp): the session
// consumes the mouse-up, so the host rebuilds it for the button the drag
// started with, at the session's end point, and sends it to the view. A
// real session needs a held button, so the drag itself is a hand check (the
// integration queue entry for host-drag-and-drop); this drives the release
// the host sends at its end.

#import <AppKit/AppKit.h>
#include "src/surface/macos/services/appkit_drag_release.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

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

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double a, double b) {
    return std::fabs(a - b) < 0.5;
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
        } catch (const std::exception& error) {
            std::cerr << "gui_forms_macos_drag_release_tests: " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << "gui_forms_macos_drag_release_tests: all tests passed\n";
    return 0;
}
