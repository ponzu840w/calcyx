// platform_tray_win.cpp — Windows システムトレイ＋ホットキー実装
//
// トレイ: Shell_NotifyIconW
// ホットキー: RegisterHotKey + WM_HOTKEY
// メッセージ処理: SetWindowSubclass (comctl32)

#include "platform_tray.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/platform.H>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <cstring>
#include <cstdio>

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

// ---- FLTK キー → Windows VK 変換 ----

static UINT flkey_to_vk(int fl_key) {
    if (fl_key >= 'a' && fl_key <= 'z') return 'A' + (fl_key - 'a');
    if (fl_key >= '0' && fl_key <= '9') return (UINT)fl_key;
    if (fl_key == ' ')  return VK_SPACE;
    if (fl_key == FL_Tab) return VK_TAB;
    if (fl_key == FL_Escape) return VK_ESCAPE;
    if (fl_key >= FL_F + 1 && fl_key <= FL_F + 12)
        return VK_F1 + (fl_key - FL_F - 1);
    return 0;
}

// ---- 状態 ----

#define WM_TRAY_ICON (WM_USER + 1)
#define HOTKEY_ID    1

static bool s_tray_active = false;
static TrayCallbacks s_callbacks;
static NOTIFYICONDATAW s_nid = {};
static HWND s_hwnd = nullptr;
static bool s_subclassed = false;
static bool s_hotkey_registered = false;

// ---- サブクラスプロシージャ ----

static LRESULT CALLBACK tray_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR id, DWORD_PTR data) {
    (void)id; (void)data;

    if (msg == WM_TRAY_ICON) {
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP:
            if (s_callbacks.on_open) s_callbacks.on_open();
            return 0;
        case WM_RBUTTONUP: {
            // コンテキストメニュー
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Open");
            AppendMenuW(menu, MF_STRING, 2, L"Exit");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                       pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (cmd == 1 && s_callbacks.on_open) s_callbacks.on_open();
            if (cmd == 2 && s_callbacks.on_exit) s_callbacks.on_exit();
            return 0;
        }
        }
    }

    if (msg == WM_HOTKEY && wp == HOTKEY_ID) {
        if (s_callbacks.on_hotkey) s_callbacks.on_hotkey();
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ---- トレイ作成/破棄 ----

bool plat_tray_create(void *owner, const TrayCallbacks &cb) {
    s_callbacks = cb;

    // オーナーウィンドウの HWND を取得
    auto *flw = static_cast<Fl_Window *>(owner);
    if (!flw || !flw->shown()) return false;
    s_hwnd = fl_xid(flw);
    if (!s_hwnd) return false;

    // サブクラス化
    if (!s_subclassed) {
        SetWindowSubclass(s_hwnd, tray_subclass_proc, 1, 0);
        s_subclassed = true;
    }

    // トレイアイコン作成
    memset(&s_nid, 0, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = s_hwnd;
    s_nid.uID = 1;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_TRAY_ICON;
    s_nid.hIcon = (HICON)SendMessage(s_hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!s_nid.hIcon)
        s_nid.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(101));
    if (!s_nid.hIcon)
        s_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy(s_nid.szTip, L"calcyx");
    Shell_NotifyIconW(NIM_ADD, &s_nid);

    s_tray_active = true;
    return true;
}

void plat_tray_destroy() {
    if (!s_tray_active) return;

    Shell_NotifyIconW(NIM_DELETE, &s_nid);

    if (s_subclassed && s_hwnd) {
        RemoveWindowSubclass(s_hwnd, tray_subclass_proc, 1);
        s_subclassed = false;
    }

    s_tray_active = false;
    s_callbacks = {};
}

bool plat_tray_is_active() {
    if (!s_tray_active) return false;
    // Explorer 再起動等でトレイが消えた場合を検出:
    // NIM_MODIFY は対象アイコンが存在しなければ失敗する
    if (!Shell_NotifyIconW(NIM_MODIFY, &s_nid)) {
        s_tray_active = false;
        return false;
    }
    return true;
}

// ---- ホットキー ----

bool plat_hotkey_register(int modifiers, int keycode) {
    // s_hwnd は plat_tray_create で設定済みであること
    if (!s_hwnd) return false;

    // サブクラス化 (トレイなしでもホットキーだけ使う場合)
    if (!s_subclassed) {
        SetWindowSubclass(s_hwnd, tray_subclass_proc, 1, 0);
        s_subclassed = true;
    }

    UINT win_mods = 0;
    if (modifiers & PMOD_ALT)   win_mods |= MOD_ALT;
    if (modifiers & PMOD_CTRL)  win_mods |= MOD_CONTROL;
    if (modifiers & PMOD_SHIFT) win_mods |= MOD_SHIFT;
    if (modifiers & PMOD_WIN)   win_mods |= MOD_WIN;

    UINT vk = flkey_to_vk(keycode);
    if (vk == 0) return false;

    if (s_hotkey_registered)
        UnregisterHotKey(s_hwnd, HOTKEY_ID);

    s_hotkey_registered = RegisterHotKey(s_hwnd, HOTKEY_ID, win_mods, vk) != 0;
    return s_hotkey_registered;
}

void plat_hotkey_unregister() {
    if (s_hotkey_registered && s_hwnd) {
        UnregisterHotKey(s_hwnd, HOTKEY_ID);
        s_hotkey_registered = false;
    }
}

// Windows ではポーリング不要 (WM_HOTKEY で受信)
void plat_hotkey_poll() {}

// ---- ウィンドウトグル ----

void plat_window_toggle(void *fl_window, bool tray_mode) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;

    HWND hwnd = fl_xid(win);
    if (!hwnd) return;

    if (win->visible() && GetForegroundWindow() == hwnd) {
        // フォアグラウンドで表示中 → 隠す
        if (tray_mode) {
            ShowWindow(hwnd, SW_HIDE);
        } else {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
    } else {
        // 非表示またはバックグラウンド → 表示して前面に
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
}

void plat_window_raise(void *fl_window) {
    auto *win = static_cast<Fl_Window *>(fl_window);
    if (!win) return;
    HWND hwnd = fl_xid(win);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}
