// platform_tray_mac.mm — macOS システムトレイ＋ホットキー実装
//
// トレイ: NSStatusItem + NSMenu
// ホットキー: Carbon RegisterEventHotKey

#include "platform_tray.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <cstring>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

// ---- キー名テーブル (共通) ----

struct KeyEntry { const char *name; int fl_key; };

static const KeyEntry KEY_TABLE[] = {
    {"Space",  ' '},
    {"A", 'a'}, {"B", 'b'}, {"C", 'c'}, {"D", 'd'}, {"E", 'e'},
    {"F", 'f'}, {"G", 'g'}, {"H", 'h'}, {"I", 'i'}, {"J", 'j'},
    {"K", 'k'}, {"L", 'l'}, {"M", 'm'}, {"N", 'n'}, {"O", 'o'},
    {"P", 'p'}, {"Q", 'q'}, {"R", 'r'}, {"S", 's'}, {"T", 't'},
    {"U", 'u'}, {"V", 'v'}, {"W", 'w'}, {"X", 'x'}, {"Y", 'y'},
    {"Z", 'z'},
    {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
    {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
    {"F1",  FL_F+1},  {"F2",  FL_F+2},  {"F3",  FL_F+3},
    {"F4",  FL_F+4},  {"F5",  FL_F+5},  {"F6",  FL_F+6},
    {"F7",  FL_F+7},  {"F8",  FL_F+8},  {"F9",  FL_F+9},
    {"F10", FL_F+10}, {"F11", FL_F+11}, {"F12", FL_F+12},
    {"Tab", FL_Tab},
    {"Escape", FL_Escape},
};

static const int KEY_TABLE_SIZE = sizeof(KEY_TABLE) / sizeof(KEY_TABLE[0]);

static const char *s_key_name_list[sizeof(KEY_TABLE)/sizeof(KEY_TABLE[0]) + 1] = {};
static bool s_key_names_built = false;

static void build_key_names() {
    if (s_key_names_built) return;
    for (int i = 0; i < KEY_TABLE_SIZE; i++)
        s_key_name_list[i] = KEY_TABLE[i].name;
    s_key_name_list[KEY_TABLE_SIZE] = nullptr;
    s_key_names_built = true;
}

int plat_keyname_to_flkey(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < KEY_TABLE_SIZE; i++)
        if (strcmp(name, KEY_TABLE[i].name) == 0) return KEY_TABLE[i].fl_key;
    return 0;
}

const char *plat_flkey_to_keyname(int fl_key) {
    for (int i = 0; i < KEY_TABLE_SIZE; i++)
        if (KEY_TABLE[i].fl_key == fl_key) return KEY_TABLE[i].name;
    return "Space";
}

const char *const *plat_key_names() {
    build_key_names();
    return s_key_name_list;
}

int plat_key_names_count() {
    return KEY_TABLE_SIZE;
}

// ---- FLTK キー → macOS virtual keycode 変換 ----

static UInt32 flkey_to_mac_vk(int fl_key) {
    // macOS virtual keycodes
    if (fl_key >= 'a' && fl_key <= 'z') {
        // ANSI キーボードレイアウトの virtual keycode
        static const UInt32 az_vk[] = {
            kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
            kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
            kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
            kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
            kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
            kVK_ANSI_Z
        };
        return az_vk[fl_key - 'a'];
    }
    if (fl_key >= '0' && fl_key <= '9') {
        static const UInt32 num_vk[] = {
            kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
            kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9
        };
        return num_vk[fl_key - '0'];
    }
    if (fl_key == ' ')       return kVK_Space;
    if (fl_key == FL_Tab)    return kVK_Tab;
    if (fl_key == FL_Escape) return kVK_Escape;
    if (fl_key >= FL_F + 1 && fl_key <= FL_F + 12)
        return kVK_F1 + (fl_key - FL_F - 1);
    return 0xFFFF;
}

// ---- 状態 ----

static bool s_tray_active = false;
static TrayCallbacks s_callbacks;
static NSStatusItem *s_status_item = nil;
static EventHotKeyRef s_hotkey_ref = nullptr;
static bool s_hotkey_registered = false;

// ---- メニューデリゲート ----

@interface CalcyxTrayDelegate : NSObject
- (void)openAction:(id)sender;
- (void)exitAction:(id)sender;
@end

@implementation CalcyxTrayDelegate
- (void)openAction:(id)sender {
    (void)sender;
    if (s_callbacks.on_open) s_callbacks.on_open();
}
- (void)exitAction:(id)sender {
    (void)sender;
    if (s_callbacks.on_exit) s_callbacks.on_exit();
}
@end

static CalcyxTrayDelegate *s_delegate = nil;

// ---- Carbon ホットキーハンドラ ----

static OSStatus hotkey_handler(EventHandlerCallRef, EventRef, void *) {
    if (s_callbacks.on_hotkey) {
        // FLTK イベントループで安全に呼ぶ
        Fl::awake([](void *) {
            if (s_callbacks.on_hotkey) s_callbacks.on_hotkey();
        }, nullptr);
    }
    return noErr;
}

static EventHandlerRef s_handler_ref = nullptr;

// ---- トレイ作成/破棄 ----

bool plat_tray_create(void *owner, const TrayCallbacks &cb) {
    (void)owner;  // macOS ではオーナーウィンドウ不要
    s_callbacks = cb;

    @autoreleasepool {
        // NSStatusItem 作成
        s_status_item = [[NSStatusBar systemStatusBar]
                         statusItemWithLength:NSVariableStatusItemLength];
        [s_status_item retain];

        // アイコン設定
        NSImage *icon = [NSApp applicationIconImage];
        if (icon) {
            NSImage *small = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];
            [small lockFocus];
            [icon drawInRect:NSMakeRect(0, 0, 18, 18)
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0];
            [small unlockFocus];
            s_status_item.button.image = small;
        }

        // メニュー
        s_delegate = [[CalcyxTrayDelegate alloc] init];
        NSMenu *menu = [[NSMenu alloc] init];
        NSMenuItem *openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                          action:@selector(openAction:)
                                                   keyEquivalent:@""];
        openItem.target = s_delegate;
        [menu addItem:openItem];

        NSMenuItem *exitItem = [[NSMenuItem alloc] initWithTitle:@"Exit"
                                                          action:@selector(exitAction:)
                                                   keyEquivalent:@""];
        exitItem.target = s_delegate;
        [menu addItem:exitItem];

        s_status_item.menu = menu;

        // ボタンクリック (メニューなしの場合の左クリック対応)
        // NSMenu が設定されている場合、左クリックでメニューが表示される
    }

    s_tray_active = true;
    return true;
}

void plat_tray_destroy() {
    if (!s_tray_active) return;

    @autoreleasepool {
        if (s_status_item) {
            [[NSStatusBar systemStatusBar] removeStatusItem:s_status_item];
            [s_status_item release];
            s_status_item = nil;
        }
        if (s_delegate) {
            [s_delegate release];
            s_delegate = nil;
        }
    }

    s_tray_active = false;
    s_callbacks = {};
}

bool plat_tray_is_active() {
    return s_tray_active;
}

// ---- ホットキー ----

bool plat_hotkey_register(int modifiers, int keycode) {
    plat_hotkey_unregister();

    UInt32 mac_mods = 0;
    if (modifiers & PMOD_ALT)   mac_mods |= optionKey;
    if (modifiers & PMOD_CTRL)  mac_mods |= controlKey;
    if (modifiers & PMOD_SHIFT) mac_mods |= shiftKey;
    if (modifiers & PMOD_WIN)   mac_mods |= cmdKey;

    UInt32 vk = flkey_to_mac_vk(keycode);
    if (vk == 0xFFFF) return false;

    // Carbon イベントハンドラを登録
    if (!s_handler_ref) {
        EventTypeSpec spec = { kEventClassKeyboard, kEventHotKeyPressed };
        InstallApplicationEventHandler(&hotkey_handler, 1, &spec, nullptr, &s_handler_ref);
    }

    EventHotKeyID hkid = { 'CALC', 1 };
    OSStatus err = RegisterEventHotKey(vk, mac_mods, hkid,
                                        GetApplicationEventTarget(),
                                        0, &s_hotkey_ref);
    s_hotkey_registered = (err == noErr);
    return s_hotkey_registered;
}

void plat_hotkey_unregister() {
    if (s_hotkey_registered && s_hotkey_ref) {
        UnregisterEventHotKey(s_hotkey_ref);
        s_hotkey_ref = nullptr;
        s_hotkey_registered = false;
    }
}

// macOS ではポーリング不要 (Carbon イベントで受信)
void plat_hotkey_poll() {}

// ---- ウィンドウトグル ----

void plat_window_toggle(void *fl_window, bool tray_mode) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;

    @autoreleasepool {
        if (win->visible()) {
            if (tray_mode) {
                [NSApp hide:nil];
            } else {
                win->iconize();
            }
        } else {
            win->show();
            [NSApp activateIgnoringOtherApps:YES];
            // FLTK ウィンドウを前面に
            NSWindow *nswin = nil;
            for (NSWindow *w in [NSApp windows]) {
                if ([w isVisible]) { nswin = w; break; }
            }
            if (nswin) {
                [nswin makeKeyAndOrderFront:nil];
            }
        }
    }
}

void plat_window_raise(void *fl_window) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;
    @autoreleasepool {
        win->show();
        [NSApp activateIgnoringOtherApps:YES];
        for (NSWindow *w in [NSApp windows]) {
            if ([w isVisible]) { [w makeKeyAndOrderFront:nil]; break; }
        }
    }
}
