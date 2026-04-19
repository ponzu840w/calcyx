// platform_mac.mm — macOS 固有の機能 (Objective-C++)

#import <Cocoa/Cocoa.h>
#include <FL/Fl.H>
#include <FL/platform.H>

extern "C" void mac_set_window_level(Fl_Window *win, int topmost) {
    // FLTK ウィンドウから NSWindow を取得
    NSWindow *nswin = (NSWindow *)fl_xid(win);
    if (!nswin) return;
    [nswin setLevel: topmost ? NSFloatingWindowLevel : NSNormalWindowLevel];
}
