// tab_number_format.cpp — Number Format タブ (小数桁・E 表記・桁区切り)

#include "prefs_common.h"
#include "SheetView.h"
#include <FL/Fl_Box.H>

static void fmt_change_cb(Fl_Widget *, void *data) {
    refresh_previews(static_cast<DlgState *>(data));
}

void build_number_format_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " Number Format ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    int lx = 20, ly = 55, lw = 200;

    auto make_row = [&](const char *label, double mn, double mx, double val) -> Fl_Spinner * {
        Fl_Box *lb = new Fl_Box(lx, ly, lw, 25, label);
        style_label(lb);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        auto *sp = new Fl_Spinner(lx + lw + 10, ly, 80, 25);
        style_spinner(sp);
        sp->range(mn, mx);
        sp->step(1);
        sp->value(val);
        ly += 35;
        return sp;
    };

    st.fmt_decimal_spin = make_row("Decimal digits:", 1, 34, g_fmt_settings.decimal_len);
    st.fmt_decimal_spin->callback(fmt_change_cb, &st);

    st.fmt_exp_chk = new Fl_Check_Button(lx, ly, 250, 25, "Scientific notation (E)");
    style_check(st.fmt_exp_chk);
    st.fmt_exp_chk->value(g_fmt_settings.e_notation ? 1 : 0);
    st.fmt_exp_chk->callback(fmt_change_cb, &st);
    ly += 30;

    st.fmt_exp_pos_spin = make_row("E notation positive min:", 1, 30, g_fmt_settings.e_positive_min);
    st.fmt_exp_pos_spin->callback(fmt_change_cb, &st);
    st.fmt_exp_neg_spin = make_row("E notation negative max:", -30, -1, g_fmt_settings.e_negative_max);
    st.fmt_exp_neg_spin->callback(fmt_change_cb, &st);

    st.fmt_align_chk = new Fl_Check_Button(lx, ly, 250, 25, "Align E notation");
    style_check(st.fmt_align_chk);
    st.fmt_align_chk->value(g_fmt_settings.e_alignment ? 1 : 0);
    st.fmt_align_chk->callback(fmt_change_cb, &st);
    ly += 35;

    st.sep_thousands_chk = new Fl_Check_Button(lx, ly, DW - 40, 25, "Separate decimal numbers every 3 digits");
    style_check(st.sep_thousands_chk);
    st.sep_thousands_chk->value(g_sep_thousands ? 1 : 0);
    st.sep_thousands_chk->callback(fmt_change_cb, &st);
    ly += 30;

    st.sep_hex_chk = new Fl_Check_Button(lx, ly, DW - 40, 25, "Separate hex/bin/oct numbers every 4 digits");
    style_check(st.sep_hex_chk);
    st.sep_hex_chk->value(g_sep_hex ? 1 : 0);
    st.sep_hex_chk->callback(fmt_change_cb, &st);

    ly += 35;
    int preview_h = tab_h - 25 - (ly - 30);
    st.fmt_preview_sv = new SheetView(lx, ly, DW - 50, preview_h, true);

    g->end();
}
