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
#include <FL/Fl_Hold_Browser.H>
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
#include <vector>
#include <set>
#include <string>
#include <algorithm>

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

static const int DW = 540;
static const int DH = 576;

// ---- Font tab ----
struct SysFont {
    std::string name;
    Fl_Font     id;
    bool        monospace;
};

static std::vector<SysFont> s_all_fonts;
static bool s_fonts_loaded = false;

static bool detect_monospace(Fl_Font id) {
    fl_font(id, 14);
    return fl_width("iiii", 4) == fl_width("MMMM", 4);
}

static void load_system_fonts() {
    if (s_fonts_loaded) return;
    s_fonts_loaded = true;
    Fl_Font n = Fl::set_fonts(nullptr);
    std::set<std::string> seen;
    for (Fl_Font i = 0; i < n; i++) {
        int attr = 0;
        const char *raw = Fl::get_font_name(i, &attr);
        if (!raw || !raw[0]) continue;
        if (raw[0] == ' ' || raw[0] == '.') continue;
        std::string name(raw);
        if (!seen.insert(name).second) continue;
        bool mono = detect_monospace(i);
        s_all_fonts.push_back({name, i, mono});
    }
    std::sort(s_all_fonts.begin(), s_all_fonts.end(),
              [](const SysFont &a, const SysFont &b) { return a.name < b.name; });
}

static const struct { const char *label; Fl_Font id; bool mono; } BUILTIN_FONTS[] = {
    { "Courier",      FL_COURIER,      true  },
    { "Courier Bold", FL_COURIER_BOLD, true  },
    { "Helvetica",    FL_HELVETICA,    false },
    { "Times",        FL_TIMES,        false },
    { "Screen",       FL_SCREEN,       true  },
    { "Screen Bold",  FL_SCREEN_BOLD,  true  },
};
static const int BUILTIN_COUNT = sizeof(BUILTIN_FONTS) / sizeof(BUILTIN_FONTS[0]);

struct DlgState;
static void refresh_previews(DlgState *st);

struct FontTab {
    Fl_Button       *font_btn;
    Fl_Spinner      *size_spin;
    Fl_Box          *preview;
    Fl_Font          selected_id;
    std::string      selected_name;
};

static std::string font_id_to_display_name(Fl_Font id) {
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (BUILTIN_FONTS[i].id == id) return BUILTIN_FONTS[i].label;
    int attr = 0;
    const char *n = Fl::get_font_name(id, &attr);
    if (n && n[0]) return n;
    return "Courier";
}

static void update_font_btn(FontTab *ft) {
    ft->font_btn->copy_label(ft->selected_name.c_str());
    ft->font_btn->labelfont(ft->selected_id);
    ft->font_btn->redraw();
}

static void update_preview(FontTab *ft) {
    if (!ft->preview) return;
    ft->preview->labelfont(ft->selected_id);
    ft->preview->labelsize((int)ft->size_spin->value());
    ft->preview->redraw();
}

static void open_font_picker(FontTab *ft, Fl_Window *parent) {
    const int PW = 500, PH = 480;
    Fl_Double_Window picker(PW, PH, "Select Font");
    picker.set_modal();
    picker.color(DLG_BG);

    Fl_Hold_Browser browser(10, 10, PW - 20, PH - 100);
    browser.color(DLG_INPUT);
    browser.textcolor(DLG_TEXT);
    browser.textsize(13);
    browser.selection_color(fl_rgb_color(60, 80, 120));

    Fl_Check_Button sys_chk(10, PH - 82, 180, 22, "Use system fonts");
    sys_chk.labelcolor(DLG_LABEL);
    sys_chk.labelsize(12);

    Fl_Check_Button prop_chk(200, PH - 82, PW - 210, 22,
                             "Show proportional fonts (not recommended)");
    prop_chk.labelcolor(DLG_LABEL);
    prop_chk.labelsize(12);

    // 現在のフォントが組み込みか判定
    bool cur_builtin = false;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (BUILTIN_FONTS[i].id == ft->selected_id) { cur_builtin = true; break; }
    sys_chk.value(cur_builtin ? 0 : 1);

    struct PickerState {
        Fl_Hold_Browser *browser;
        Fl_Check_Button *sys_chk;
        Fl_Check_Button *prop_chk;
        std::vector<Fl_Font> ids;
        Fl_Font cur_id;
    };
    PickerState ps = { &browser, &sys_chk, &prop_chk, {}, ft->selected_id };

    auto rebuild = [](PickerState *ps) {
        ps->browser->clear();
        ps->ids.clear();
        bool use_sys  = ps->sys_chk->value() != 0;
        bool show_all = ps->prop_chk->value() != 0;
        int select = 0;

        if (!use_sys) {
            for (int i = 0; i < BUILTIN_COUNT; i++) {
                if (!show_all && !BUILTIN_FONTS[i].mono) continue;
                if (BUILTIN_FONTS[i].id == ps->cur_id)
                    select = (int)ps->ids.size() + 1;
                ps->ids.push_back(BUILTIN_FONTS[i].id);
                ps->browser->add(BUILTIN_FONTS[i].label);
            }
        } else {
            load_system_fonts();
            for (int i = 0; i < (int)s_all_fonts.size(); i++) {
                if (!show_all && !s_all_fonts[i].monospace) continue;
                if (s_all_fonts[i].id == ps->cur_id)
                    select = (int)ps->ids.size() + 1;
                ps->ids.push_back(s_all_fonts[i].id);
                ps->browser->add(s_all_fonts[i].name.c_str());
            }
        }
        if (select >= 1 && select <= (int)ps->ids.size()) {
            ps->browser->value(select);
            ps->browser->middleline(select);
        }
    };
    rebuild(&ps);

    sys_chk.callback([](Fl_Widget *, void *d) {
        auto *ps = static_cast<PickerState *>(d);
        if (ps->sys_chk->value()) load_system_fonts();
        auto rebuild = [](PickerState *ps) {
            ps->browser->clear(); ps->ids.clear();
            bool use_sys = ps->sys_chk->value() != 0;
            bool show_all = ps->prop_chk->value() != 0;
            int select = 0;
            if (!use_sys) {
                for (int i = 0; i < BUILTIN_COUNT; i++) {
                    if (!show_all && !BUILTIN_FONTS[i].mono) continue;
                    if (BUILTIN_FONTS[i].id == ps->cur_id) select = (int)ps->ids.size() + 1;
                    ps->ids.push_back(BUILTIN_FONTS[i].id);
                    ps->browser->add(BUILTIN_FONTS[i].label);
                }
            } else {
                for (int i = 0; i < (int)s_all_fonts.size(); i++) {
                    if (!show_all && !s_all_fonts[i].monospace) continue;
                    if (s_all_fonts[i].id == ps->cur_id) select = (int)ps->ids.size() + 1;
                    ps->ids.push_back(s_all_fonts[i].id);
                    ps->browser->add(s_all_fonts[i].name.c_str());
                }
            }
            if (select >= 1 && select <= (int)ps->ids.size()) {
                ps->browser->value(select); ps->browser->middleline(select);
            }
        };
        rebuild(ps);
    }, &ps);
    prop_chk.callback(sys_chk.callback(), &ps);

    Fl_Button ok_btn(PW - 170, PH - 40, 75, 30, "OK");
    ok_btn.color(DLG_BTN); ok_btn.labelcolor(DLG_TEXT); ok_btn.labelsize(12);
    Fl_Button cancel_btn(PW - 85, PH - 40, 75, 30, "Cancel");
    cancel_btn.color(DLG_BTN); cancel_btn.labelcolor(DLG_TEXT); cancel_btn.labelsize(12);

    bool accepted = false;
    ok_btn.callback([](Fl_Widget *w, void *d) {
        *static_cast<bool *>(d) = true;
        w->window()->hide();
    }, &accepted);
    cancel_btn.callback([](Fl_Widget *w, void *) { w->window()->hide(); }, nullptr);

    // ダブルクリックで確定
    browser.callback([](Fl_Widget *w, void *d) {
        if (Fl::event_clicks()) {
            *static_cast<bool *>(d) = true;
            w->window()->hide();
        }
    }, &accepted);

    picker.end();
    picker.position(parent->x() + (parent->w() - PW) / 2,
                    parent->y() + (parent->h() - PH) / 2);
    picker.show();
    while (picker.shown()) Fl::wait();

    if (accepted) {
        int line = browser.value();
        if (line >= 1 && line <= (int)ps.ids.size()) {
            ft->selected_id = ps.ids[line - 1];
            ft->selected_name = font_id_to_display_name(ft->selected_id);
            update_font_btn(ft);
            update_preview(ft);
        }
    }
}

static void font_btn_cb(Fl_Widget *w, void *data);
static void size_change_cb(Fl_Widget *, void *data);
static void fmt_change_cb(Fl_Widget *, void *data);

// ---- Colors tab ----
struct ColorEntry {
    const char *label;
    Fl_Color   *target;
};

struct SwatchData {
    Fl_Color *target;
    DlgState *dlg;
};

struct ColorsTab {
    Fl_Button *swatches[15];
    ColorEntry entries[15];
    SwatchData swatch_data[15];
    int count;
};

static void swatch_cb(Fl_Widget *w, void *data) {
    auto *sd = static_cast<SwatchData *>(data);
    uchar r, g, b;
    Fl::get_color(*sd->target, r, g, b);
    if (fl_color_chooser("Color", r, g, b)) {
        *sd->target = fl_rgb_color(r, g, b);
        w->color(*sd->target);
        w->redraw();
        refresh_previews(sd->dlg);
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
    // プレビュー用 SheetView (各タブに1つ)
    SheetView  *font_preview_sv;
    SheetView  *color_preview_sv;
    SheetView  *fmt_preview_sv;
    // apply 前の値を保存 (プレビュー後に復元)
    Fl_Font     saved_font_id;
    int         saved_font_size;
    fmt_settings_t saved_fmt;
    bool        saved_sep_thousands;
    bool        saved_sep_hex;
};

static const std::vector<std::string> COLOR_PREVIEW_EXPRS = {
    "123 + 456 * 2",
    "sqrt(PI) / 3",
    "sin(cos(abs(max(1, 2))))",
    "0xFF + 0b1010",
    "1.5e3 + 2.0e-4",
    "si(1000)",
    "\"hello\" + \" world\"",
    "undefined_var",
};

static const std::vector<std::string> PREVIEW_EXPRS = {
    "123 + 456",
    "sqrt(2)",
    "0xFF",
    "sin(PI / 6)",
};

static const std::vector<std::string> FMT_PREVIEW_EXPRS = {
    "1 / 3",
    "1000000 + 1",
    "2 ^ 80",
    "1.23e-10 * 4.56e-7",
    "hex(3735928559)",
    "PI",
    "sqrt(2)",
};

static void write_dlg_to_globals(DlgState *st) {
    g_font_id   = st->font.selected_id;
    g_font_size = (int)st->font.size_spin->value();
    g_fmt_settings.decimal_len    = (int)st->fmt_decimal_spin->value();
    g_fmt_settings.e_notation     = st->fmt_exp_chk->value() != 0;
    g_fmt_settings.e_positive_min = (int)st->fmt_exp_pos_spin->value();
    g_fmt_settings.e_negative_max = (int)st->fmt_exp_neg_spin->value();
    g_fmt_settings.e_alignment    = st->fmt_align_chk->value() != 0;
    g_sep_thousands = st->sep_thousands_chk->value() != 0;
    g_sep_hex       = st->sep_hex_chk->value() != 0;
}

static void refresh_previews(DlgState *st) {
    write_dlg_to_globals(st);
    if (st->font_preview_sv) {
        st->font_preview_sv->color(g_colors.bg);
        st->font_preview_sv->preview_set_exprs(PREVIEW_EXPRS);
    }
    if (st->color_preview_sv) {
        st->color_preview_sv->color(g_colors.bg);
        st->color_preview_sv->preview_set_exprs(COLOR_PREVIEW_EXPRS);
    }
    if (st->fmt_preview_sv) {
        st->fmt_preview_sv->color(g_colors.bg);
        st->fmt_preview_sv->preview_set_exprs(FMT_PREVIEW_EXPRS);
    }
}

static void font_btn_cb(Fl_Widget *w, void *data) {
    auto *st = static_cast<DlgState *>(data);
    open_font_picker(&st->font, w->window());
    update_preview(&st->font);
    refresh_previews(st);
}

static void size_change_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    update_preview(&st->font);
    refresh_previews(st);
}

static void fmt_change_cb(Fl_Widget *, void *data) {
    refresh_previews(static_cast<DlgState *>(data));
}

static void apply_settings(DlgState *st) {
    write_dlg_to_globals(st);
    g_input_auto_completion     = st->auto_complete_chk->value() != 0;
    g_input_auto_close_brackets = st->auto_brackets_chk->value() != 0;

    st->saved_font_id       = g_font_id;
    st->saved_font_size     = g_font_size;
    st->saved_fmt           = g_fmt_settings;
    st->saved_sep_thousands = g_sep_thousands;
    st->saved_sep_hex       = g_sep_hex;

    st->sheet->apply_font();
    st->sheet->live_eval();
    st->sheet->redraw();

    settings_save();
}

static void reset_to_defaults(DlgState *st) {
    // Font
    st->font.selected_id = FL_COURIER;
    st->font.selected_name = "Courier";
    update_font_btn(&st->font);
    st->font.size_spin->value(13);
    update_preview(&st->font);

    // Colors
    CalcyxColors def;
    colors_init_defaults(&def);
    g_colors = def;
    for (int i = 0; i < st->colors.count; i++) {
        st->colors.swatches[i]->color(*st->colors.entries[i].target);
        st->colors.swatches[i]->redraw();
    }

    // Number Format
    st->fmt_decimal_spin->value(15);
    st->fmt_exp_chk->value(1);
    st->fmt_exp_pos_spin->value(7);
    st->fmt_exp_neg_spin->value(-4);
    st->fmt_align_chk->value(1);
    st->sep_thousands_chk->value(1);
    st->sep_hex_chk->value(1);

    // Input
    st->auto_complete_chk->value(1);
    st->auto_brackets_chk->value(0);
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

    const int TAB_H = 496;
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

        int lx = 20, ly = 50, lw = 70;

        st.font.selected_id = g_font_id;
        st.font.selected_name = font_id_to_display_name(g_font_id);

        Fl_Box *lb1 = new Fl_Box(lx, ly, lw, 25, "Font:");
        style_label(lb1);
        lb1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.font_btn = new Fl_Button(lx + lw, ly, DW - lx - lw - 30, 25);
        st.font.font_btn->box(FL_DOWN_BOX);
        st.font.font_btn->color(DLG_INPUT);
        st.font.font_btn->labelcolor(DLG_TEXT);
        st.font.font_btn->labelsize(13);
        st.font.font_btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.font.font_btn->callback(font_btn_cb, &st);
        update_font_btn(&st.font);

        ly += 35;
        Fl_Box *lb2 = new Fl_Box(lx, ly, lw, 25, "Size:");
        style_label(lb2);
        lb2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.size_spin = new Fl_Spinner(lx + lw, ly, 80, 25);
        style_spinner(st.font.size_spin);
        st.font.size_spin->range(8, 36);
        st.font.size_spin->step(1);
        st.font.size_spin->value(g_font_size);
        st.font.size_spin->callback(size_change_cb, &st);

        ly += 35;
        int preview_h = TAB_H - 25 - (ly - 30);
        st.font_preview_sv = new SheetView(lx, ly, DW - 50, preview_h, true);
        st.font.preview = nullptr;

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
            st.colors.swatch_data[i] = { entries[i].color, &st };
            btn->callback(swatch_cb, &st.colors.swatch_data[i]);
            st.colors.swatches[i] = btn;
            st.colors.entries[i] = { entries[i].label, entries[i].color };
        }

        int color_bottom = sy + 8 * 30;

        // Reset ボタン
        auto *reset = new Fl_Button(20, color_bottom, 80, 25, "Reset Colors");
        reset->color(DLG_BTN);
        reset->labelcolor(DLG_TEXT);
        reset->labelsize(12);
        reset->callback([](Fl_Widget *, void *data) {
            auto *st = static_cast<DlgState *>(data);
            colors_init_defaults(&g_colors);
            for (int i = 0; i < st->colors.count; i++)
                st->colors.swatches[i]->color(*st->colors.entries[i].target);
            refresh_previews(st);
        }, &st);

        int preview_y = color_bottom + 35;
        int preview_h = TAB_H - 25 - (preview_y - 30);
        st.color_preview_sv = new SheetView(20, preview_y, DW - 50, preview_h, true);

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
        int preview_h = TAB_H - 25 - (ly - 30);
        st.fmt_preview_sv = new SheetView(lx, ly, DW - 50, preview_h, true);

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

    // プレビューの初期化
    st.saved_font_id       = g_font_id;
    st.saved_font_size     = g_font_size;
    st.saved_fmt           = g_fmt_settings;
    st.saved_sep_thousands = g_sep_thousands;
    st.saved_sep_hex       = g_sep_hex;
    refresh_previews(&st);

    // ======== OK / Cancel / Apply / Open File ========
    int by = DH - 38;
    int bw = 80, bh = 28;

    Fl_Button reset_btn(8, by, 60, bh, "Reset");
    reset_btn.color(DLG_BTN);
    reset_btn.labelcolor(DLG_LABEL);
    reset_btn.labelsize(11);
    reset_btn.tooltip("Reset all settings to defaults");
    reset_btn.callback([](Fl_Widget *, void *data) {
        if (fl_choice("Reset all settings to defaults?", "Cancel", "Reset", nullptr) == 1)
            reset_to_defaults(static_cast<DlgState *>(data));
    }, &st);

    Fl_Button open_btn(8 + 60 + 8, by, bw + 30, bh, "Open conf file");
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
        if (system(cmd.c_str())) {}
#else
        // Linux: open explicitly as text
        std::string cmd = std::string("xdg-open \"") + path + "\" 2>/dev/null &";
        const char *editor = getenv("VISUAL");
        if (!editor || !editor[0]) editor = getenv("EDITOR");
        if (editor && editor[0])
            cmd = std::string(editor) + " \"" + path + "\" &";
        if (system(cmd.c_str())) {}
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
        g_colors        = st->saved_colors;
        g_font_id       = st->saved_font_id;
        g_font_size     = st->saved_font_size;
        g_fmt_settings  = st->saved_fmt;
        g_sep_thousands = st->saved_sep_thousands;
        g_sep_hex       = st->saved_sep_hex;
        st->sheet->apply_font();
        st->sheet->live_eval();
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
