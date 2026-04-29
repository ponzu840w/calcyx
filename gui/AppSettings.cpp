#include "AppSettings.h"
#include "settings_globals.h"

namespace AppSettings {

Snapshot capture() {
    Snapshot s;
    s.language                   = g_language;
    s.font_id                    = g_font_id;
    s.font_size                  = g_font_size;
    s.input_auto_completion      = g_input_auto_completion;
    s.input_auto_close_brackets  = g_input_auto_close_brackets;
    s.input_bs_delete_empty_row  = g_input_bs_delete_empty_row;
    s.popup_independent_normal   = g_popup_independent_normal;
    s.popup_independent_compact  = g_popup_independent_compact;
    s.sep_thousands              = g_sep_thousands;
    s.sep_hex                    = g_sep_hex;
    s.limit_max_array_length     = g_limit_max_array_length;
    s.limit_max_string_length    = g_limit_max_string_length;
    s.limit_max_call_depth       = g_limit_max_call_depth;
    s.show_rowlines              = g_show_rowlines;
    s.remember_position          = g_remember_position;
    s.start_topmost              = g_start_topmost;
    s.tray_icon                  = g_tray_icon;
    s.hotkey_enabled             = g_hotkey_enabled;
    s.hotkey_win                 = g_hotkey_win;
    s.hotkey_alt                 = g_hotkey_alt;
    s.hotkey_ctrl                = g_hotkey_ctrl;
    s.hotkey_shift               = g_hotkey_shift;
    s.hotkey_keycode             = g_hotkey_keycode;
    s.fmt                        = g_fmt_settings;
    s.colors                     = g_colors;
    s.user_colors                = g_user_colors;
    s.color_preset               = g_color_preset;
    return s;
}

void restore(const Snapshot &s) {
    g_language                   = s.language;
    g_font_id                    = s.font_id;
    g_font_size                  = s.font_size;
    g_input_auto_completion      = s.input_auto_completion;
    g_input_auto_close_brackets  = s.input_auto_close_brackets;
    g_input_bs_delete_empty_row  = s.input_bs_delete_empty_row;
    g_popup_independent_normal   = s.popup_independent_normal;
    g_popup_independent_compact  = s.popup_independent_compact;
    g_sep_thousands              = s.sep_thousands;
    g_sep_hex                    = s.sep_hex;
    g_limit_max_array_length     = s.limit_max_array_length;
    g_limit_max_string_length    = s.limit_max_string_length;
    g_limit_max_call_depth       = s.limit_max_call_depth;
    g_show_rowlines              = s.show_rowlines;
    g_remember_position          = s.remember_position;
    g_start_topmost              = s.start_topmost;
    g_tray_icon                  = s.tray_icon;
    g_hotkey_enabled             = s.hotkey_enabled;
    g_hotkey_win                 = s.hotkey_win;
    g_hotkey_alt                 = s.hotkey_alt;
    g_hotkey_ctrl                = s.hotkey_ctrl;
    g_hotkey_shift               = s.hotkey_shift;
    g_hotkey_keycode             = s.hotkey_keycode;
    g_fmt_settings               = s.fmt;
    g_colors                     = s.colors;
    g_user_colors                = s.user_colors;
    g_color_preset               = s.color_preset;
}

}  // namespace AppSettings
