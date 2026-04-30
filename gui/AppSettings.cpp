#include "AppSettings.h"
#include "settings_globals.h"

/* 全 GUI 設定の単一所有 instance. デフォルト値は settings_globals.h の
 * DEFAULT_* 定数。 色は colors_init_preset(otaku-black) で起動時に上書き。 */
AppSettings g_settings = {
    /* language */                   "auto",
    /* font_id */                    DEFAULT_FONT_ID,
    /* font_size */                  DEFAULT_FONT_SIZE,
    /* input_auto_completion */      DEFAULT_AUTO_COMPLETION,
    /* input_auto_close_brackets */  DEFAULT_AUTO_CLOSE_BRACKETS,
    /* input_bs_delete_empty_row */  DEFAULT_BS_DELETE_EMPTY_ROW,
    /* popup_independent_normal */   DEFAULT_POPUP_INDEPENDENT_NORMAL,
    /* popup_independent_compact */  DEFAULT_POPUP_INDEPENDENT_COMPACT,
    /* sep_thousands */              DEFAULT_SEP_THOUSANDS,
    /* sep_hex */                    DEFAULT_SEP_HEX,
    /* limit_max_array_length */     DEFAULT_MAX_ARRAY_LENGTH,
    /* limit_max_string_length */    DEFAULT_MAX_STRING_LENGTH,
    /* limit_max_call_depth */       DEFAULT_MAX_CALL_DEPTH,
    /* show_rowlines */              DEFAULT_SHOW_ROWLINES,
    /* gui_menubar_in_window */      DEFAULT_GUI_MENUBAR_IN_WINDOW,
    /* remember_position */          DEFAULT_REMEMBER_POSITION,
    /* start_topmost */              DEFAULT_START_TOPMOST,
    /* tray_icon */                  DEFAULT_TRAY_ICON,
    /* hotkey_enabled */             DEFAULT_HOTKEY_ENABLED,
    /* hotkey_win */                 DEFAULT_HOTKEY_WIN,
    /* hotkey_alt */                 DEFAULT_HOTKEY_ALT,
    /* hotkey_ctrl */                DEFAULT_HOTKEY_CTRL,
    /* hotkey_shift */               DEFAULT_HOTKEY_SHIFT,
    /* hotkey_keycode */             DEFAULT_HOTKEY_KEYCODE,
    /* color_preset */               DEFAULT_COLOR_PRESET,
    /* colors */                     {},
    /* user_colors */                {},
};

AppSettings::Snapshot AppSettings::capture() {
    return { g_settings, g_fmt_settings };
}

void AppSettings::restore(const Snapshot &snap) {
    g_settings     = snap.s;
    g_fmt_settings = snap.fmt;
}
