// settings_globals.h — ユーザー設定 (calcyx.conf)
// アプリ状態 (state.ini) とは分離されたユーザー編集可能な設定ファイル。
// ダイアログからも手動テキスト編集からも変更できる。

#pragma once

extern "C" {
#include "types/val.h"
}

// ---- フォント ----
extern int g_font_id;    // default FL_COURIER
extern int g_font_size;  // default 13

// ---- 入力 ----
extern bool g_input_auto_completion;
extern bool g_input_auto_close_brackets;

// ---- 桁区切り ----
extern bool g_sep_thousands;
extern bool g_sep_hex;

// ---- 計算リミット ----
extern int g_limit_max_array_length;
extern int g_limit_max_string_length;
extern int g_limit_max_call_depth;

// ---- 表示 ----
extern bool g_show_rowlines;

// ---- ウィンドウ ----
extern bool g_remember_position;

void settings_init_defaults();
void settings_load();
void settings_save();

// calcyx.conf のフルパスを返す
const char *settings_path();
