// settings_globals.h — ユーザー設定 (calcyx.conf)
// アプリ状態 (state.ini) とは分離されたユーザー編集可能な設定ファイル。
// ダイアログからも手動テキスト編集からも変更できる。
//
// デフォルト値は DEFAULT_* 定数で一元管理する。
// settings_init_defaults(), settings_load(), PrefsDialog の reset は
// すべてここの定数を参照する。

#pragma once

#include <FL/Fl.H>
#include <string>

extern "C" {
#include "types/val.h"
}

// ---- デフォルト値 (唯一の定義) ----
constexpr int  DEFAULT_FONT_ID              = FL_COURIER;
constexpr int  DEFAULT_FONT_SIZE            = 15;
constexpr bool DEFAULT_AUTO_COMPLETION      = true;
constexpr bool DEFAULT_AUTO_CLOSE_BRACKETS  = false;
constexpr bool DEFAULT_BS_DELETE_EMPTY_ROW  = true;
// 補完ポップアップを独立ウィンドウ (Fl_Menu_Window) で出すかどうか。
// true: メインウィンドウの境界を超えてはみ出せる (独立)
// false: メインウィンドウ内にクリップされる (埋め込み)
constexpr bool DEFAULT_POPUP_INDEPENDENT_NORMAL  = false;  // 通常モードの既定
constexpr bool DEFAULT_POPUP_INDEPENDENT_COMPACT = true;   // コンパクト時の既定
constexpr bool DEFAULT_SEP_THOUSANDS        = true;
constexpr bool DEFAULT_SEP_HEX             = true;
constexpr int  DEFAULT_MAX_ARRAY_LENGTH     = 256;
constexpr int  DEFAULT_MAX_STRING_LENGTH    = 256;
constexpr int  DEFAULT_MAX_CALL_DEPTH       = 64;
constexpr bool DEFAULT_SHOW_ROWLINES        = true;
constexpr bool DEFAULT_REMEMBER_POSITION    = true;
constexpr bool DEFAULT_START_TOPMOST        = false;
constexpr int  DEFAULT_FMT_DECIMAL_LEN      = 9;
constexpr bool DEFAULT_FMT_E_NOTATION       = true;
constexpr int  DEFAULT_FMT_E_POSITIVE_MIN   = 15;
constexpr int  DEFAULT_FMT_E_NEGATIVE_MAX   = -5;
constexpr bool DEFAULT_FMT_E_ALIGNMENT      = false;
constexpr int  DEFAULT_COLOR_PRESET         = 0;  // COLOR_PRESET_OTAKU_BLACK

// ---- グローバル変数 ----
// 実体は AppSettings (gui/AppSettings.h) に一元化されている.
// 既存コードのために `g_<name>` の名前で参照を再公開している.
//
// 新規コードは可能なら g_settings.<name> 直接参照を推奨.

extern std::string &g_language;

extern int &g_font_id;
extern int &g_font_size;

extern bool &g_input_auto_completion;
extern bool &g_input_auto_close_brackets;
/* 空行で BackSpace を押したときに行を削除して上に詰めるかどうか。
 * 既定 true (Calctus 互換)。 */
extern bool &g_input_bs_delete_empty_row;
extern bool &g_popup_independent_normal;
extern bool &g_popup_independent_compact;

extern bool &g_sep_thousands;
extern bool &g_sep_hex;

extern int &g_limit_max_array_length;
extern int &g_limit_max_string_length;
extern int &g_limit_max_call_depth;

extern bool &g_show_rowlines;

extern bool &g_remember_position;
extern bool &g_start_topmost;

// ---- システムトレイ・ホットキー ----
constexpr bool DEFAULT_TRAY_ICON       = false;
constexpr bool DEFAULT_HOTKEY_ENABLED  = false;
constexpr bool DEFAULT_HOTKEY_WIN      = false;
constexpr bool DEFAULT_HOTKEY_ALT      = true;
constexpr bool DEFAULT_HOTKEY_CTRL     = false;
constexpr bool DEFAULT_HOTKEY_SHIFT    = false;
constexpr int  DEFAULT_HOTKEY_KEYCODE  = ' ';  // Space

extern bool &g_tray_icon;
extern bool &g_hotkey_enabled;
extern bool &g_hotkey_win;
extern bool &g_hotkey_alt;
extern bool &g_hotkey_ctrl;
extern bool &g_hotkey_shift;
extern int  &g_hotkey_keycode;

// ---- 関数 ----
void settings_init_defaults();
void settings_load();
void settings_save();

// calcyx.conf のフルパスを返す
const char *settings_path();

// テスト専用: conf ファイルのパスを差し替える (本番コードからは呼ばない)。
// path に NULL を渡すとデフォルトに戻す (次回 ensure_path で決定される)。
void settings_set_path_for_test(const char *path);
