// AppSettings — GUI 設定値の単一所有構造体 (旧 g_* 25 個を集約)。
// settings_globals.h / colors.h の g_<name> は g_settings.<name> 参照として
// 再公開 (後方互換)。 g_fmt_settings は engine 側に残し Snapshot だけが扱う。

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
    /* macOS のみ意味あり: グローバルメニューに加えて window 内のメニュー
     * バーにも項目を出すか。 false でも右側のツールボタンのために bar
     * widget は残る。 */
    bool gui_menubar_in_window;

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

    /* PrefsDialog Cancel 用スナップショット (g_settings + g_fmt_settings)。
     * 前方宣言だけ持って外で定義する (nested struct は自分自身を value
     * member に持てない)。 */
    struct Snapshot;
    static Snapshot capture();
    static void     restore(const Snapshot &snap);
};

extern AppSettings g_settings;

struct AppSettings::Snapshot {
    AppSettings    s;
    fmt_settings_t fmt;
};
