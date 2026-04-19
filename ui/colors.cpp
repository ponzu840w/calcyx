// colors.cpp — CalcyxColors プリセットとデフォルト値

#include "colors.h"
#include <FL/fl_draw.H>

CalcyxColors g_colors;
int g_color_preset = COLOR_PRESET_OTAKU_BLACK;

const ColorPresetInfo COLOR_PRESET_INFO[COLOR_PRESET_COUNT] = {
    { "otaku-black",     "otaku-black"     },
    { "gyakubari-white", "gyakubari-white" },
    { "saboten-grey",    "saboten-grey"    },
    { "saboten-white",   "saboten-white"   },
    { "user",            "User defined"    },
};

static void init_otaku_black(CalcyxColors *c) {
    c->bg         = fl_rgb_color( 22,  22,  22);
    c->sel_bg     = fl_rgb_color( 38,  42,  55);
    c->rowline    = fl_rgb_color( 32,  32,  36);
    c->sep        = fl_rgb_color( 55,  55,  65);
    c->text       = fl_rgb_color(255, 255, 255);
    c->cursor     = fl_rgb_color(180, 200, 255);
    c->symbol     = fl_rgb_color( 64, 192, 255);
    c->ident      = fl_rgb_color(192, 255, 128);
    c->special    = fl_rgb_color(255, 192,  64);
    c->si_pfx     = fl_rgb_color(224, 160, 255);
    c->paren[0]   = fl_rgb_color( 64, 192, 255);
    c->paren[1]   = fl_rgb_color(192, 128, 255);
    c->paren[2]   = fl_rgb_color(255, 128, 192);
    c->paren[3]   = fl_rgb_color(255, 192,  64);
    c->error      = fl_rgb_color(110, 110, 110);
    c->ui_win_bg  = fl_rgb_color( 30,  30,  30);
    c->ui_bg      = fl_rgb_color( 38,  38,  43);
    c->ui_input   = fl_rgb_color( 50,  52,  60);
    c->ui_btn     = fl_rgb_color( 55,  60,  75);
    c->ui_menu    = fl_rgb_color( 40,  40,  45);
    c->ui_text    = fl_rgb_color(215, 215, 225);
    c->ui_label   = fl_rgb_color(180, 180, 190);
    c->ui_dim     = fl_rgb_color(110, 110, 115);
    c->pop_bg     = fl_rgb_color( 28,  28,  35);
    c->pop_sel    = fl_rgb_color( 40,  80, 140);
    c->pop_text   = fl_rgb_color(220, 220, 220);
    c->pop_desc   = fl_rgb_color(150, 150, 160);
    c->pop_desc_bg= fl_rgb_color( 20,  20,  28);
    c->pop_border = fl_rgb_color( 80,  80, 100);
}

static void init_gyakubari_white(CalcyxColors *c) {
    c->bg         = fl_rgb_color(250, 250, 250);
    c->sel_bg     = fl_rgb_color(210, 220, 240);
    c->rowline    = fl_rgb_color(230, 230, 232);
    c->sep        = fl_rgb_color(200, 200, 210);
    c->text       = fl_rgb_color(  0,   0,   0);
    c->cursor     = fl_rgb_color( 40,  60, 160);
    c->symbol     = fl_rgb_color(  0, 100, 200);
    c->ident      = fl_rgb_color( 40, 130,   0);
    c->special    = fl_rgb_color(180, 100,   0);
    c->si_pfx     = fl_rgb_color(140,  60, 200);
    c->paren[0]   = fl_rgb_color(  0, 100, 200);
    c->paren[1]   = fl_rgb_color(140,  60, 200);
    c->paren[2]   = fl_rgb_color(200,  50, 120);
    c->paren[3]   = fl_rgb_color(180, 100,   0);
    c->error      = fl_rgb_color(160, 160, 160);
    c->ui_win_bg  = fl_rgb_color(240, 240, 245);
    c->ui_bg      = fl_rgb_color(235, 235, 240);
    c->ui_input   = fl_rgb_color(255, 255, 255);
    c->ui_btn     = fl_rgb_color(220, 220, 228);
    c->ui_menu    = fl_rgb_color(228, 228, 235);
    c->ui_text    = fl_rgb_color( 30,  30,  35);
    c->ui_label   = fl_rgb_color( 80,  80,  90);
    c->ui_dim     = fl_rgb_color(190, 190, 195);
    c->pop_bg     = fl_rgb_color(255, 255, 255);
    c->pop_sel    = fl_rgb_color(180, 210, 255);
    c->pop_text   = fl_rgb_color( 20,  20,  25);
    c->pop_desc   = fl_rgb_color( 90,  90, 100);
    c->pop_desc_bg= fl_rgb_color(242, 242, 248);
    c->pop_border = fl_rgb_color(180, 180, 200);
}

static void init_saboten_grey(CalcyxColors *c) {
    c->bg         = fl_rgb_color( 32,  32,  32);
    c->sel_bg     = fl_rgb_color(  0,   0,   0);
    c->rowline    = fl_rgb_color( 40,  40,  40);
    c->sep        = fl_rgb_color( 96,  96,  96);
    c->text       = fl_rgb_color(255, 255, 255);
    c->cursor     = fl_rgb_color(  0, 128, 255);
    c->symbol     = fl_rgb_color( 64, 192, 255);
    c->ident      = fl_rgb_color(192, 255, 128);
    c->special    = fl_rgb_color(255, 192,  64);
    c->si_pfx     = fl_rgb_color(224, 160, 255);
    c->paren[0]   = fl_rgb_color( 64, 192, 255);
    c->paren[1]   = fl_rgb_color(192, 128, 255);
    c->paren[2]   = fl_rgb_color(255, 128, 192);
    c->paren[3]   = fl_rgb_color(255, 192,  64);
    c->error      = fl_rgb_color(255, 128, 128);
    c->ui_win_bg  = fl_rgb_color( 40,  40,  40);
    c->ui_bg      = fl_rgb_color( 48,  48,  48);
    c->ui_input   = fl_rgb_color( 60,  60,  60);
    c->ui_btn     = fl_rgb_color( 72,  72,  80);
    c->ui_menu    = fl_rgb_color( 50,  50,  55);
    c->ui_text    = fl_rgb_color(240, 240, 240);
    c->ui_label   = fl_rgb_color(180, 180, 180);
    c->ui_dim     = fl_rgb_color(110, 110, 110);
    c->pop_bg     = fl_rgb_color( 38,  38,  42);
    c->pop_sel    = fl_rgb_color( 50,  85, 140);
    c->pop_text   = fl_rgb_color(230, 230, 230);
    c->pop_desc   = fl_rgb_color(150, 150, 155);
    c->pop_desc_bg= fl_rgb_color( 28,  28,  32);
    c->pop_border = fl_rgb_color( 90,  90,  96);
}

static void init_saboten_white(CalcyxColors *c) {
    c->bg         = fl_rgb_color(224, 224, 224);
    c->sel_bg     = fl_rgb_color(255, 255, 255);
    c->rowline    = fl_rgb_color(216, 216, 216);
    c->sep        = fl_rgb_color(160, 160, 160);
    c->text       = fl_rgb_color(  0,   0,   0);
    c->cursor     = fl_rgb_color(  0,  80, 160);
    c->symbol     = fl_rgb_color(  0, 120, 192);
    c->ident      = fl_rgb_color( 64, 160,   0);
    c->special    = fl_rgb_color(192, 120,   0);
    c->si_pfx     = fl_rgb_color(144,  80, 224);
    c->paren[0]   = fl_rgb_color(  0, 120, 192);
    c->paren[1]   = fl_rgb_color(128,  64, 192);
    c->paren[2]   = fl_rgb_color(192,  64, 128);
    c->paren[3]   = fl_rgb_color(192, 120,   0);
    c->error      = fl_rgb_color(192,  64,  64);
    c->ui_win_bg  = fl_rgb_color(235, 235, 235);
    c->ui_bg      = fl_rgb_color(230, 230, 230);
    c->ui_input   = fl_rgb_color(245, 245, 245);
    c->ui_btn     = fl_rgb_color(210, 210, 215);
    c->ui_menu    = fl_rgb_color(220, 220, 225);
    c->ui_text    = fl_rgb_color( 20,  20,  25);
    c->ui_label   = fl_rgb_color( 90,  90, 100);
    c->ui_dim     = fl_rgb_color(180, 180, 185);
    c->pop_bg     = fl_rgb_color(245, 245, 248);
    c->pop_sel    = fl_rgb_color(190, 215, 250);
    c->pop_text   = fl_rgb_color( 15,  15,  20);
    c->pop_desc   = fl_rgb_color(100, 100, 110);
    c->pop_desc_bg= fl_rgb_color(232, 232, 238);
    c->pop_border = fl_rgb_color(170, 170, 185);
}

void colors_init_defaults(CalcyxColors *c) {
    init_otaku_black(c);
}

void colors_init_preset(CalcyxColors *c, int preset) {
    switch (preset) {
    case COLOR_PRESET_OTAKU_BLACK:     init_otaku_black(c);     break;
    case COLOR_PRESET_GYAKUBARI_WHITE: init_gyakubari_white(c); break;
    case COLOR_PRESET_SABOTEN_GREY:    init_saboten_grey(c);    break;
    case COLOR_PRESET_SABOTEN_WHITE:   init_saboten_white(c);   break;
    default:                           init_otaku_black(c);     break;
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
    Fl::set_color(FL_SELECTION_COLOR, g_colors.cursor);
}
