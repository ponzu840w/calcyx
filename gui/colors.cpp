// colors.cpp — CalcyxColors プリセットとデフォルト値
//
// プリセット定義は shared/color_presets.{h,c} に一本化されている (TUI と
// 共有するため)。ここでは shared の RGB パレットを Fl_Color に変換するだけ。

#include "colors.h"
#include "AppSettings.h"
#include <FL/fl_draw.H>

extern "C" {
#include "color_presets.h"
}

/* 実体は g_settings 内. 既存コードのため参照で再公開している. */
CalcyxColors &g_colors       = g_settings.colors;
CalcyxColors &g_user_colors  = g_settings.user_colors;
int          &g_color_preset = g_settings.color_preset;

const ColorPresetInfo COLOR_PRESET_INFO[COLOR_PRESET_COUNT] = {
    { "otaku-black",     "otaku-black"     },
    { "gyakubari-white", "gyakubari-white" },
    { "saboten-grey",    "saboten-grey"    },
    { "saboten-white",   "saboten-white"   },
    { "user",            "User defined"    },
};

static Fl_Color rgb_to_fl(const calcyx_rgb_t &c) {
    return fl_rgb_color(c.r, c.g, c.b);
}

void colors_init_preset(CalcyxColors *c, int preset) {
    calcyx_color_palette_t pal;
    /* enum 値は ui/colors.h と shared/color_presets.h で一致しているのでそのまま渡す */
    calcyx_color_preset_get(preset == COLOR_PRESET_USER_DEFINED
                                ? CALCYX_COLOR_PRESET_OTAKU_BLACK
                                : preset,
                            &pal);

    c->bg          = rgb_to_fl(pal.bg);
    c->sel_bg      = rgb_to_fl(pal.sel_bg);
    c->rowline     = rgb_to_fl(pal.rowline);
    c->text        = rgb_to_fl(pal.text);
    c->accent      = rgb_to_fl(pal.accent);
    c->symbol      = rgb_to_fl(pal.symbol);
    c->ident       = rgb_to_fl(pal.ident);
    c->special     = rgb_to_fl(pal.special);
    c->si_pfx      = rgb_to_fl(pal.si_pfx);
    for (int i = 0; i < 4; i++) c->paren[i] = rgb_to_fl(pal.paren[i]);
    c->error       = rgb_to_fl(pal.error);
    c->ui_win_bg   = rgb_to_fl(pal.ui_win_bg);
    c->ui_bg       = rgb_to_fl(pal.ui_bg);
    c->ui_input    = rgb_to_fl(pal.ui_input);
    c->ui_btn      = rgb_to_fl(pal.ui_btn);
    c->ui_menu     = rgb_to_fl(pal.ui_menu);
    c->ui_text     = rgb_to_fl(pal.ui_text);
    c->ui_label    = rgb_to_fl(pal.ui_label);
    c->pop_bg      = rgb_to_fl(pal.pop_bg);
    c->pop_sel     = rgb_to_fl(pal.pop_sel);
    c->pop_text    = rgb_to_fl(pal.pop_text);
    c->pop_desc    = rgb_to_fl(pal.pop_desc);
    c->pop_desc_bg = rgb_to_fl(pal.pop_desc_bg);
    c->pop_border  = rgb_to_fl(pal.pop_border);
    // 無効化時文字 (Undo/Redo のグレーアウト等): text と menu 背景の中間色
    c->ui_dim = fl_color_average(c->ui_text, c->ui_menu, 0.5f);
}

void colors_init_defaults(CalcyxColors *c) {
    colors_init_preset(c, COLOR_PRESET_OTAKU_BLACK);
}

void colors_apply_preset(int preset) {
    g_color_preset = preset;
    if (preset == COLOR_PRESET_USER_DEFINED) {
        g_colors = g_user_colors;
    } else {
        colors_init_preset(&g_colors, preset);
    }
}

void colors_apply_fl_scheme() {
    uchar r, g, b;
    // ウィジェット背景 → FL_BACKGROUND_COLOR → FL_DARK1..3, FL_LIGHT1..3 を派生
    Fl::get_color(g_colors.ui_bg, r, g, b);
    Fl::background(r, g, b);

    // FLTK のデフォルト派生はダークテーマで FL_LIGHT が明るすぎるため手動補正
    int avg = ((int)r + g + b) / 3;
    if (avg < 100) {
        // ダークテーマ: セパレータやフレームが自然になるよう控えめに派生
        Fl::set_color(FL_DARK1,  fl_color_average(g_colors.ui_bg, FL_BLACK, 0.75f));
        Fl::set_color(FL_DARK2,  fl_color_average(g_colors.ui_bg, FL_BLACK, 0.55f));
        Fl::set_color(FL_DARK3,  fl_color_average(g_colors.ui_bg, FL_BLACK, 0.35f));
        Fl::set_color(FL_LIGHT1, fl_color_average(g_colors.ui_bg, FL_WHITE, 0.90f));
        Fl::set_color(FL_LIGHT2, fl_color_average(g_colors.ui_bg, FL_WHITE, 0.80f));
        Fl::set_color(FL_LIGHT3, fl_color_average(g_colors.ui_bg, FL_WHITE, 0.65f));
    }

    // テキスト → FL_FOREGROUND_COLOR
    Fl::get_color(g_colors.ui_text, r, g, b);
    Fl::foreground(r, g, b);
    // 入力欄背景 → FL_BACKGROUND2_COLOR
    Fl::get_color(g_colors.ui_input, r, g, b);
    Fl::background2(r, g, b);
    // 選択色
    Fl::set_color(FL_SELECTION_COLOR, g_colors.accent);
}
