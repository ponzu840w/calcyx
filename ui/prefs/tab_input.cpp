// tab_input.cpp — Input タブ (自動補完 / 括弧の自動閉じ)

#include "prefs_common.h"

void build_input_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " Input ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    int lx = 20, ly = 60;

    st.auto_complete_chk = new Fl_Check_Button(lx, ly, 300, 25, "Auto-completion (Ctrl+Space always works)");
    style_check(st.auto_complete_chk);
    st.auto_complete_chk->value(g_input_auto_completion ? 1 : 0);
    ly += 35;

    st.auto_brackets_chk = new Fl_Check_Button(lx, ly, 300, 25, "Auto-close brackets ( ) [ ] { }");
    style_check(st.auto_brackets_chk);
    st.auto_brackets_chk->value(g_input_auto_close_brackets ? 1 : 0);

    g->end();
}
