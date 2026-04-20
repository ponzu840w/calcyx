// platform_tray_stub.cpp — トレイ/ホットキーのスタブ実装
// Web (Emscripten) やトレイ非対応プラットフォーム用。すべて no-op。

#include "platform_tray.h"

// キー名テーブル・変換関数は platform_tray_common.cpp にある。

// ---- スタブ実装 ----

bool plat_tray_create(void *, const TrayCallbacks &) { return false; }
void plat_tray_destroy() {}
bool plat_tray_is_active() { return false; }

bool plat_hotkey_register(int, int) { return false; }
void plat_hotkey_unregister() {}
void plat_hotkey_poll() {}

void plat_window_toggle(void *, bool) {}
void plat_window_raise(void *) {}
