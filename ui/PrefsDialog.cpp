// PrefsDialog — 設定ダイアログ (5タブ: General, Appearance, Number Format, Input, System)

#include "PrefsDialog.h"
#include "SheetView.h"
#include "settings_globals.h"
#include "platform_tray.h"
#include "colors.h"
#include "app_prefs.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/filename.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
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

#define DLG_BG     g_colors.ui_bg
#define DLG_INPUT  g_colors.ui_input
#define DLG_BTN    g_colors.ui_btn
#define DLG_TEXT   g_colors.ui_text
#define DLG_LABEL  g_colors.ui_label

static const int DW = 540;
static const int DH = 600;

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
    browser.selection_color(g_colors.cursor);

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

static const int MAX_SWATCHES = 30;

struct ColorsTab {
    Fl_Button *swatches[MAX_SWATCHES];
    ColorEntry entries[MAX_SWATCHES];
    SwatchData swatch_data[MAX_SWATCHES];
    int count;
};

static void swatch_cb(Fl_Widget *w, void *data);

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
    // General tab
    Fl_Check_Button *show_rowlines_chk;
    Fl_Check_Button *remember_pos_chk;
    // System tab
    Fl_Check_Button *tray_chk;
    Fl_Check_Button *hotkey_chk;
    Fl_Check_Button *hotkey_win_chk;
    Fl_Check_Button *hotkey_alt_chk;
    Fl_Check_Button *hotkey_ctrl_chk;
    Fl_Check_Button *hotkey_shift_chk;
    Fl_Choice       *hotkey_key_choice;
    bool saved_tray_icon;
    bool saved_hotkey_enabled;
    bool saved_hotkey_win;
    bool saved_hotkey_alt;
    bool saved_hotkey_ctrl;
    bool saved_hotkey_shift;
    int  saved_hotkey_keycode;
    Fl_Spinner *limit_array_spin;
    Fl_Spinner *limit_string_spin;
    Fl_Spinner *limit_depth_spin;
    Fl_Choice  *preset_choice;
    CalcyxColors user_colors;
    SheetView  *sheet;
    CalcyxColors saved_colors;
    int         saved_preset;
    CalcyxColors saved_user_colors;
    SheetView  *font_preview_sv;
    SheetView  *color_preview_sv;
    SheetView  *fmt_preview_sv;
    Fl_Font     saved_font_id;
    int         saved_font_size;
    fmt_settings_t saved_fmt;
    bool        saved_sep_thousands;
    bool        saved_sep_hex;
    int         saved_limit_array;
    int         saved_limit_string;
    int         saved_limit_depth;
    bool        saved_show_rowlines;
    bool        saved_remember_pos;
    PrefsApplyUiCb ui_cb;
    void       *ui_data;
    Fl_Window  *dlg_win;
    Fl_Tabs    *tabs;
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

static void update_swatch_labels(DlgState *st);
static void refresh_dlg_colors(DlgState *st);
static void style_check(Fl_Check_Button *chk);
static void style_spinner(Fl_Spinner *sp);

static void swatch_cb(Fl_Widget *w, void *data) {
    auto *sd = static_cast<SwatchData *>(data);
    if (sd->dlg->preset_choice->value() != COLOR_PRESET_USER_DEFINED)
        return;
    uchar r, g, b;
    Fl::get_color(*sd->target, r, g, b);
    if (fl_color_chooser("Color", r, g, b)) {
        *sd->target = fl_rgb_color(r, g, b);
        sd->dlg->user_colors = g_colors;
        update_swatch_labels(sd->dlg);
        refresh_previews(sd->dlg);
    }
}

static Fl_Color contrast_label_color(Fl_Color c) {
    uchar r, g, b;
    Fl::get_color(c, r, g, b);
    int lum = r * 299 + g * 587 + b * 114;
    return (lum > 128000) ? FL_BLACK : FL_WHITE;
}

static void update_swatch_labels(DlgState *st) {
    for (int i = 0; i < st->colors.count; i++) {
        Fl_Color c = *st->colors.entries[i].target;
        uchar r, g, b;
        Fl::get_color(c, r, g, b);
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
        st->colors.swatches[i]->copy_label(buf);
        st->colors.swatches[i]->labelcolor(contrast_label_color(c));
        st->colors.swatches[i]->color(c);
        st->colors.swatches[i]->redraw();
    }
}

static void update_swatch_state(DlgState *st) {
    update_swatch_labels(st);
}

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
    g_limit_max_array_length  = (int)st->limit_array_spin->value();
    g_limit_max_string_length = (int)st->limit_string_spin->value();
    g_limit_max_call_depth    = (int)st->limit_depth_spin->value();
    g_show_rowlines           = st->show_rowlines_chk->value() != 0;
    g_remember_position       = st->remember_pos_chk->value() != 0;
    g_color_preset = st->preset_choice->value();
}

// ダイアログ自身のウィジェットカラーを現在の g_colors.ui_* で更新
static void refresh_dlg_colors_recurse(Fl_Group *grp, DlgState *st) {
    for (int i = 0; i < grp->children(); i++) {
        Fl_Widget *w = grp->child(i);
        // スウォッチ (色見本ボタン) はスキップ
        bool is_swatch = false;
        for (int j = 0; j < st->colors.count; j++) {
            if (w == st->colors.swatches[j]) { is_swatch = true; break; }
        }
        if (is_swatch) continue;
        // プレビュー SheetView はスキップ (独自の色管理)
        if (w == st->color_preview_sv || w == st->font_preview_sv || w == st->fmt_preview_sv)
            continue;

        if (auto *chk = dynamic_cast<Fl_Check_Button *>(w)) {
            style_check(chk);
        } else if (auto *sp = dynamic_cast<Fl_Spinner *>(w)) {
            style_spinner(sp);
            // Fl_Spinner の子ウィジェットも更新
            if (auto *sg = dynamic_cast<Fl_Group *>(w)) {
                for (int j = 0; j < sg->children(); j++) {
                    Fl_Widget *c = sg->child(j);
                    c->color(DLG_INPUT);
                    c->labelcolor(DLG_TEXT);
                    c->redraw();
                }
            }
        } else if (auto *cho = dynamic_cast<Fl_Choice *>(w)) {
            cho->color(DLG_INPUT);
            cho->textcolor(DLG_TEXT);
            cho->labelcolor(DLG_LABEL);
            cho->selection_color(g_colors.cursor);
        } else if (auto *btn = dynamic_cast<Fl_Button *>(w)) {
            if (btn->box() == FL_DOWN_BOX) {
                // フォント選択ボタン等
                btn->color(DLG_INPUT);
                btn->labelcolor(DLG_TEXT);
            } else {
                btn->color(DLG_BTN);
                btn->labelcolor(DLG_TEXT);
            }
        } else if (auto *box = dynamic_cast<Fl_Box *>(w)) {
            if (box->labelfont() == FL_BOLD)
                box->labelcolor(DLG_LABEL);
            else
                box->labelcolor(DLG_LABEL);
        }

        if (auto *g = dynamic_cast<Fl_Group *>(w)) {
            g->color(DLG_BG);
            g->selection_color(DLG_BG);
            if (dynamic_cast<Fl_Tabs *>(w))
                g->labelcolor(DLG_LABEL);
            else
                g->labelcolor(DLG_TEXT);
            refresh_dlg_colors_recurse(g, st);
        }
        w->redraw();
    }
}

static void refresh_dlg_colors(DlgState *st) {
    if (!st->dlg_win) return;
    colors_apply_fl_scheme();
    st->dlg_win->color(DLG_BG);
    if (st->tabs) {
        st->tabs->color(DLG_BG);
        st->tabs->selection_color(DLG_BG);
        st->tabs->labelcolor(DLG_LABEL);
    }
    refresh_dlg_colors_recurse(static_cast<Fl_Group *>(st->dlg_win), st);
    update_swatch_labels(st);
    st->dlg_win->redraw();
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

static void preset_change_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    int preset = st->preset_choice->value();
    if (preset == COLOR_PRESET_USER_DEFINED) {
        g_colors = st->user_colors;
    } else {
        colors_init_preset(&g_colors, preset);
    }
    update_swatch_state(st);
    refresh_dlg_colors(st);
    refresh_previews(st);
}

static void apply_settings(DlgState *st) {
    write_dlg_to_globals(st);
    g_input_auto_completion     = st->auto_complete_chk->value() != 0;
    g_input_auto_close_brackets = st->auto_brackets_chk->value() != 0;

    // System tab
    g_tray_icon      = st->tray_chk->value() != 0;
    g_hotkey_enabled = st->hotkey_chk->value() != 0;
    g_hotkey_win     = st->hotkey_win_chk->value() != 0;
    g_hotkey_alt     = st->hotkey_alt_chk->value() != 0;
    g_hotkey_ctrl    = st->hotkey_ctrl_chk->value() != 0;
    g_hotkey_shift   = st->hotkey_shift_chk->value() != 0;
    {
        int ki = st->hotkey_key_choice->value();
        const char *const *names = plat_key_names();
        if (ki >= 0 && ki < plat_key_names_count())
            g_hotkey_keycode = plat_keyname_to_flkey(names[ki]);
    }

    if (g_color_preset == COLOR_PRESET_USER_DEFINED)
        st->user_colors = g_colors;

    st->saved_font_id       = g_font_id;
    st->saved_font_size     = g_font_size;
    st->saved_fmt           = g_fmt_settings;
    st->saved_sep_thousands = g_sep_thousands;
    st->saved_sep_hex       = g_sep_hex;
    st->saved_limit_array   = g_limit_max_array_length;
    st->saved_limit_string  = g_limit_max_string_length;
    st->saved_limit_depth   = g_limit_max_call_depth;
    st->saved_show_rowlines   = g_show_rowlines;
    st->saved_remember_pos    = g_remember_position;
    st->saved_preset          = g_color_preset;
    st->saved_colors          = g_colors;
    st->saved_user_colors     = st->user_colors;
    st->saved_tray_icon       = g_tray_icon;
    st->saved_hotkey_enabled  = g_hotkey_enabled;
    st->saved_hotkey_win      = g_hotkey_win;
    st->saved_hotkey_alt      = g_hotkey_alt;
    st->saved_hotkey_ctrl     = g_hotkey_ctrl;
    st->saved_hotkey_shift    = g_hotkey_shift;
    st->saved_hotkey_keycode  = g_hotkey_keycode;

    st->sheet->apply_font();
    st->sheet->live_eval();
    st->sheet->redraw();

    if (st->ui_cb) st->ui_cb(st->ui_data);

    // ダイアログ自身のカラーも更新
    refresh_dlg_colors(st);

    settings_save();
}

static void reset_to_defaults(DlgState *st) {
    // Font
    st->font.selected_id = DEFAULT_FONT_ID;
    st->font.selected_name = font_id_to_display_name(DEFAULT_FONT_ID);
    update_font_btn(&st->font);
    st->font.size_spin->value(DEFAULT_FONT_SIZE);
    update_preview(&st->font);

    // Colors
    st->preset_choice->value(DEFAULT_COLOR_PRESET);
    colors_init_preset(&g_colors, DEFAULT_COLOR_PRESET);
    for (int i = 0; i < st->colors.count; i++) {
        st->colors.swatches[i]->color(*st->colors.entries[i].target);
        st->colors.swatches[i]->redraw();
    }
    update_swatch_state(st);

    // Number Format
    st->fmt_decimal_spin->value(DEFAULT_FMT_DECIMAL_LEN);
    st->fmt_exp_chk->value(DEFAULT_FMT_E_NOTATION ? 1 : 0);
    st->fmt_exp_pos_spin->value(DEFAULT_FMT_E_POSITIVE_MIN);
    st->fmt_exp_neg_spin->value(DEFAULT_FMT_E_NEGATIVE_MAX);
    st->fmt_align_chk->value(DEFAULT_FMT_E_ALIGNMENT ? 1 : 0);
    st->sep_thousands_chk->value(DEFAULT_SEP_THOUSANDS ? 1 : 0);
    st->sep_hex_chk->value(DEFAULT_SEP_HEX ? 1 : 0);

    // Input
    st->auto_complete_chk->value(DEFAULT_AUTO_COMPLETION ? 1 : 0);
    st->auto_brackets_chk->value(DEFAULT_AUTO_CLOSE_BRACKETS ? 1 : 0);

    // General
    st->show_rowlines_chk->value(DEFAULT_SHOW_ROWLINES ? 1 : 0);
    st->remember_pos_chk->value(DEFAULT_REMEMBER_POSITION ? 1 : 0);
    st->limit_array_spin->value(DEFAULT_MAX_ARRAY_LENGTH);
    st->limit_string_spin->value(DEFAULT_MAX_STRING_LENGTH);
    st->limit_depth_spin->value(DEFAULT_MAX_CALL_DEPTH);

    // System
    st->tray_chk->value(DEFAULT_TRAY_ICON ? 1 : 0);
    st->hotkey_chk->value(DEFAULT_HOTKEY_ENABLED ? 1 : 0);
    st->hotkey_win_chk->value(DEFAULT_HOTKEY_WIN ? 1 : 0);
    st->hotkey_alt_chk->value(DEFAULT_HOTKEY_ALT ? 1 : 0);
    st->hotkey_ctrl_chk->value(DEFAULT_HOTKEY_CTRL ? 1 : 0);
    st->hotkey_shift_chk->value(DEFAULT_HOTKEY_SHIFT ? 1 : 0);
    {
        const char *def_name = plat_flkey_to_keyname(DEFAULT_HOTKEY_KEYCODE);
        int idx = 0;
        int n = plat_key_names_count();
        const char *const *names = plat_key_names();
        for (int i = 0; i < n; i++) {
            if (strcmp(names[i], def_name) == 0) { idx = i; break; }
        }
        st->hotkey_key_choice->value(idx);
    }

    refresh_previews(st);
}

static void style_label(Fl_Widget *w) {
    w->labelcolor(DLG_LABEL);
    w->labelsize(12);
}

static void style_check(Fl_Check_Button *chk) {
    chk->color(DLG_BG);
    chk->labelcolor(DLG_TEXT);
    chk->labelsize(12);
    chk->selection_color(g_colors.cursor);
}

static void style_spinner(Fl_Spinner *sp) {
    sp->color(DLG_INPUT);
    sp->textcolor(DLG_TEXT);
    sp->labelcolor(DLG_LABEL);
    sp->labelsize(12);
    sp->textsize(12);
    sp->selection_color(g_colors.cursor);
}

void PrefsDialog::run(SheetView *sheet, PrefsApplyUiCb ui_cb, void *ui_data) {
    DlgState st;
    st.sheet = sheet;
    st.ui_cb = ui_cb;
    st.ui_data = ui_data;
    st.saved_colors = g_colors;
    st.saved_preset = g_color_preset;
    st.user_colors = g_colors;
    st.saved_user_colors = g_colors;

    Fl_Double_Window dlg(DW, DH, "Preferences");
    dlg.set_modal();
    dlg.color(DLG_BG);
    st.dlg_win = &dlg;

    const int TAB_H = DH - 60;
    Fl_Tabs tabs(5, 5, DW - 10, TAB_H);
    tabs.color(DLG_BG);
    tabs.labelcolor(DLG_LABEL);
    tabs.selection_color(DLG_BG);
    st.tabs = &tabs;

    // ======== General tab ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " General ");
        g->color(DLG_BG);
        g->selection_color(DLG_BG);
        g->labelcolor(DLG_TEXT);
        g->labelsize(12);

        int lx = 20, ly = 55;

        // --- Window ---
        Fl_Box *sec_win = new Fl_Box(lx, ly, 200, 20, "Window");
        sec_win->labelcolor(DLG_LABEL);
        sec_win->labelsize(12);
        sec_win->labelfont(FL_BOLD);
        sec_win->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        st.remember_pos_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Remember window position on exit");
        style_check(st.remember_pos_chk);
        st.remember_pos_chk->value(g_remember_position ? 1 : 0);
        ly += 40;

        // --- Calculation Limits ---
        Fl_Box *sec_lim = new Fl_Box(lx, ly, 200, 20, "Calculation Limits");
        sec_lim->labelcolor(DLG_LABEL);
        sec_lim->labelsize(12);
        sec_lim->labelfont(FL_BOLD);
        sec_lim->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        int slx = lx + 10, slw = 190, ssw = 100;

        Fl_Box *lb_arr = new Fl_Box(slx, ly, slw, 25, "Max array length:");
        style_label(lb_arr);
        lb_arr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.limit_array_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
        style_spinner(st.limit_array_spin);
        st.limit_array_spin->range(1, 1000000);
        st.limit_array_spin->step(1);
        st.limit_array_spin->value(g_limit_max_array_length);
        ly += 30;

        Fl_Box *lb_str = new Fl_Box(slx, ly, slw, 25, "Max string length:");
        style_label(lb_str);
        lb_str->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.limit_string_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
        style_spinner(st.limit_string_spin);
        st.limit_string_spin->range(1, 1000000);
        st.limit_string_spin->step(1);
        st.limit_string_spin->value(g_limit_max_string_length);
        ly += 30;

        Fl_Box *lb_dep = new Fl_Box(slx, ly, slw, 25, "Max call recursion depth:");
        style_label(lb_dep);
        lb_dep->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.limit_depth_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
        style_spinner(st.limit_depth_spin);
        st.limit_depth_spin->range(1, 1000);
        st.limit_depth_spin->step(1);
        st.limit_depth_spin->value(g_limit_max_call_depth);
        ly += 40;

        // --- Configuration ---
        Fl_Box *sec_cfg = new Fl_Box(lx, ly, 200, 20, "Configuration");
        sec_cfg->labelcolor(DLG_LABEL);
        sec_cfg->labelsize(12);
        sec_cfg->labelfont(FL_BOLD);
        sec_cfg->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        std::string cfg_dir = AppPrefs::config_dir();
        Fl_Box *path_box = new Fl_Box(slx, ly, DW - 60, 20);
        path_box->copy_label(cfg_dir.c_str());
        path_box->labelcolor(DLG_TEXT);
        path_box->labelsize(11);
        path_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        Fl_Button *open_dir_btn = new Fl_Button(slx, ly, 120, 28, "Open folder");
        open_dir_btn->color(DLG_BTN);
        open_dir_btn->labelcolor(DLG_TEXT);
        open_dir_btn->labelsize(12);
        open_dir_btn->callback([](Fl_Widget *, void *) {
            std::string dir = AppPrefs::config_dir();
#if defined(_WIN32)
            ShellExecuteA(NULL, "open", dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
            std::string cmd = "open \"" + dir + "\"";
            if (system(cmd.c_str())) {}
#else
            std::string cmd = "xdg-open \"" + dir + "\" 2>/dev/null &";
            if (system(cmd.c_str())) {}
#endif
        }, nullptr);

        g->end();
    }

    // ======== Appearance tab (Font + Colors) ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " Appearance ");
        g->color(DLG_BG);
        g->selection_color(DLG_BG);
        g->labelcolor(DLG_TEXT);
        g->labelsize(12);

        int lx = 20, ly = 50;

        // --- Font ---
        Fl_Box *sec_font = new Fl_Box(lx, ly, 200, 20, "Font");
        sec_font->labelcolor(DLG_LABEL);
        sec_font->labelsize(12);
        sec_font->labelfont(FL_BOLD);
        sec_font->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 22;

        int lw = 50;
        st.font.selected_id = g_font_id;
        st.font.selected_name = font_id_to_display_name(g_font_id);

        Fl_Box *lb1 = new Fl_Box(lx + 10, ly, lw, 25, "Font:");
        style_label(lb1);
        lb1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.font_btn = new Fl_Button(lx + 10 + lw, ly, DW - lx - lw - 50, 25);
        st.font.font_btn->box(FL_DOWN_BOX);
        st.font.font_btn->color(DLG_INPUT);
        st.font.font_btn->labelcolor(DLG_TEXT);
        st.font.font_btn->labelsize(13);
        st.font.font_btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.font.font_btn->callback(font_btn_cb, &st);
        update_font_btn(&st.font);

        ly += 28;
        Fl_Box *lb2 = new Fl_Box(lx + 10, ly, lw, 25, "Size:");
        style_label(lb2);
        lb2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.font.size_spin = new Fl_Spinner(lx + 10 + lw, ly, 80, 25);
        style_spinner(st.font.size_spin);
        st.font.size_spin->range(8, 36);
        st.font.size_spin->step(1);
        st.font.size_spin->value(g_font_size);
        st.font.size_spin->callback(size_change_cb, &st);
        ly += 30;

        // --- Colors ---
        Fl_Box *sec_col = new Fl_Box(lx, ly, 200, 20, "Colors");
        sec_col->labelcolor(DLG_LABEL);
        sec_col->labelsize(12);
        sec_col->labelfont(FL_BOLD);
        sec_col->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 22;

        Fl_Box *lb_preset = new Fl_Box(lx + 10, ly, 60, 25, "Preset:");
        style_label(lb_preset);
        lb_preset->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        st.preset_choice = new Fl_Choice(lx + 70, ly, 200, 25);
        st.preset_choice->color(DLG_INPUT);
        st.preset_choice->textcolor(DLG_TEXT);
        st.preset_choice->labelsize(12);
        st.preset_choice->textsize(12);
        st.preset_choice->selection_color(g_colors.cursor);
        for (int i = 0; i < COLOR_PRESET_COUNT; i++)
            st.preset_choice->add(COLOR_PRESET_INFO[i].label);
        st.preset_choice->value(g_color_preset);
        st.preset_choice->callback(preset_change_cb, &st);
        ly += 28;

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
            { "Win BG",       &g_colors.ui_win_bg },
            { "Dlg BG",       &g_colors.ui_bg },
            { "UI Input",     &g_colors.ui_input },
            { "UI Button",    &g_colors.ui_btn },
            { "Menu BG",      &g_colors.ui_menu },
            { "UI Text",      &g_colors.ui_text },
            { "UI Label",     &g_colors.ui_label },
            { "UI Dim",       &g_colors.ui_dim },
            { "Popup BG",     &g_colors.pop_bg },
            { "Popup Sel",    &g_colors.pop_sel },
            { "Popup Text",   &g_colors.pop_text },
            { "Popup Desc",   &g_colors.pop_desc },
            { "Popup DescBG", &g_colors.pop_desc_bg },
            { "Popup Border", &g_colors.pop_border },
        };

        const int n_entries = 29;
        st.colors.count = n_entries;
        // 3カラム × 10行
        const int cols = 3, rows = 10;
        int col_w = (DW - 40) / cols;
        int sy = ly;
        for (int i = 0; i < n_entries; i++) {
            int c = i / rows, r = i % rows;
            int cx = lx + 10 + c * col_w;
            int cy = sy + r * 24;
            Fl_Box *lb = new Fl_Box(cx, cy, 70, 20, entries[i].label);
            style_label(lb);
            lb->labelsize(11);
            lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

            auto *btn = new Fl_Button(cx + 70, cy, 80, 20);
            btn->box(FL_DOWN_BOX);
            btn->color(*entries[i].color);
            btn->labelsize(10);
            btn->align(FL_ALIGN_INSIDE);
            st.colors.swatch_data[i] = { entries[i].color, &st };
            btn->callback(swatch_cb, &st.colors.swatch_data[i]);
            st.colors.swatches[i] = btn;
            st.colors.entries[i] = { entries[i].label, entries[i].color };
        }

        ly = sy + rows * 24 + 4;

        st.show_rowlines_chk = new Fl_Check_Button(lx + 10, ly, 300, 22, "Show row separator lines");
        style_check(st.show_rowlines_chk);
        st.show_rowlines_chk->value(g_show_rowlines ? 1 : 0);
        st.show_rowlines_chk->callback([](Fl_Widget *, void *data) {
            auto *st = static_cast<DlgState *>(data);
            g_show_rowlines = st->show_rowlines_chk->value() != 0;
            refresh_previews(st);
        }, &st);
        ly += 26;

        // --- Preview ---
        int preview_h = TAB_H - 25 - (ly - 30);
        st.font_preview_sv = nullptr;
        st.color_preview_sv = new SheetView(lx, ly, DW - 50, preview_h, true);
        st.font.preview = nullptr;

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

    // ======== System tab (Tray + Hotkey) ========
    {
        Fl_Group *g = new Fl_Group(5, 30, DW - 10, TAB_H - 25, " System ");
        g->color(DLG_BG);
        g->selection_color(DLG_BG);
        g->labelcolor(DLG_TEXT);
        g->labelsize(12);

        int lx = 20, ly = 55;

        // --- System Tray ---
        Fl_Box *sec_tray = new Fl_Box(lx, ly, 200, 20, "System Tray");
        sec_tray->labelcolor(DLG_LABEL);
        sec_tray->labelsize(12);
        sec_tray->labelfont(FL_BOLD);
        sec_tray->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        st.tray_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Enable system tray icon");
        style_check(st.tray_chk);
        st.tray_chk->value(g_tray_icon ? 1 : 0);
        ly += 25;

        Fl_Box *tray_note = new Fl_Box(lx + 30, ly, 400, 20, "When enabled, closing the window minimizes to tray.");
        tray_note->labelcolor(DLG_LABEL);
        tray_note->labelsize(11);
        tray_note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 35;

        // --- Global Hotkey ---
        Fl_Box *sec_hk = new Fl_Box(lx, ly, 200, 20, "Global Hotkey");
        sec_hk->labelcolor(DLG_LABEL);
        sec_hk->labelsize(12);
        sec_hk->labelfont(FL_BOLD);
        sec_hk->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ly += 25;

        st.hotkey_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Enable global hotkey");
        style_check(st.hotkey_chk);
        st.hotkey_chk->value(g_hotkey_enabled ? 1 : 0);
        ly += 30;

        // 修飾キーチェックボックス (横並び)
        Fl_Box *mod_label = new Fl_Box(lx + 30, ly, 80, 25, "Modifiers:");
        style_label(mod_label);
        mod_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        int mx = lx + 110;
        st.hotkey_win_chk = new Fl_Check_Button(mx, ly, 55, 25, "Win");
        style_check(st.hotkey_win_chk);
        st.hotkey_win_chk->value(g_hotkey_win ? 1 : 0);
        mx += 55;
        st.hotkey_alt_chk = new Fl_Check_Button(mx, ly, 50, 25, "Alt");
        style_check(st.hotkey_alt_chk);
        st.hotkey_alt_chk->value(g_hotkey_alt ? 1 : 0);
        mx += 50;
        st.hotkey_ctrl_chk = new Fl_Check_Button(mx, ly, 55, 25, "Ctrl");
        style_check(st.hotkey_ctrl_chk);
        st.hotkey_ctrl_chk->value(g_hotkey_ctrl ? 1 : 0);
        mx += 55;
        st.hotkey_shift_chk = new Fl_Check_Button(mx, ly, 60, 25, "Shift");
        style_check(st.hotkey_shift_chk);
        st.hotkey_shift_chk->value(g_hotkey_shift ? 1 : 0);
        ly += 30;

        // キー選択
        Fl_Box *key_label = new Fl_Box(lx + 30, ly, 80, 25, "Key:");
        style_label(key_label);
        key_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.hotkey_key_choice = new Fl_Choice(lx + 110, ly, 100, 25);
        st.hotkey_key_choice->color(DLG_INPUT);
        st.hotkey_key_choice->textcolor(DLG_TEXT);
        st.hotkey_key_choice->labelcolor(DLG_LABEL);
        st.hotkey_key_choice->textsize(12);
        int key_count = plat_key_names_count();
        const char *const *key_names = plat_key_names();
        int sel_idx = 0;
        for (int i = 0; i < key_count; i++) {
            st.hotkey_key_choice->add(key_names[i]);
            if (plat_keyname_to_flkey(key_names[i]) == g_hotkey_keycode)
                sel_idx = i;
        }
        st.hotkey_key_choice->value(sel_idx);

        g->end();
    }

    tabs.end();

    // プレビューの初期化
    st.saved_font_id       = g_font_id;
    st.saved_font_size     = g_font_size;
    st.saved_fmt           = g_fmt_settings;
    st.saved_sep_thousands = g_sep_thousands;
    st.saved_sep_hex       = g_sep_hex;
    st.saved_limit_array   = g_limit_max_array_length;
    st.saved_limit_string  = g_limit_max_string_length;
    st.saved_limit_depth   = g_limit_max_call_depth;
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

    // ======== OK / Cancel / Apply / Open File ========
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
