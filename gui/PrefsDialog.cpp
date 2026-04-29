// PrefsDialog — 設定ダイアログの枠組み (5 タブ: General / Appearance /
// Input / Number Format / Calculation)。タブ本体は prefs/tab_*.cpp、
// 共有ロジックは prefs/prefs_common.{h,cpp} に分割してある。

#include "PrefsDialog.h"
#include "SheetView.h"
#include "prefs/prefs_common.h"
#include "i18n.h"
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
    /* Cancel 用に全設定をスナップショット (色 / フォント / 数値書式 / 制限 /
     * トレイ・ホットキー / 表示) を 1 個の構造体にまとめて取得。 */
    st.saved = AppSettings::capture();

    Fl_Double_Window dlg(DW, DH, _("Preferences"));
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

    refresh_previews(&st);
    update_swatch_state(&st);

    // --- OK / Cancel / Apply / Reset ---
    int by = DH - 38;
    int bw = 80, bh = 28;

    Fl_Button reset_btn(8, by, 60, bh, _("Reset"));
    reset_btn.color(DLG_BTN);
    reset_btn.labelcolor(DLG_TEXT);
    reset_btn.labelsize(12);
    reset_btn.tooltip(_("Reset all settings to defaults"));
    reset_btn.callback([](Fl_Widget *, void *data) {
        if (fl_choice("%s", _("Cancel"), _("Reset"), nullptr,
                      _("Reset all settings to defaults?")) == 1)
            reset_to_defaults(static_cast<DlgState *>(data));
    }, &st);

    Fl_Button ok_btn(DW - 3 * (bw + 8), by, bw, bh, _("OK"));
    ok_btn.color(DLG_BTN);
    ok_btn.labelcolor(DLG_TEXT);
    ok_btn.labelsize(12);

    Fl_Button cancel_btn(DW - 2 * (bw + 8), by, bw, bh, _("Cancel"));
    cancel_btn.color(DLG_BTN);
    cancel_btn.labelcolor(DLG_TEXT);
    cancel_btn.labelsize(12);

    Fl_Button apply_btn(DW - (bw + 8), by, bw, bh, _("Apply"));
    apply_btn.color(DLG_BTN);
    apply_btn.labelcolor(DLG_TEXT);
    apply_btn.labelsize(12);

    ok_btn.callback([](Fl_Widget *w, void *data) {
        apply_settings(static_cast<DlgState *>(data));
        w->window()->hide();
    }, &st);

    cancel_btn.callback([](Fl_Widget *w, void *data) {
        auto *st = static_cast<DlgState *>(data);
        AppSettings::restore(st->saved);
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
