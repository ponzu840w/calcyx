// platform_tray.h — システムトレイ＋グローバルホットキー抽象レイヤー
//
// プラットフォーム別実装:
//   platform_tray_win.cpp  (Windows: Shell_NotifyIcon, RegisterHotKey)
//   platform_tray_x11.cpp  (Linux:   X11 System Tray, XQueryKeymap polling)
//   platform_tray_mac.mm   (macOS:   NSStatusItem, Carbon hotkey)
//   platform_tray_stub.cpp (Web/その他: すべて no-op)

#pragma once

#include <functional>

// ---- 修飾キーフラグ ----
enum PlatModifier : int {
    PMOD_NONE  = 0,
    PMOD_ALT   = 1,
    PMOD_CTRL  = 2,
    PMOD_SHIFT = 4,
    PMOD_WIN   = 8,
};

// ---- コールバック ----
struct TrayCallbacks {
    std::function<void()> on_open;    // トレイクリック / "Open" メニュー
    std::function<void()> on_exit;    // "Exit" メニュー
    std::function<void()> on_hotkey;  // グローバルホットキー発火
};

// ---- トレイアイコン ----
// owner: トレイメッセージを受け取る Fl_Window* (MainWindow)
// Fl::first_window() は PrefsDialog 等に化ける場合があるので明示指定が必要
bool  plat_tray_create(void *owner, const TrayCallbacks &cb);
void  plat_tray_destroy();
bool  plat_tray_is_active();

// ---- グローバルホットキー ----
bool  plat_hotkey_register(int modifiers, int keycode);
void  plat_hotkey_unregister();

// ポーリング (Linux XQueryKeymap 用。他プラットフォームは no-op)
void  plat_hotkey_poll();

// ---- ウィンドウ表示トグル ----
// fl_window: Fl_Window* を void* で受け取る
// tray_mode: true ならトレイに隠す、false ならタスクバーに最小化
void  plat_window_toggle(void *fl_window, bool tray_mode);

// ---- キー名 ↔ FLTK キー定数 変換 ----
int         plat_keyname_to_flkey(const char *name);
const char *plat_flkey_to_keyname(int fl_key);

// 選択肢として使えるキー名リスト (nullptr 終端)
const char *const *plat_key_names();
int plat_key_names_count();
