// platform_tray_mac.mm — macOS システムトレイ＋ホットキー実装
//
// トレイ: NSStatusItem + NSMenu
// ホットキー: Carbon RegisterEventHotKey

#include "platform_tray.h"
#include "i18n.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <cstring>
#include <cmath>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

// キー名テーブル・変換関数は platform_tray_common.cpp にある。

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

static NSMenu *s_tray_menu = nil;

@interface CalcyxTrayDelegate : NSObject
- (void)statusItemClicked:(id)sender;
- (void)openAction:(id)sender;
- (void)exitAction:(id)sender;
@end

@implementation CalcyxTrayDelegate
- (void)statusItemClicked:(id)sender {
    (void)sender;
    NSEvent *event = [NSApp currentEvent];
    if (event.type == NSEventTypeRightMouseUp ||
        (event.modifierFlags & NSEventModifierFlagControl)) {
        // 右クリック or Ctrl+クリック → メニューを表示
        [s_tray_menu popUpMenuPositioningItem:nil
                                  atLocation:NSMakePoint(0, s_status_item.button.bounds.size.height)
                                      inView:s_status_item.button];
    } else {
        // 左クリック → ウィンドウを開く
        if (s_callbacks.on_open) s_callbacks.on_open();
    }
}
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
        // dispatch_async で GCD メインキューに投げる。
        // Fl::awake() だと [NSApp hide:] 後にランループモードの問題で
        // コールバックが発火しないことがある。
        auto cb = s_callbacks.on_hotkey;
        dispatch_async(dispatch_get_main_queue(), ^{
            cb();
        });
    }
    return noErr;
}

static EventHandlerRef s_handler_ref = nullptr;

/* カラーアイコン → テンプレート用モノクロアウトライン生成。
 *
 * 入力 NSImage を 2x 解像度の RGBA に rasterize し、 luminance の
 * Sobel エッジ強度を新しい alpha とした NSImage を返す。 RGB は 0
 * (system tint で塗られるので不要)。 setTemplate:YES 済み。
 *
 * calcyx の icon は緑のベタ塗り計算機なので alpha だけだと
 * 黒い四角に潰れる。 luminance Sobel なら計算機の外周と display 枠
 * など色境界部分が線として残り、 mac の他のステータスバー項目
 * (細線アウトライン) と調和する。 */
static NSImage *make_template_from_color(NSImage *src, CGFloat pt_size) {
    if (!src) return nil;
    CGFloat px = pt_size * 2;     // 2x for retina + sharper sobel input
    NSInteger W = (NSInteger)px, H = (NSInteger)px;
    if (W < 4 || H < 4) return nil;

    NSBitmapImageRep *in = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:W pixelsHigh:H
                   bitsPerSample:8 samplesPerPixel:4
                        hasAlpha:YES isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:0 bitsPerPixel:32];
    if (!in) return nil;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:
        [NSGraphicsContext graphicsContextWithBitmapImageRep:in]];
    [src drawInRect:NSMakeRect(0, 0, W, H)
           fromRect:NSZeroRect
          operation:NSCompositingOperationCopy
           fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];

    unsigned char *sd = [in bitmapData];
    NSInteger sBpr = [in bytesPerRow];

    /* premultiplied luminance: 透明領域は 0、 不透明部分は luma 値。
     * Sobel が外周 (透明 ↔ body) でも内側 (body ↔ display 枠) でも
     * 反応するようにするための前処理。 */
    std::vector<int> lum((size_t)(W * H));
    for (NSInteger y = 0; y < H; y++) {
        for (NSInteger x = 0; x < W; x++) {
            unsigned char *p = sd + y * sBpr + x * 4;
            int a = p[3];
            int l = (299 * p[0] + 587 * p[1] + 114 * p[2]) / 1000;
            lum[(size_t)(y * W + x)] = l * a / 255;
        }
    }

    NSBitmapImageRep *out = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:W pixelsHigh:H
                   bitsPerSample:8 samplesPerPixel:4
                        hasAlpha:YES isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:0 bitsPerPixel:32];
    if (!out) return nil;
    unsigned char *od = [out bitmapData];
    NSInteger dBpr = [out bytesPerRow];

    static const int kx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    static const int ky[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    for (NSInteger y = 0; y < H; y++) {
        for (NSInteger x = 0; x < W; x++) {
            int gx = 0, gy = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    NSInteger nx = x + i, ny = y + j;
                    if (nx < 0) nx = 0; if (nx >= W) nx = W - 1;
                    if (ny < 0) ny = 0; if (ny >= H) ny = H - 1;
                    int v = lum[(size_t)(ny * W + nx)];
                    gx += kx[j+1][i+1] * v;
                    gy += ky[j+1][i+1] * v;
                }
            }
            int mag = (int)sqrt((double)(gx * gx + gy * gy));
            if (mag > 255) mag = 255;
            unsigned char *q = od + y * dBpr + x * 4;
            q[0] = q[1] = q[2] = 0;
            q[3] = (unsigned char)mag;
        }
    }

    [out setSize:NSMakeSize(pt_size, pt_size)];
    NSImage *tmpl = [[NSImage alloc] initWithSize:NSMakeSize(pt_size, pt_size)];
    [tmpl addRepresentation:out];
    [tmpl setTemplate:YES];
    return tmpl;
}

// ---- トレイ作成/破棄 ----

bool plat_tray_create(void *owner, const TrayCallbacks &cb) {
    (void)owner;  // macOS ではオーナーウィンドウ不要
    s_callbacks = cb;

    @autoreleasepool {
        // NSStatusItem 作成
        s_status_item = [[NSStatusBar systemStatusBar]
                         statusItemWithLength:NSVariableStatusItemLength];
        [s_status_item retain];

        // アイコン設定。 元の calcyx icon (緑のべた塗り計算機) を
        // luminance Sobel でアウトライン化してテンプレート画像にすると、
        // システムが light/dark mode で適切に tint してくれて、
        // 他のステータスバー項目と調和する。
        NSImage *icon = [NSApp applicationIconImage];
        NSImage *tmpl = make_template_from_color(icon, 18.0);
        if (tmpl) {
            s_status_item.button.image = tmpl;
        } else if (icon) {
            // フォールバック: 元のカラーアイコン (Sobel 失敗時のみ)
            NSImage *small = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];
            [small lockFocus];
            [icon drawInRect:NSMakeRect(0, 0, 18, 18)
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0];
            [small unlockFocus];
            s_status_item.button.image = small;
        }

        // メニュー (右クリック用に保持、直接 menu に設定しない)
        s_delegate = [[CalcyxTrayDelegate alloc] init];
        s_tray_menu = [[NSMenu alloc] init];
        NSMenuItem *openItem = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:_("Open")]
                   action:@selector(openAction:)
            keyEquivalent:@""];
        openItem.target = s_delegate;
        [s_tray_menu addItem:openItem];

        NSMenuItem *exitItem = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:_("Exit")]
                   action:@selector(exitAction:)
            keyEquivalent:@""];
        exitItem.target = s_delegate;
        [s_tray_menu addItem:exitItem];

        // menu を設定しない → 左クリックで action が呼ばれる
        s_status_item.button.action = @selector(statusItemClicked:);
        s_status_item.button.target = s_delegate;
        [s_status_item.button sendActionOn:(NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp)];
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
        if (s_tray_menu) {
            [s_tray_menu release];
            s_tray_menu = nil;
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

    fprintf(stderr, "DEBUG plat_window_toggle: visible=%d shown=%d tray_mode=%d\n",
            win->visible(), win->shown(), tray_mode);

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
