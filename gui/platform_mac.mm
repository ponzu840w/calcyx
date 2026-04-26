// platform_mac.mm — macOS 固有の機能 (Objective-C++)

#import <Cocoa/Cocoa.h>
#include <FL/Fl.H>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <FL/platform.H>
#pragma clang diagnostic pop

extern "C" void mac_set_window_level(Fl_Window *win, int topmost) {
    // FLTK ウィンドウから NSWindow を取得
    NSWindow *nswin = (NSWindow *)fl_xid(win);
    if (!nswin) return;
    [nswin setLevel: topmost ? NSFloatingWindowLevel : NSNormalWindowLevel];
}

// 補完ポップアップウィンドウを非アクティブ化 (フォーカスを奪わない) し、
// メインウィンドウより手前に表示する。
extern "C" void mac_configure_popup(Fl_Window *popup, Fl_Window *main_win) {
    NSWindow *nsPopup = (NSWindow *)fl_xid(popup);
    NSWindow *nsMain  = (NSWindow *)fl_xid(main_win);
    if (!nsPopup || !nsMain) return;

    // ポップアップをメインウィンドウより1レベル上に配置
    [nsPopup setLevel: [nsMain level] + 1];

    // フォーカスをメインウィンドウに戻す
    [nsMain makeKeyWindow];
}
