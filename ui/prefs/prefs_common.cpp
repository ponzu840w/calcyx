// prefs_common.cpp — PrefsDialog タブ間で共有されるロジック
//
// ・style_* ヘルパー
// ・DlgState → globals 反映 (write_dlg_to_globals)
// ・プレビュー更新 (refresh_previews)
// ・ダイアログ色の反映 (refresh_dlg_colors)
// ・swatch 表示更新 (update_swatch_labels / update_swatch_state)
// ・OK / Apply / Reset 動作 (apply_settings / reset_to_defaults)

#include "prefs_common.h"
#include "SheetView.h"
#include "settings_globals.h"
#include "platform_tray.h"
#include <FL/Fl_Tabs.H>
#include <cstring>
#include <cstdio>

extern "C" {
#include "types/val.h"
}

// ---- プレビュー用の式 ----
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

// ---- スタイル補助 ----
void style_label(Fl_Widget *w) {
    w->labelcolor(DLG_LABEL);
    w->labelsize(12);
}

void style_check(Fl_Check_Button *chk) {
    chk->color(DLG_BG);
    chk->labelcolor(DLG_TEXT);
    chk->labelsize(12);
    chk->selection_color(g_colors.cursor);
}

void style_spinner(Fl_Spinner *sp) {
    sp->color(DLG_INPUT);
    sp->textcolor(DLG_TEXT);
    sp->labelcolor(DLG_LABEL);
    sp->labelsize(12);
    sp->textsize(12);
    sp->selection_color(g_colors.cursor);
}

// ---- DlgState → グローバル反映 ----
void write_dlg_to_globals(DlgState *st) {
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

// ---- プレビュー更新 ----
void refresh_previews(DlgState *st) {
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

// ---- swatch 表示更新 ----
static Fl_Color contrast_label_color(Fl_Color c) {
    uchar r, g, b;
    Fl::get_color(c, r, g, b);
    int lum = r * 299 + g * 587 + b * 114;
    return (lum > 128000) ? FL_BLACK : FL_WHITE;
}

void update_swatch_labels(DlgState *st) {
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

void update_swatch_state(DlgState *st) {
    update_swatch_labels(st);
}

// ---- ダイアログ自身のウィジェットカラーを現在の g_colors.ui_* で更新 ----
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

void refresh_dlg_colors(DlgState *st) {
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

// ---- Apply / OK ----
void apply_settings(DlgState *st) {
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

// ---- Reset to defaults ----
void reset_to_defaults(DlgState *st) {
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
