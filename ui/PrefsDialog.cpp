// PrefsDialog — 設定ダイアログ (4タブ: Font, Colors, Number Format, Input)

#include "PrefsDialog.h"
#include "SheetView.h"
#include "settings_globals.h"
#include "colors.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/filename.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Color_Chooser.H>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#endif

extern "C" {
#include "types/val.h"
}

static const Fl_Color DLG_BG      = fl_rgb_color( 38,  38,  43);
static const Fl_Color DLG_INPUT   = fl_rgb_color( 50,  52,  60);
static const Fl_Color DLG_BTN     = fl_rgb_color( 55,  60,  75);
static const Fl_Color DLG_TEXT    = fl_rgb_color(215, 215, 225);
static const Fl_Color DLG_LABEL   = fl_rgb_color(180, 180, 190);

static const int DW = 480;
static const int DH = 480;

// ---- Font tab ----
struct FontTab {
    Fl_Choice  *font_choice;
    Fl_Spinner *size_spin;
    Fl_Box     *preview;
};

struct FontEntry {
    const char *label;
    int         id;
};

static const FontEntry FONTS[] = {
    { "Courier",     FL_COURIER },
    { "Courier Bold", FL_COURIER_BOLD },
    { "Helvetica",   FL_HELVETICA },
    { "Times",       FL_TIMES },
    { "Screen",      FL_SCREEN },
    { "Screen Bold", FL_SCREEN_BOLD },
};
static const int FONT_COUNT = sizeof(FONTS) / sizeof(FONTS[0]);

static int font_id_to_index(int id) {
    for (int i = 0; i < FONT_COUNT; i++)
        if (FONTS[i].id == id) return i;
    return 0;
}

static void update_preview(FontTab *ft) {
    int idx = ft->font_choice->value();
    int fid = (idx >= 0 && idx < FONT_COUNT) ? FONTS[idx].id : FL_COURIER;
    int fsz = (int)ft->size_spin->value();
    ft->preview->labelfont(fid);
    ft->preview->labelsize(fsz);
    ft->preview->redraw();
}

static void font_change_cb(Fl_Widget *, void *data) {
    update_preview(static_cast<FontTab *>(data));
}

// ---- Colors tab ----
struct ColorEntry {
    const char *label;
    Fl_Color   *target;
};

struct ColorsTab {
    Fl_Button *swatches[15];
    ColorEntry entries[15];
    int count;
};

static void swatch_cb(Fl_Widget *w, void *data) {
    Fl_Color *target = static_cast<Fl_Color *>(data);
    uchar r, g, b;
    Fl::get_color(*target, r, g, b);
    if (fl_color_chooser("Color", r, g, b)) {
        *target = fl_rgb_color(r, g, b);
        w->color(*target);
        w->redraw();
    }
}

// ---- state for apply ----
struct DlgState {
    FontTab     font;
    ColorsTab   colors;
    Fl_Spinner *fmt_decimal_spin;
    Fl_Check_Button *fmt_exp_chk;
    Fl_Spinner *fmt_exp_pos_spin;
    Fl_Spinner *fmt_exp_neg_spin;
    Fl_Check_Button *fmt_align_chk;
    Fl_Check_Button *sep_thousands_chk;
    Fl_Check_Button *sep_hex_chk;
    Fl_Check_Button *auto_complete_chk;
    Fl_Check_Button *auto_brackets_chk;
    SheetView  *sheet;
    CalcyxColors saved_colors;
};

static void apply_settings(DlgState *st) {
    int idx = st->font.font_choice->value();
    g_font_id   = (idx >= 0 && idx < FONT_COUNT) ? FONTS[idx].id : FL_COURIER;
    g_font_size  = (int)st->font.size_spin->value();

    g_fmt_settings.decimal_len    = (int)st->fmt_decimal_spin->value();
    g_fmt_settings.e_notation     = st->fmt_exp_chk->value() != 0;
    g_fmt_settings.e_positive_min = (int)st->fmt_exp_pos_spin->value();
    g_fmt_settings.e_negative_max = (int)st->fmt_exp_neg_spin->value();
    g_fmt_settings.e_alignment    = st->fmt_align_chk->value() != 0;
    g_sep_thousands = st->sep_thousands_chk->value() != 0;
    g_sep_hex       = st->sep_hex_chk->value() != 0;

    g_input_auto_completion     = st->auto_complete_chk->value() != 0;
    g_input_auto_close_brackets = st->auto_brackets_chk->value() != 0;

    st->sheet->apply_font();
    st->sheet->live_eval();
    st->sheet->redraw();

    settings_save();
}

static void style_label(Fl_Widget *w) {
    w->labelcolor(DLG_LABEL);
    w->labelsize(12);
}

static void style_check(Fl_Check_Button *chk) {
    chk->color(DLG_BG);
    chk->labelcolor(DLG_TEXT);
    chk->labelsize(12);
    chk->selection_color(fl_rgb_color(80, 140, 255));
}

static void style_spinner(Fl_Spinner *sp) {
    sp->color(DLG_INPUT);
    sp->textcolor(DLG_TEXT);
    sp->labelcolor(DLG_LABEL);
    sp->labelsize(12);
    sp->textsize(12);
    sp->selection_color(fl_rgb_color(60, 80, 120));
}

void PrefsDialog::run(SheetView *sheet) {
    DlgState st;
    st.sheet = sheet;
    st.saved_colors = g_colors;

    Fl_Double_Window dlg(DW, DH, "Preferences");
    dlg.set_modal();
    dlg.color(DLG_BG);

    const int TAB_H = 400;
    Fl_Tabs tabs(5, 5, DW - 10, TAB_H);
    tabs.color(fl_rgb_color(28, 28, 32));
    tabs.labelcolor(DLG_LABEL);
    tabs.selection_color(DLG_BG);

    // ======== Font tab ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " Font ");
        g->color(DLG_BG);
        g->selection_color(DLG_BG);
        g->labelcolor(DLG_TEXT);
        g->labelsize(12);

        int lx = 20, ly = 50, lw = 120;

        Fl_Box *lb1 = new Fl_Box(lx, ly, lw, 25, "Font:");
        style_label(lb1);
        lb1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.font_choice = new Fl_Choice(lx + lw, ly, 200, 25);
        st.font.font_choice->color(DLG_INPUT);
        st.font.font_choice->textcolor(DLG_TEXT);
        st.font.font_choice->textsize(12);
        for (int i = 0; i < FONT_COUNT; i++)
            st.font.font_choice->add(FONTS[i].label);
        st.font.font_choice->value(font_id_to_index(g_font_id));
        st.font.font_choice->callback(font_change_cb, &st.font);

        ly += 35;
        Fl_Box *lb2 = new Fl_Box(lx, ly, lw, 25, "Size:");
        style_label(lb2);
        lb2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.size_spin = new Fl_Spinner(lx + lw, ly, 80, 25);
        style_spinner(st.font.size_spin);
        st.font.size_spin->range(8, 36);
        st.font.size_spin->step(1);
        st.font.size_spin->value(g_font_size);
        st.font.size_spin->callback(font_change_cb, &st.font);

        ly += 45;
        st.font.preview = new Fl_Box(lx, ly, DW - 50, 80, "AaBbCc 0123456789 +-*/=");
        st.font.preview->box(FL_DOWN_BOX);
        st.font.preview->color(fl_rgb_color(22, 22, 22));
        st.font.preview->labelcolor(fl_rgb_color(255, 255, 255));
        update_preview(&st.font);

        g->end();
    }

    // ======== Colors tab ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " Colors ");
        g->color(DLG_BG);
        g->selection_color(DLG_BG);
        g->labelcolor(DLG_TEXT);
        g->labelsize(12);

        struct { const char *label; Fl_Color *color; } entries[] = {
            { "Background",   &g_colors.bg },
            { "Selection",    &g_colors.sel_bg },
            { "Row Line",     &g_colors.rowline },
            { "Separator",    &g_colors.sep },
            { "Text",         &g_colors.text },
            { "Cursor",       &g_colors.cursor },
            { "Symbols",      &g_colors.symbol },
            { "Identifiers",  &g_colors.ident },
            { "Literals",     &g_colors.special },
            { "SI Prefix",    &g_colors.si_pfx },
            { "Paren 1",      &g_colors.paren[0] },
            { "Paren 2",      &g_colors.paren[1] },
            { "Paren 3",      &g_colors.paren[2] },
            { "Paren 4",      &g_colors.paren[3] },
            { "Error",        &g_colors.error },
        };

        st.colors.count = 15;
        int col1_x = 20, col2_x = DW / 2 + 10;
        int sy = 50;
        for (int i = 0; i < 15; i++) {
            int cx = (i < 8) ? col1_x : col2_x;
            int cy = sy + (i < 8 ? i : i - 8) * 30;
            Fl_Box *lb = new Fl_Box(cx, cy, 100, 25, entries[i].label);
            style_label(lb);
            lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

            auto *btn = new Fl_Button(cx + 105, cy, 80, 25);
            btn->box(FL_DOWN_BOX);
            btn->color(*entries[i].color);
            btn->callback(swatch_cb, entries[i].color);
            st.colors.swatches[i] = btn;
            st.colors.entries[i] = { entries[i].label, entries[i].color };
        }

        // Reset ボタン
        auto *reset = new Fl_Button(DW / 2 - 40, DH - 100, 80, 25, "Reset");
        reset->color(DLG_BTN);
        reset->labelcolor(DLG_TEXT);
        reset->labelsize(12);
        reset->callback([](Fl_Widget *, void *data) {
            auto *st = static_cast<DlgState *>(data);
            colors_init_defaults(&g_colors);
            for (int i = 0; i < st->colors.count; i++)
                st->colors.swatches[i]->color(*st->colors.entries[i].target);
            st->sheet->redraw();
        }, &st);

        g->end();
    }

    // ======== Number Format tab ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " Number Format ");
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

        st.fmt_exp_chk = new Fl_Check_Button(lx, ly, 250, 25, "Scientific notation (E)");
        style_check(st.fmt_exp_chk);
        st.fmt_exp_chk->value(g_fmt_settings.e_notation ? 1 : 0);
        ly += 30;

        st.fmt_exp_pos_spin = make_row("E notation positive min:", 1, 30, g_fmt_settings.e_positive_min);
        st.fmt_exp_neg_spin = make_row("E notation negative max:", -30, -1, g_fmt_settings.e_negative_max);

        st.fmt_align_chk = new Fl_Check_Button(lx, ly, 250, 25, "Align E notation");
        style_check(st.fmt_align_chk);
        st.fmt_align_chk->value(g_fmt_settings.e_alignment ? 1 : 0);
        ly += 35;

        st.sep_thousands_chk = new Fl_Check_Button(lx, ly, 250, 25, "Thousands separator (1,000)");
        style_check(st.sep_thousands_chk);
        st.sep_thousands_chk->value(g_sep_thousands ? 1 : 0);
        ly += 30;

        st.sep_hex_chk = new Fl_Check_Button(lx, ly, 250, 25, "Hex separator (0xFF_FF)");
        style_check(st.sep_hex_chk);
        st.sep_hex_chk->value(g_sep_hex ? 1 : 0);

        g->end();
    }

    // ======== Input tab ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " Input ");
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

    tabs.end();

    // ======== OK / Cancel / Apply / Open File ========
    int by = DH - 38;
    int bw = 80, bh = 28;

    Fl_Button open_btn(8, by, bw + 30, bh, "Open conf file");
    open_btn.color(DLG_BTN);
    open_btn.labelcolor(DLG_LABEL);
    open_btn.labelsize(11);
    open_btn.tooltip(settings_path());
    open_btn.callback([](Fl_Widget *, void *) {
        settings_save();
        const char *path = settings_path();
#if defined(_WIN32)
        ShellExecuteA(NULL, "open", "notepad.exe", path, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        std::string cmd = std::string("open -t \"") + path + "\"";
        system(cmd.c_str());
#else
        // Linux: open explicitly as text
        std::string cmd = std::string("xdg-open \"") + path + "\" 2>/dev/null &";
        const char *editor = getenv("VISUAL");
        if (!editor || !editor[0]) editor = getenv("EDITOR");
        if (editor && editor[0])
            cmd = std::string(editor) + " \"" + path + "\" &";
        system(cmd.c_str());
#endif
    }, nullptr);

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
        g_colors = st->saved_colors;
        st->sheet->redraw();
        w->window()->hide();
    }, &st);

    apply_btn.callback([](Fl_Widget *, void *data) {
        apply_settings(static_cast<DlgState *>(data));
    }, &st);

    dlg.end();
    dlg.show();
    while (dlg.shown()) Fl::wait();
}
