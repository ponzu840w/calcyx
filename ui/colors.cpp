// colors.cpp — CalcyxColors デフォルト値

#include "colors.h"
#include <FL/fl_draw.H>

CalcyxColors g_colors;

void colors_init_defaults(CalcyxColors *c) {
    /* 背景 / フレーム */
    c->bg         = fl_rgb_color( 22,  22,  22);
    c->sel_bg     = fl_rgb_color( 38,  42,  55);
    c->rowline    = fl_rgb_color( 32,  32,  36);
    c->sep        = fl_rgb_color( 55,  55,  65);

    /* テキスト / カーソル */
    c->text       = fl_rgb_color(255, 255, 255);
    c->cursor     = fl_rgb_color(180, 200, 255);

    /* シンタックスハイライト (移植元: Calctus/Settings.cs Appearance_Color_*) */
    c->symbol     = fl_rgb_color( 64, 192, 255);  /* Appearance_Color_Symbols       */
    c->ident      = fl_rgb_color(192, 255, 128);  /* Appearance_Color_Identifiers   */
    c->special    = fl_rgb_color(255, 192,  64);  /* Appearance_Color_Special_Literals */
    c->si_pfx     = fl_rgb_color(224, 160, 255);  /* Appearance_Color_SI_Prefix     */
    c->paren[0]   = fl_rgb_color( 64, 192, 255);  /* Appearance_Color_Parenthesis_1 */
    c->paren[1]   = fl_rgb_color(192, 128, 255);  /* Appearance_Color_Parenthesis_2 */
    c->paren[2]   = fl_rgb_color(255, 128, 192);  /* Appearance_Color_Parenthesis_3 */
    c->paren[3]   = fl_rgb_color(255, 192,  64);  /* Appearance_Color_Parenthesis_4 */
    c->error      = fl_rgb_color(110, 110, 110);  /* Appearance_Color_Error         */
}
