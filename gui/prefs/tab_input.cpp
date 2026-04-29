// tab_input.cpp — Input タブ (補完 / 括弧の自動閉じ)

#include "prefs_common.h"
#include "i18n.h"

void build_input_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, _(" Input "));
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    const int lx = 16;
    const int sw = DW - 40;
    int ly = 50;

    // ===== Completion =====
    {
        int body_h = 134;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Completion"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        st.auto_complete_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Auto-completion on typing")),
            "auto_completion");
        style_check(st.auto_complete_chk);
        st.auto_complete_chk->value(g_input_auto_completion ? 1 : 0);
        inner_y += 22;

        Fl_Box *note = new Fl_Box(lx + 30, inner_y, sw - 40, 18,
            _("Ctrl+Space opens the popup regardless of this setting."));
        note->box(FL_NO_BOX);
        note->labelcolor(DLG_LABEL);
        note->labelsize(11);
        note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        inner_y += 24;

        // 補完ポップアップを独立 OS ウィンドウで出すかどうか。
        // 独立ウィンドウはメインウィンドウ外にはみ出せるが、ウィンドウ
        // 生成/破棄のコストとフォーカス管理の面倒が乗る。
        Fl_Box *pop_label = new Fl_Box(lx + 10, inner_y, sw - 20, 20,
            _("Show popup as a separate window:"));
        pop_label->box(FL_NO_BOX);
        pop_label->labelcolor(DLG_TEXT);
        pop_label->labelsize(12);
        pop_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        inner_y += 22;

        st.popup_indep_normal_chk = make_lockable(
            new Fl_Check_Button(lx + 30, inner_y, sw - 40, 22,
                _("In normal mode")),
            "popup_independent_normal");
        style_check(st.popup_indep_normal_chk);
        st.popup_indep_normal_chk->value(g_popup_independent_normal ? 1 : 0);
        inner_y += 22;

        st.popup_indep_compact_chk = make_lockable(
            new Fl_Check_Button(lx + 30, inner_y, sw - 40, 22,
                _("In compact mode")),
            "popup_independent_compact");
        style_check(st.popup_indep_compact_chk);
        st.popup_indep_compact_chk->value(g_popup_independent_compact ? 1 : 0);

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Brackets =====
    {
        int body_h = 36;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Brackets"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        st.auto_brackets_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Auto-close brackets ( ) [ ] { }")),
            "auto_close_brackets");
        style_check(st.auto_brackets_chk);
        st.auto_brackets_chk->value(g_input_auto_close_brackets ? 1 : 0);

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Editing =====
    {
        int body_h = 36;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Editing"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        st.bs_delete_empty_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Backspace on empty row deletes the row")),
            "bs_delete_empty_row");
        style_check(st.bs_delete_empty_chk);
        st.bs_delete_empty_chk->value(g_input_bs_delete_empty_row ? 1 : 0);

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    g->end();
}
