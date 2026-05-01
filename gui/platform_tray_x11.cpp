// platform_tray_x11.cpp — Linux X11 システムトレイ＋ホットキー実装
//
// トレイ: freedesktop System Tray Specification (XEMBED)
// ホットキー: XQueryKeymap ポーリング (Calctus LinuxX11HotKeyService 方式)

#include "platform_tray.h"
#include "i18n.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/platform.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

// キー名テーブル・変換関数は platform_tray_common.cpp にある。

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
static bool s_window_withdrawn = false;  // XWithdrawWindow で隠した状態
/* tray manager が ConfigureNotify で割り当てた現在のウィンドウサイズ。
 * パネル高さによって動的に変わる (Ubuntu MATE = 1px に縮められたり、
 * Ubuntu 26 = 40px に拡げられたりする) ので、 描画は常にこの値で行う。 */
static int s_tray_w = 24, s_tray_h = 24;
/* tray manager が _NET_SYSTEM_TRAY_VISUAL で 32-bit ARGB visual を
 * 公告していれば、 そちらで window を作ることで真の透過が出せる。
 * 24-bit (default) フォールバックでは parent backing_pixel と
 * alpha-blend する旧来の動作になる (compositor 非対応 panel 用)。 */
static Visual *s_tray_visual = nullptr;
static int     s_tray_depth = 24;
static Colormap s_tray_colormap = 0;

// ---- 右クリックメニュー ----

static int s_popup_x = 0, s_popup_y = 0;
static bool s_popup_active = false;

// ---- カスタムポップアップメニュー (Fl_Menu_Item::popup の座標問題を回避) ----

static Fl_Window *s_popup_win = nullptr;
static int s_popup_result = 0;  // 0=未選択、 1=Open, 2=Exit

// ホバー対応ボタン
class HoverButton : public Fl_Button {
public:
    HoverButton(int x, int y, int w, int h, const char *l = nullptr)
        : Fl_Button(x, y, w, h, l) {
        box(FL_FLAT_BOX);
        color(FL_BACKGROUND_COLOR);
        align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    }
    int handle(int event) override {
        switch (event) {
        case FL_ENTER:
            color(FL_SELECTION_COLOR);
            labelcolor(fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR));
            redraw();
            return 1;
        case FL_LEAVE:
            color(FL_BACKGROUND_COLOR);
            labelcolor(FL_FOREGROUND_COLOR);
            redraw();
            return 1;
        default:
            return Fl_Button::handle(event);
        }
    }
};

// メニュー外クリックで閉じるためのウィンドウクラス
class PopupMenuWindow : public Fl_Window {
public:
    PopupMenuWindow(int x, int y, int w, int h)
        : Fl_Window(x, y, w, h) {}
    int handle(int event) override {
        if (event == FL_UNFOCUS) {
            hide();
            return 1;
        }
        return Fl_Window::handle(event);
    }
};

static void popup_btn_cb(Fl_Widget *, void *data) {
    s_popup_result = (int)(long)data;
    if (s_popup_win) s_popup_win->hide();
}

static void show_tray_menu_cb(void *) {
    if (s_popup_active) return;  // 再入防止
    s_popup_active = true;
    s_popup_result = 0;

    const int btn_w = 80, btn_h = 24, pad = 1;
    const int win_w = btn_w + pad * 2;
    const int win_h = btn_h * 2 + pad;

    int mx = s_popup_x;
    int my = s_popup_y;

    if (!s_popup_win) {
        s_popup_win = new PopupMenuWindow(mx, my, win_w, win_h);
        s_popup_win->border(0);
        s_popup_win->set_override();

        /* ラベルは i18n 翻訳 + 左側にスペース 1 個分のパディング。
         * copy_label() で内部コピーさせる (一時 std::string が短命なため)。 */
        auto *b1 = new HoverButton(pad, 0, btn_w, btn_h);
        b1->copy_label((std::string(" ") + _("Open")).c_str());
        b1->callback(popup_btn_cb, (void *)1);

        auto *b2 = new HoverButton(pad, btn_h, btn_w, btn_h);
        b2->copy_label((std::string(" ") + _("Exit")).c_str());
        b2->callback(popup_btn_cb, (void *)2);

        s_popup_win->end();
    }

    s_popup_win->position(mx, my);
    s_popup_win->show();

    // ポップアップが閉じるまでモーダルループ
    while (s_popup_win->visible()) {
        Fl::wait();
    }

    s_popup_active = false;

    if (s_popup_result == 1 && s_callbacks.on_open)
        Fl::add_timeout(0.0, [](void *) { if (s_callbacks.on_open) s_callbacks.on_open(); }, nullptr);
    if (s_popup_result == 2 && s_callbacks.on_exit)
        Fl::add_timeout(0.0, [](void *) { if (s_callbacks.on_exit) s_callbacks.on_exit(); }, nullptr);
}

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

    /* tray manager がウィンドウサイズを変更したら追従する。 描画は次の
     * Expose で実行されるが、 一部の panel は resize 後に Expose を
     * 発行しないので XClearArea(..., True) で再露光要求を出す。 */
    if (xev->type == ConfigureNotify && xev->xconfigure.window == s_tray_win) {
        s_tray_w = xev->xconfigure.width;
        s_tray_h = xev->xconfigure.height;
        if (s_tray_w > 0 && s_tray_h > 0) {
            XClearArea(fl_display, s_tray_win, 0, 0, 0, 0, True);
        }
        return 1;
    }

    if (xev->type == Expose && xev->xexpose.window == s_tray_win) {
        Display *dpy = fl_display;
        int W = s_tray_w, H = s_tray_h;
        if (W <= 0 || H <= 0) return 1;

        bool argb = (s_tray_depth == 32);
        /* ARGB 経路: 真の透過 (alpha チャンネル保持)。
         * 24-bit fallback: 親 panel の backing_pixel と alpha-blend。 */
        unsigned char bg_r = 0xD0, bg_g = 0xD0, bg_b = 0xD0;
        if (!argb) {
            XWindowAttributes wa;
            Window parent = 0, root = 0, *children = nullptr;
            unsigned int nchildren = 0;
            if (XQueryTree(dpy, s_tray_win, &root, &parent, &children, &nchildren)) {
                if (children) XFree(children);
                if (parent && XGetWindowAttributes(dpy, parent, &wa) && wa.backing_pixel) {
                    bg_r = (wa.backing_pixel >> 16) & 0xFF;
                    bg_g = (wa.backing_pixel >> 8) & 0xFF;
                    bg_b = wa.backing_pixel & 0xFF;
                }
            }
            // 余白を panel 風グレーで先に塗っておく
            XSetForeground(dpy, s_tray_gc,
                           ((unsigned long)bg_r << 16) |
                           ((unsigned long)bg_g << 8) |
                           bg_b);
            XFillRectangle(dpy, s_tray_win, s_tray_gc, 0, 0, W, H);
        } else {
            // ARGB: 透過のままクリア
            XClearWindow(dpy, s_tray_win);
        }

        if (s_icon_img && s_icon_img->w() > 0) {
            XImage *xi = XCreateImage(dpy, s_tray_visual, s_tray_depth,
                                       ZPixmap, 0, nullptr, W, H, 32, 0);
            if (xi) {
                xi->data = (char *)malloc(xi->bytes_per_line * H);
                const unsigned char *src = (const unsigned char *)s_icon_img->data()[0];
                int src_w = s_icon_img->w();
                int src_h = s_icon_img->h();
                int src_d = s_icon_img->d();
                /* アイコンを正方形に保ったまま中央に置く (パネルが
                 * 横長/縦長の場合に歪まないように)。 */
                int side = W < H ? W : H;
                int ox = (W - side) / 2;
                int oy = (H - side) / 2;
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        unsigned char r, g, b, a = 0;
                        int lx = x - ox, ly = y - oy;
                        if (lx >= 0 && lx < side && ly >= 0 && ly < side) {
                            int sx = lx * src_w / side;
                            int sy = ly * src_h / side;
                            const unsigned char *p = src + (sy * src_w + sx) * src_d;
                            if (src_d >= 3) { r = p[0]; g = p[1]; b = p[2]; }
                            else            { r = g = b = p[0]; }
                            a = (src_d == 4) ? p[3] : 0xFF;
                        } else {
                            r = bg_r; g = bg_g; b = bg_b; a = 0;
                        }
                        unsigned long px;
                        if (argb) {
                            // 真の ARGB: alpha 保持 (premultiplied)
                            unsigned char pr = (r * a) / 255;
                            unsigned char pg = (g * a) / 255;
                            unsigned char pb = (b * a) / 255;
                            px = ((unsigned long)a  << 24)
                               | ((unsigned long)pr << 16)
                               | ((unsigned long)pg << 8)
                               |  (unsigned long)pb;
                        } else if (lx >= 0 && lx < side && ly >= 0 && ly < side) {
                            // 24-bit: backing_pixel と alpha-blend
                            unsigned char br = (r * a + bg_r * (255 - a)) / 255;
                            unsigned char bg2= (g * a + bg_g * (255 - a)) / 255;
                            unsigned char bb = (b * a + bg_b * (255 - a)) / 255;
                            px = ((unsigned long)br  << 16)
                               | ((unsigned long)bg2 << 8)
                               |  (unsigned long)bb;
                        } else {
                            px = ((unsigned long)bg_r << 16)
                               | ((unsigned long)bg_g << 8)
                               |  (unsigned long)bg_b;
                        }
                        XPutPixel(xi, x, y, px);
                    }
                }
                XPutImage(dpy, s_tray_win, s_tray_gc, xi, 0, 0, 0, 0, W, H);
                free(xi->data);
                xi->data = nullptr;
                XDestroyImage(xi);
            }
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
            // X11 ハンドラ内から FLTK ポップアップを直接呼ぶとクラッシュするため
            // 座標を保存して次のイベントループで表示する
            s_popup_x = xev->xbutton.x_root;
            s_popup_y = xev->xbutton.y_root;
            Fl::add_timeout(0.0, show_tray_menu_cb, nullptr);
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

    /* tray manager が _NET_SYSTEM_TRAY_VISUAL を公告していれば、 その visual
     * (典型的には 32-bit ARGB) で window を作る。 これで合成型 panel で
     * 真の透過が出せる。 公告無しなら DefaultVisual (24-bit) で fallback。 */
    s_tray_visual   = DefaultVisual(dpy, screen);
    s_tray_depth    = DefaultDepth(dpy, screen);
    s_tray_colormap = DefaultColormap(dpy, screen);
    {
        Atom xa_visual = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);
        Atom actual_type = 0;
        int  actual_format = 0;
        unsigned long nitems = 0, bytes_after = 0;
        unsigned char *prop = nullptr;
        if (XGetWindowProperty(dpy, manager, xa_visual, 0, 1, False,
                               XA_VISUALID, &actual_type, &actual_format,
                               &nitems, &bytes_after, &prop) == Success
            && prop && nitems == 1) {
            VisualID vid = *(VisualID *)prop;
            XVisualInfo tmpl; tmpl.visualid = vid;
            int n = 0;
            XVisualInfo *vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);
            if (vi && n > 0) {
                s_tray_visual   = vi[0].visual;
                s_tray_depth    = vi[0].depth;
                s_tray_colormap = XCreateColormap(dpy, DefaultRootWindow(dpy),
                                                  s_tray_visual, AllocNone);
                XFree(vi);
            }
        }
        if (prop) XFree(prop);
    }

    // 24x24 のウィンドウを作成 (実サイズは tray manager が ConfigureNotify で
    // 通知してくる。 描画は s_tray_w/s_tray_h ベースで行う)
    s_tray_w = 24;
    s_tray_h = 24;
    XSetWindowAttributes swa = {};
    swa.colormap         = s_tray_colormap;
    swa.background_pixel = 0;     // ARGB 経路では完全透過
    swa.border_pixel     = 0;
    s_tray_win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                                0, 0, s_tray_w, s_tray_h, 0,
                                s_tray_depth, InputOutput,
                                s_tray_visual,
                                CWColormap | CWBackPixel | CWBorderPixel, &swa);
    /* WM_NORMAL_HINTS でサイズ範囲を伝える。 一部の tray (Ubuntu MATE 24)
     * はヒント無しだと最小サイズ (1px) に縮めてくる。 */
    {
        XSizeHints *hints = XAllocSizeHints();
        if (hints) {
            hints->flags = PMinSize | PBaseSize | PSize;
            hints->min_width  = 16;
            hints->min_height = 16;
            hints->base_width  = 24;
            hints->base_height = 24;
            hints->width  = 24;
            hints->height = 24;
            XSetWMNormalHints(dpy, s_tray_win, hints);
            XFree(hints);
        }
    }
    // イベントマスク (ConfigureNotify で resize 追従)
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

    // ポップアップメニュー破棄
    if (s_popup_win) {
        delete s_popup_win;
        s_popup_win = nullptr;
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

// ウィンドウを前面に表示してフォーカスを与える
static void raise_and_focus(Fl_Window *win) {
    Display *dpy = fl_display;
    if (!dpy || !win) return;

    if (!win->shown()) win->show();

    Window xwin = fl_xid(win);
    if (!xwin) return;

    if (s_window_withdrawn) {
        XMapRaised(dpy, xwin);
        s_window_withdrawn = false;
    }

    // _NET_ACTIVE_WINDOW で WM にフォーカス要求
    Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    XEvent ev = {};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = xwin;
    ev.xclient.message_type = active;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;  // source: application
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(dpy);
}

void plat_window_toggle(void *fl_window, bool tray_mode) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;

    Display *dpy = fl_display;
    if (!dpy) return;

    if (s_window_withdrawn) {
        // Withdraw 状態 → 表示
        raise_and_focus(win);
    } else if (win->shown() && !tray_mode) {
        win->iconize();
    } else if (win->shown()) {
        // 表示中 → XWithdrawWindow で隠す (X ウィンドウは保持)
        Window xwin = fl_xid(win);
        if (xwin) {
            XWithdrawWindow(dpy, xwin, DefaultScreen(dpy));
            s_window_withdrawn = true;
            XFlush(dpy);
        }
    } else {
        raise_and_focus(win);
    }
}

void plat_window_raise(void *fl_window) {
    raise_and_focus(static_cast<Fl_Window *>(fl_window));
}
