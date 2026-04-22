// tab_number_format.cpp — Number Format タブ
// セクション:
//   - Decimal (小数桁)
//   - Scientific Notation (E 表記 enable + 閾値 + Engineering alignment)
//   - Numeric Separators (3/4 桁区切り)
//   - Preview (プレビュー SheetView)

#include "prefs_common.h"
#include "SheetView.h"
#include <FL/Fl_Box.H>

static void fmt_change_cb(Fl_Widget *, void *data) {
    refresh_previews(static_cast<DlgState *>(data));
}

// E-notation enable で依存ウィジェットをグレーアウト
static void exp_enable_sync(DlgState *st) {
    bool on = st->fmt_exp_chk->value() != 0;
    auto set = [on](Fl_Widget *w) { if (on) w->activate(); else w->deactivate(); };
    set(st->fmt_exp_pos_spin);
    set(st->fmt_exp_neg_spin);
    set(st->fmt_align_chk);
}

static void exp_toggle_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    exp_enable_sync(st);
    refresh_previews(st);
}

void build_number_format_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " Number Format ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    const int lx = 16;
    const int sw = DW - 40;
    int ly = 50;

    // ===== Decimal =====
    {
        int body_h = 40;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, "Decimal");
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        Fl_Box *lb = new Fl_Box(lx + 10, inner_y, 300, 22,
            "Max length of decimal places to display:");
        style_label(lb);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.fmt_decimal_spin = new Fl_Spinner(lx + 10 + 300, inner_y, 70, 22);
        style_spinner(st.fmt_decimal_spin);
        st.fmt_decimal_spin->range(1, 34);
        st.fmt_decimal_spin->step(1);
        st.fmt_decimal_spin->value(g_fmt_settings.decimal_len);
        st.fmt_decimal_spin->callback(fmt_change_cb, &st);
        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Scientific Notation =====
    // 構成: enable チェック (セクション外) + 枠内に 2 つの log10 閾値 + Engineering Alignment
    {
        st.fmt_exp_chk = new Fl_Check_Button(lx + 4, ly, 220, 20,
            "Scientific notation (E)");
        style_check(st.fmt_exp_chk);
        st.fmt_exp_chk->value(g_fmt_settings.e_notation ? 1 : 0);
        st.fmt_exp_chk->callback(exp_toggle_cb, &st);
        ly += 22;

        int body_h = 86;
        Fl_Group *sec = new Fl_Group(lx, ly, sw, body_h);
        sec->box(FL_ENGRAVED_FRAME);
        sec->color(DLG_BG);
        sec->begin();
        int inner_y = ly + SECTION_PAD_TOP;

        Fl_Box *lb_pos = new Fl_Box(lx + 10, inner_y, 110, 22, "log10(x) \xe2\x89\xa7");
        style_label(lb_pos);
        lb_pos->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.fmt_exp_pos_spin = new Fl_Spinner(lx + 120, inner_y, 70, 22);
        style_spinner(st.fmt_exp_pos_spin);
        st.fmt_exp_pos_spin->range(1, 30);
        st.fmt_exp_pos_spin->step(1);
        st.fmt_exp_pos_spin->value(g_fmt_settings.e_positive_min);
        st.fmt_exp_pos_spin->callback(fmt_change_cb, &st);
        inner_y += 26;

        Fl_Box *lb_neg = new Fl_Box(lx + 10, inner_y, 110, 22, "log10(x) \xe2\x89\xa6");
        style_label(lb_neg);
        lb_neg->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.fmt_exp_neg_spin = new Fl_Spinner(lx + 120, inner_y, 70, 22);
        style_spinner(st.fmt_exp_neg_spin);
        st.fmt_exp_neg_spin->range(-30, -1);
        st.fmt_exp_neg_spin->step(1);
        st.fmt_exp_neg_spin->value(g_fmt_settings.e_negative_max);
        st.fmt_exp_neg_spin->callback(fmt_change_cb, &st);
        inner_y += 26;

        st.fmt_align_chk = new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
            "Engineering alignment");
        style_check(st.fmt_align_chk);
        st.fmt_align_chk->value(g_fmt_settings.e_alignment ? 1 : 0);
        st.fmt_align_chk->callback(fmt_change_cb, &st);
        sec->end();

        exp_enable_sync(&st);
        ly += body_h + SECTION_GAP;
    }

    // ===== Numeric Separators =====
    {
        int body_h = 62;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, "Numeric Separators");
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        st.sep_thousands_chk = new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
            "Separate decimal numbers every 3 digits");
        style_check(st.sep_thousands_chk);
        st.sep_thousands_chk->value(g_sep_thousands ? 1 : 0);
        st.sep_thousands_chk->callback(fmt_change_cb, &st);
        inner_y += 26;

        st.sep_hex_chk = new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
            "Separate hex/bin/oct numbers every 4 digits");
        style_check(st.sep_hex_chk);
        st.sep_hex_chk->value(g_sep_hex ? 1 : 0);
        st.sep_hex_chk->callback(fmt_change_cb, &st);
        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Preview (残りのスペース) =====
    // 上: 直前のセクション枠との間に小さな余白 / 下: タブ枠との間に同じ余白
    const int PREVIEW_MARGIN = 6;
    ly += PREVIEW_MARGIN;
    int preview_h = tab_h - 25 - (ly - 30) - PREVIEW_MARGIN;
    if (preview_h > 40) {
        st.fmt_preview_sv = new SheetView(lx, ly, sw, preview_h, true);
    }

    g->end();
}
