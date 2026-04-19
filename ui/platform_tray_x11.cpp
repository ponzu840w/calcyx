// platform_tray_x11.cpp — Linux X11 システムトレイ＋ホットキー実装
//
// トレイ: freedesktop System Tray Specification (XEMBED)
// ホットキー: XQueryKeymap ポーリング (Calctus LinuxX11HotKeyService 方式)

#include "platform_tray.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/platform.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Menu_Button.H>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cstring>
#include <cstdio>

// ---- キー名テーブル (stub と同じ共通実装) ----

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

// ---- FLTK キー → X11 KeySym 変換 ----

static KeySym flkey_to_keysym(int fl_key) {
    // ASCII 文字 (小文字 a-z, 0-9, space)
    if (fl_key >= 'a' && fl_key <= 'z') return (KeySym)fl_key;
    if (fl_key >= '0' && fl_key <= '9') return (KeySym)fl_key;
    if (fl_key == ' ')  return XK_space;
    if (fl_key == FL_Tab) return XK_Tab;
    if (fl_key == FL_Escape) return XK_Escape;
    // F1-F12
    if (fl_key >= FL_F + 1 && fl_key <= FL_F + 12)
        return XK_F1 + (fl_key - FL_F - 1);
    return 0;
}

// ---- トレイ状態 ----

static bool s_tray_active = false;
static TrayCallbacks s_callbacks;
static Window s_tray_win = 0;
static Atom s_tray_opcode = 0;
static GC s_tray_gc = 0;
static Fl_PNG_Image *s_icon_img = nullptr;

// ---- ホットキー状態 ----

static bool s_hotkey_registered = false;
static int  s_hotkey_mods = 0;
static KeyCode s_hotkey_keycode = 0;
static bool s_hotkey_was_pressed = false;  // エッジ検出用

// X11 修飾キーの KeyCode キャッシュ
static KeyCode s_kc_shift_l = 0, s_kc_shift_r = 0;
static KeyCode s_kc_ctrl_l = 0,  s_kc_ctrl_r = 0;
static KeyCode s_kc_alt_l = 0,   s_kc_alt_r = 0;
static KeyCode s_kc_super_l = 0, s_kc_super_r = 0;

// ---- X11 System Tray Protocol ----

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static Window find_tray_manager(Display *dpy, int screen) {
    char atom_name[64];
    snprintf(atom_name, sizeof(atom_name), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom tray_atom = XInternAtom(dpy, atom_name, False);
    return XGetSelectionOwner(dpy, tray_atom);
}

static void send_dock_request(Display *dpy, Window manager, Window icon_win) {
    s_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    XEvent ev = {};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = manager;
    ev.xclient.message_type = s_tray_opcode;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
    ev.xclient.data.l[2] = (long)icon_win;
    XSendEvent(dpy, manager, False, NoEventMask, &ev);
    XFlush(dpy);
}

// アイコン画像をロード (icon_128.png)
static void load_icon() {
    if (s_icon_img) return;
#ifdef CALCYX_ICON_PNG
    s_icon_img = new Fl_PNG_Image(CALCYX_ICON_PNG);
    if (s_icon_img->fail()) {
        delete s_icon_img;
        s_icon_img = nullptr;
    }
#endif
}

// トレイウィンドウの Expose イベントハンドラ (FLTK の外で X11 直接)
static int tray_x11_handler(void *event, void *) {
    (void)event;
    if (!s_tray_active || s_tray_win == 0) return 0;

    XEvent *xev = (XEvent *)fl_xevent;
    if (!xev) return 0;

    if (xev->type == Expose && xev->xexpose.window == s_tray_win) {
        Display *dpy = fl_display;
        if (s_icon_img && s_icon_img->w() > 0) {
            // Fl_PNG_Image のデータを X11 に描画
            XImage *xi = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                                       DefaultDepth(dpy, DefaultScreen(dpy)),
                                       ZPixmap, 0, nullptr, 24, 24, 32, 0);
            if (xi) {
                xi->data = (char *)malloc(xi->bytes_per_line * 24);
                // FLTK Image → XImage ピクセル変換
                const unsigned char *src = (const unsigned char *)s_icon_img->data()[0];
                int src_w = s_icon_img->w();
                int src_h = s_icon_img->h();
                int src_d = s_icon_img->d();
                for (int y = 0; y < 24; y++) {
                    for (int x = 0; x < 24; x++) {
                        // ソース座標 (最近傍補間)
                        int sx = x * src_w / 24;
                        int sy = y * src_h / 24;
                        const unsigned char *p = src + (sy * src_w + sx) * src_d;
                        unsigned long pixel;
                        if (src_d >= 3)
                            pixel = (p[0] << 16) | (p[1] << 8) | p[2];
                        else
                            pixel = (p[0] << 16) | (p[0] << 8) | p[0];
                        XPutPixel(xi, x, y, pixel);
                    }
                }
                XPutImage(dpy, s_tray_win, s_tray_gc, xi, 0, 0, 0, 0, 24, 24);
                free(xi->data);
                xi->data = nullptr;
                XDestroyImage(xi);
            }
        } else {
            // アイコンなし: 単色で塗りつぶし
            XSetForeground(dpy, s_tray_gc, BlackPixel(dpy, DefaultScreen(dpy)));
            XFillRectangle(dpy, s_tray_win, s_tray_gc, 0, 0, 24, 24);
        }
        return 1;
    }

    // トレイアイコンのクリック
    if (xev->type == ButtonPress && xev->xbutton.window == s_tray_win) {
        if (xev->xbutton.button == 1) {
            // 左クリック: ウィンドウを表示
            if (s_callbacks.on_open) s_callbacks.on_open();
        } else if (xev->xbutton.button == 3) {
            // 右クリック: コンテキストメニュー
            Fl_Menu_Button popup(Fl::event_x_root(), Fl::event_y_root(), 0, 0);
            popup.type(Fl_Menu_Button::POPUP3);
            popup.add("Open", 0, nullptr, (void *)1);
            popup.add("Exit", 0, nullptr, (void *)2);
            popup.textsize(12);
            const Fl_Menu_Item *picked = popup.popup();
            if (picked) {
                long idx = (long)picked->user_data();
                if (idx == 1 && s_callbacks.on_open) s_callbacks.on_open();
                if (idx == 2 && s_callbacks.on_exit) s_callbacks.on_exit();
            }
        }
        return 1;
    }

    return 0;
}

// ---- トレイ作成/破棄 ----

bool plat_tray_create(void *owner, const TrayCallbacks &cb) {
    (void)owner;  // X11 ではオーナーウィンドウ不要
    s_callbacks = cb;

    Display *dpy = fl_display;
    if (!dpy) return false;

    int screen = DefaultScreen(dpy);
    Window manager = find_tray_manager(dpy, screen);
    if (manager == None) {
        // トレイマネージャーがない (GNOME 3.38+ など)
        return false;
    }

    load_icon();

    // 24x24 のウィンドウを作成
    s_tray_win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                                      0, 0, 24, 24, 0,
                                      BlackPixel(dpy, screen),
                                      BlackPixel(dpy, screen));
    // イベントマスク
    XSelectInput(dpy, s_tray_win, ExposureMask | ButtonPressMask | StructureNotifyMask);

    // GC 作成
    s_tray_gc = XCreateGC(dpy, s_tray_win, 0, nullptr);

    // XEMBED info を設定
    Atom xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);
    long info[2] = { 0, 1 };  // version=0, flags=MAPPED
    XChangeProperty(dpy, s_tray_win, xembed_info, xembed_info, 32,
                    PropModeReplace, (unsigned char *)info, 2);

    // ドッキング要求
    send_dock_request(dpy, manager, s_tray_win);

    // X11 イベントハンドラを登録
    Fl::add_system_handler(tray_x11_handler, nullptr);

    s_tray_active = true;
    return true;
}

void plat_tray_destroy() {
    if (!s_tray_active) return;

    Fl::remove_system_handler(tray_x11_handler);

    Display *dpy = fl_display;
    if (dpy && s_tray_win) {
        if (s_tray_gc) { XFreeGC(dpy, s_tray_gc); s_tray_gc = 0; }
        XDestroyWindow(dpy, s_tray_win);
        s_tray_win = 0;
    }

    s_tray_active = false;
    s_callbacks = {};
}

bool plat_tray_is_active() {
    return s_tray_active;
}

// ---- ホットキー ----

bool plat_hotkey_register(int modifiers, int keycode) {
    Display *dpy = fl_display;
    if (!dpy) return false;

    KeySym ks = flkey_to_keysym(keycode);
    if (ks == 0) return false;

    s_hotkey_keycode = XKeysymToKeycode(dpy, ks);
    if (s_hotkey_keycode == 0) return false;

    s_hotkey_mods = modifiers;

    // 修飾キーの KeyCode をキャッシュ
    s_kc_shift_l = XKeysymToKeycode(dpy, XK_Shift_L);
    s_kc_shift_r = XKeysymToKeycode(dpy, XK_Shift_R);
    s_kc_ctrl_l  = XKeysymToKeycode(dpy, XK_Control_L);
    s_kc_ctrl_r  = XKeysymToKeycode(dpy, XK_Control_R);
    s_kc_alt_l   = XKeysymToKeycode(dpy, XK_Alt_L);
    s_kc_alt_r   = XKeysymToKeycode(dpy, XK_Alt_R);
    s_kc_super_l = XKeysymToKeycode(dpy, XK_Super_L);
    s_kc_super_r = XKeysymToKeycode(dpy, XK_Super_R);

    s_hotkey_was_pressed = false;
    s_hotkey_registered = true;
    return true;
}

void plat_hotkey_unregister() {
    s_hotkey_registered = false;
    s_hotkey_keycode = 0;
    s_hotkey_mods = 0;
}

// キーマップ中のキーが押されているか
static bool is_key_pressed(const char keymap[32], KeyCode kc) {
    if (kc == 0) return false;
    return (keymap[kc / 8] & (1 << (kc % 8))) != 0;
}

void plat_hotkey_poll() {
    if (!s_hotkey_registered) return;

    Display *dpy = fl_display;
    if (!dpy) return;

    char keymap[32];
    XQueryKeymap(dpy, keymap);

    // 主キー
    bool key_pressed = is_key_pressed(keymap, s_hotkey_keycode);

    // 修飾キーチェック
    if (key_pressed) {
        bool shift = is_key_pressed(keymap, s_kc_shift_l) || is_key_pressed(keymap, s_kc_shift_r);
        bool ctrl  = is_key_pressed(keymap, s_kc_ctrl_l)  || is_key_pressed(keymap, s_kc_ctrl_r);
        bool alt   = is_key_pressed(keymap, s_kc_alt_l)   || is_key_pressed(keymap, s_kc_alt_r);
        bool super = is_key_pressed(keymap, s_kc_super_l) || is_key_pressed(keymap, s_kc_super_r);

        bool want_shift = (s_hotkey_mods & PMOD_SHIFT) != 0;
        bool want_ctrl  = (s_hotkey_mods & PMOD_CTRL)  != 0;
        bool want_alt   = (s_hotkey_mods & PMOD_ALT)   != 0;
        bool want_win   = (s_hotkey_mods & PMOD_WIN)   != 0;

        if (shift != want_shift || ctrl != want_ctrl ||
            alt != want_alt || super != want_win) {
            key_pressed = false;
        }
    }

    // エッジ検出: 押下開始の瞬間だけ発火
    if (key_pressed && !s_hotkey_was_pressed) {
        if (s_callbacks.on_hotkey) s_callbacks.on_hotkey();
    }
    s_hotkey_was_pressed = key_pressed;
}

// ---- ウィンドウトグル ----

void plat_window_toggle(void *fl_window, bool tray_mode) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;

    Display *dpy = fl_display;
    if (!dpy) return;

    if (win->visible()) {
        // 現在表示中 → 隠す
        if (tray_mode) {
            win->hide();
        } else {
            win->iconize();
        }
    } else {
        // 非表示 → 表示して前面に
        win->show();
        Window xwin = fl_xid(win);
        if (xwin) {
            // _NET_ACTIVE_WINDOW で前面に
            Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
            XEvent ev = {};
            ev.xclient.type = ClientMessage;
            ev.xclient.window = xwin;
            ev.xclient.message_type = active;
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = 1;  // source indication: application
            ev.xclient.data.l[1] = CurrentTime;
            XSendEvent(dpy, DefaultRootWindow(dpy), False,
                       SubstructureNotifyMask | SubstructureRedirectMask, &ev);
            XFlush(dpy);
        }
    }
}
