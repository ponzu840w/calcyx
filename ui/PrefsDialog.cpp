// PrefsDialog — 設定ダイアログの枠組み (5 タブ: General / Appearance /
// Input / Number Format / Calculation)。タブ本体は prefs/tab_*.cpp、
// 共有ロジックは prefs/prefs_common.{h,cpp} に分割してある。

#include "PrefsDialog.h"
#include "SheetView.h"
#include "prefs/prefs_common.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Button.H>
#include <FL/fl_ask.H>

void PrefsDialog::run(SheetView *sheet, PrefsApplyUiCb ui_cb, void *ui_data) {
    DlgState st{};
    st.sheet = sheet;
    st.ui_cb = ui_cb;
    st.ui_data = ui_data;
    st.saved_colors      = g_colors;
    st.saved_preset      = g_color_preset;
    st.user_colors       = g_colors;
    st.saved_user_colors = g_colors;

    Fl_Double_Window dlg(DW, DH, "Preferences");
    dlg.set_modal();
    dlg.color(DLG_BG);
    st.dlg_win = &dlg;

    const int TAB_H = DH - 48;
    Fl_Tabs tabs(5, 5, DW - 10, TAB_H);
    tabs.color(DLG_BG);
    tabs.labelcolor(DLG_LABEL);
    tabs.selection_color(DLG_BG);
    st.tabs = &tabs;

    build_general_tab(st, TAB_H);
    build_appearance_tab(st, TAB_H);
    build_input_tab(st, TAB_H);
    build_number_format_tab(st, TAB_H);
    build_calculation_tab(st, TAB_H);

    tabs.end();

    // プレビューの初期化 (Cancel 復元用に現在値をスナップショット)
    st.saved_font_id          = g_font_id;
    st.saved_font_size        = g_font_size;
    st.saved_fmt              = g_fmt_settings;
    st.saved_sep_thousands    = g_sep_thousands;
    st.saved_sep_hex          = g_sep_hex;
    st.saved_limit_array      = g_limit_max_array_length;
    st.saved_limit_string     = g_limit_max_string_length;
    st.saved_limit_depth      = g_limit_max_call_depth;
    st.saved_show_rowlines    = g_show_rowlines;
    st.saved_remember_pos     = g_remember_position;
    st.saved_tray_icon        = g_tray_icon;
    st.saved_hotkey_enabled   = g_hotkey_enabled;
    st.saved_hotkey_win       = g_hotkey_win;
    st.saved_hotkey_alt       = g_hotkey_alt;
    st.saved_hotkey_ctrl      = g_hotkey_ctrl;
    st.saved_hotkey_shift     = g_hotkey_shift;
    st.saved_hotkey_keycode   = g_hotkey_keycode;
    refresh_previews(&st);
    update_swatch_state(&st);

    // --- OK / Cancel / Apply / Reset ---
    int by = DH - 38;
    int bw = 80, bh = 28;

    Fl_Button reset_btn(8, by, 60, bh, "Reset");
    reset_btn.color(DLG_BTN);
    reset_btn.labelcolor(DLG_TEXT);
    reset_btn.labelsize(12);
    reset_btn.tooltip("Reset all settings to defaults");
    reset_btn.callback([](Fl_Widget *, void *data) {
        if (fl_choice("Reset all settings to defaults?", "Cancel", "Reset", nullptr) == 1)
            reset_to_defaults(static_cast<DlgState *>(data));
    }, &st);

    Fl_Button ok_btn(DW - 3 * (bw + 8), by, bw, bh, "OK");
    ok_btn.color(DLG_BTN);
    ok_btn.labelcolor(DLG_TEXT);
    ok_btn.labelsize(12);

    Fl_Button cancel_btn(DW - 2 * (bw + 8), by, bw, bh, "Cancel");
    cancel_btn.color(DLG_BTN);
    cancel_btn.labelcolor(DLG_TEXT);
    cancel_btn.labelsize(12);

    Fl_Button apply_btn(DW - (bw + 8), by, bw, bh, "Apply");
    apply_btn.color(DLG_BTN);
    apply_btn.labelcolor(DLG_TEXT);
    apply_btn.labelsize(12);

    ok_btn.callback([](Fl_Widget *w, void *data) {
        apply_settings(static_cast<DlgState *>(data));
        w->window()->hide();
    }, &st);

    cancel_btn.callback([](Fl_Widget *w, void *data) {
        auto *st = static_cast<DlgState *>(data);
        g_colors        = st->saved_colors;
        g_color_preset  = st->saved_preset;
        g_font_id       = st->saved_font_id;
        g_font_size     = st->saved_font_size;
        g_fmt_settings  = st->saved_fmt;
        g_sep_thousands           = st->saved_sep_thousands;
        g_sep_hex                 = st->saved_sep_hex;
        g_limit_max_array_length  = st->saved_limit_array;
        g_limit_max_string_length = st->saved_limit_string;
        g_limit_max_call_depth    = st->saved_limit_depth;
        g_show_rowlines           = st->saved_show_rowlines;
        g_remember_position       = st->saved_remember_pos;
        g_start_topmost           = st->saved_start_topmost;
        g_tray_icon               = st->saved_tray_icon;
        g_hotkey_enabled          = st->saved_hotkey_enabled;
        g_hotkey_win              = st->saved_hotkey_win;
        g_hotkey_alt              = st->saved_hotkey_alt;
        g_hotkey_ctrl             = st->saved_hotkey_ctrl;
        g_hotkey_shift            = st->saved_hotkey_shift;
        g_hotkey_keycode          = st->saved_hotkey_keycode;
        st->sheet->apply_font();
        st->sheet->live_eval();
        st->sheet->redraw();
        if (st->ui_cb) st->ui_cb(st->ui_data);
        w->window()->hide();
    }, &st);

    apply_btn.callback([](Fl_Widget *, void *data) {
        apply_settings(static_cast<DlgState *>(data));
    }, &st);

    dlg.end();
    dlg.show();
    while (dlg.shown()) Fl::wait();
}
