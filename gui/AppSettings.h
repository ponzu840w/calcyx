// AppSettings — ユーザー編集可能な GUI 設定値の単一所有構造体.
//
// 元々 settings_globals.cpp に散在していた 22 個の g_* グローバル変数と
// colors.cpp の g_colors / g_user_colors / g_color_preset を 1 個の
// `g_settings` 構造体に集約する.
//
// settings_globals.h / colors.h で定義していた個別の `g_<name>` は
// `g_settings.<name>` への参照として再公開する (後方互換). 段階的に
// 直接アクセスへ移行する.
//
// 注: g_fmt_settings (engine/types/val.c) は engine から参照されるため
// AppSettings には含めず、Snapshot だけが capture 対象として面倒を見る.

#pragma once

#include "colors.h"
#include <string>

extern "C" {
#include "types/val.h"
}

struct AppSettings {
    /* 言語 */
    std::string language;

    /* フォント */
    int font_id;
    int font_size;

    /* 入力 */
    bool input_auto_completion;
    bool input_auto_close_brackets;
    bool input_bs_delete_empty_row;
    bool popup_independent_normal;
    bool popup_independent_compact;

    /* 桁区切り */
    bool sep_thousands;
    bool sep_hex;

    /* 計算リミット */
    int limit_max_array_length;
    int limit_max_string_length;
    int limit_max_call_depth;

    /* 表示 */
    bool show_rowlines;

    /* ウィンドウ */
    bool remember_position;
    bool start_topmost;

    /* システムトレイ・ホットキー */
    bool tray_icon;
    bool hotkey_enabled;
    bool hotkey_win;
    bool hotkey_alt;
    bool hotkey_ctrl;
    bool hotkey_shift;
    int  hotkey_keycode;

    /* 色 (preset / current / user-defined backup) */
    int          color_preset;
    CalcyxColors colors;
    CalcyxColors user_colors;

    /* PrefsDialog Cancel 等で使う完全スナップショット.
     * g_settings + 別所有の g_fmt_settings をまとめて保存する.
     * (前方宣言だけ持って外で定義する: nested struct の中で
     * 自分自身を value member として持てないため) */
    struct Snapshot;
    static Snapshot capture();
    static void     restore(const Snapshot &snap);
};

extern AppSettings g_settings;

struct AppSettings::Snapshot {
    AppSettings    s;
    fmt_settings_t fmt;
};
