// tab_calculation.cpp — Calculation タブ (実行時制限)

#include "prefs_common.h"
#include <FL/Fl_Box.H>

void build_calculation_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " Calculation ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    const int lx = 16;
    const int sw = DW - 40;
    int ly = 50;

    int body_h = 110;
    Fl_Group *sec = begin_section(lx, ly, sw, body_h, "Limits");
    int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

    const int label_x = lx + 10;
    const int label_w = 220;
    const int spin_w  = 100;

    auto add_row = [&](const char *label, double mn, double mx, double val) -> Fl_Spinner * {
        Fl_Box *lb = new Fl_Box(label_x, inner_y, label_w, 24, label);
        style_label(lb);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        auto *sp = new Fl_Spinner(label_x + label_w, inner_y, spin_w, 24);
        style_spinner(sp);
        sp->range(mn, mx);
        sp->step(1);
        sp->value(val);
        inner_y += 30;
        return sp;
    };

    st.limit_array_spin  = add_row("Max array length:",        1, 1000000, g_limit_max_array_length);
    st.limit_string_spin = add_row("Max string length:",       1, 1000000, g_limit_max_string_length);
    st.limit_depth_spin  = add_row("Max call recursion depth:", 1, 1000,    g_limit_max_call_depth);

    sec->end();
    g->end();
}
