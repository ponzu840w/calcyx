// PrefsDialog タブ間で共有されるロジック (style_* / write_dlg_to_globals /
// refresh_previews / refresh_dlg_colors / swatch / apply_settings / reset).

#include "prefs_common.h"
#include "i18n.h"
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
    "sin(cos(abs(max(1, 2))))",
    "si(1.5e3 + 2.0e-4)",
    "\"hello\" + \" world\"",
    "sqrt(PI) / 0",
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
    chk->selection_color(g_colors.accent);
}

void style_spinner(Fl_Spinner *sp) {
    sp->color(DLG_INPUT);
    sp->textcolor(DLG_TEXT);
    sp->labelcolor(DLG_LABEL);
    sp->labelsize(12);
    sp->textsize(12);
    sp->selection_color(g_colors.accent);
}

// セクション枠を作る: 太字タイトル + 枠付き Fl_Group。呼び出し側で end() すること。
Fl_Group *begin_section(int x, int y, int w, int body_h, const char *title) {
    Fl_Box *label = new Fl_Box(x + 4, y, 300, SECTION_TITLE_H, title);
    label->box(FL_NO_BOX);
    label->labelcolor(DLG_LABEL);
    label->labelsize(12);
    label->labelfont(FL_BOLD);
    label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Group *g = new Fl_Group(x, y + SECTION_TITLE_H, w, body_h);
    g->box(FL_ENGRAVED_FRAME);
    g->color(DLG_BG);
    g->labelcolor(DLG_LABEL);
    g->begin();
    return g;
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
    g_start_topmost           = st->start_topmost_chk->value() != 0;
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
    /* preset = USER_DEFINED のときだけ swatch をクリック可能にする (元設計)。
     * さらに override で特定 color_* が固定されている場合はその swatch を
     * preset によらず deactivate + tooltip。 */
    bool user_def = (g_color_preset == COLOR_PRESET_USER_DEFINED);
    const auto &locked = settings_locked_keys();
    for (int i = 0; i < st->colors.count; i++) {
        const char *key = st->colors.entries[i].schema_key;
        bool is_locked = key && locked.count(key) > 0;
        Fl_Button *btn = st->colors.swatches[i];
        if (is_locked) {
            btn->deactivate();
            btn->tooltip(_("Locked by calcyx.conf.override"));
        } else if (user_def) {
            btn->activate();
            btn->tooltip(nullptr);
        } else {
            btn->deactivate();
            btn->tooltip(nullptr);
        }
    }
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
            cho->selection_color(g_colors.accent);
        } else if (auto *btn = dynamic_cast<Fl_Button *>(w)) {
            if (btn->box() == FL_DOWN_BOX) {
                btn->color(DLG_INPUT);
                btn->labelcolor(DLG_TEXT);
            } else {
                btn->color(DLG_BTN);
                btn->labelcolor(DLG_TEXT);
            }
        } else if (auto *box = dynamic_cast<Fl_Box *>(w)) {
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
    g_input_bs_delete_empty_row = st->bs_delete_empty_chk->value() != 0;
    g_popup_independent_normal  = st->popup_indep_normal_chk->value() != 0;
    g_popup_independent_compact = st->popup_indep_compact_chk->value() != 0;

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

    /* Language: 起動時のみ反映されるので、 変更を保存だけする (リスタート要)。 */
    {
        static const char *kLangIds[] = {"auto", "en", "ja"};
        int li = st->language_choice->value();
        if (li >= 0 && li < (int)(sizeof(kLangIds) / sizeof(kLangIds[0])))
            g_language = kLangIds[li];
    }

    /* user_colors は g_user_colors (グローバル) に一本化されたので
     * st->user_colors への同期は不要。 */

    /* Apply 後の状態を新たなスナップショットに。 これ以降 Cancel すれば
     * Apply 直後の値に戻る。 */
    st->saved = AppSettings::capture();

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
    // Language
    st->language_choice->value(0);  /* auto */

    // Font
    st->font.selected_id = DEFAULT_FONT_ID;
    st->font.selected_name = font_id_to_display_name(DEFAULT_FONT_ID);
    update_font_btn(&st->font);
    st->font.size_spin->value(DEFAULT_FONT_SIZE);
    update_preview(&st->font);

    // Colors
    st->preset_choice->value(DEFAULT_COLOR_PRESET);
    colors_apply_preset(DEFAULT_COLOR_PRESET);
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
    st->bs_delete_empty_chk->value(DEFAULT_BS_DELETE_EMPTY_ROW ? 1 : 0);
    st->popup_indep_normal_chk->value(DEFAULT_POPUP_INDEPENDENT_NORMAL ? 1 : 0);
    st->popup_indep_compact_chk->value(DEFAULT_POPUP_INDEPENDENT_COMPACT ? 1 : 0);

    // General
    st->show_rowlines_chk->value(DEFAULT_SHOW_ROWLINES ? 1 : 0);
    st->remember_pos_chk->value(DEFAULT_REMEMBER_POSITION ? 1 : 0);
    st->start_topmost_chk->value(DEFAULT_START_TOPMOST ? 1 : 0);
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

    // enable 系チェックに登録された sync コールバックを発火させ、
    // 依存ウィジェットのグレーアウト状態を新しい値に合わせる。
    st->hotkey_chk->do_callback();
    st->fmt_exp_chk->do_callback();

    refresh_previews(st);
}
