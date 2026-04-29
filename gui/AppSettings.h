// AppSettings — ユーザー設定値のスナップショット型と一括 capture/restore.
//
// 個別の g_* グローバル (settings_globals.h, colors.h, types/val.h の
// g_fmt_settings) を 1 個の Snapshot 構造体にまとめ、PrefsDialog の
// Cancel ボタン等で 1 行の copy で復元できるようにする。
//
// 既存の g_* グローバル自体はそのままで、capture()/restore() は単に
// それらを読み書きするヘルパー。将来 g_* を AppSettings 構造体に
// 内包する形に統合してもこの API は維持できる。

#pragma once

#include "colors.h"
#include <string>

extern "C" {
#include "types/val.h"
}

namespace AppSettings {

struct Snapshot {
    std::string language;

    int  font_id;
    int  font_size;

    bool input_auto_completion;
    bool input_auto_close_brackets;
    bool input_bs_delete_empty_row;
    bool popup_independent_normal;
    bool popup_independent_compact;

    bool sep_thousands;
    bool sep_hex;

    int  limit_max_array_length;
    int  limit_max_string_length;
    int  limit_max_call_depth;

    bool show_rowlines;
    bool remember_position;
    bool start_topmost;

    bool tray_icon;
    bool hotkey_enabled;
    bool hotkey_win;
    bool hotkey_alt;
    bool hotkey_ctrl;
    bool hotkey_shift;
    int  hotkey_keycode;

    fmt_settings_t fmt;

    CalcyxColors colors;
    CalcyxColors user_colors;
    int          color_preset;
};

/* g_* グローバル群を読み出して Snapshot を返す. */
Snapshot capture();

/* Snapshot の値を g_* グローバル群に書き戻す. */
void restore(const Snapshot &s);

}  // namespace AppSettings
