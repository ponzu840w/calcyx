// colors.h — calcyx UI カラーテーマ定義
// calctus Settings.Appearance_Color_* に準拠した論理名で管理する。
// 将来の設定ダイアログで g_colors を書き換えることで全 UI に反映できる。

#pragma once
#include <FL/Fl.H>

/* ---- CalcyxColors ---- */

typedef struct {
    /* 背景 / フレーム */
    Fl_Color bg;         /* シート背景                  (Appearance_Color_Background) */
    Fl_Color sel_bg;     /* 選択行背景                  (Appearance_Color_Active_Background 相当) */
    Fl_Color rowline;    /* 行区切り線 */
    Fl_Color sep;        /* UI セパレータ */

    /* テキスト / カーソル */
    Fl_Color text;       /* デフォルトテキスト (白)     (Appearance_Color_Text) */
    Fl_Color cursor;     /* 入力カーソル */

    /* シンタックスハイライト */
    Fl_Color symbol;     /* 演算子・記号・=             (Appearance_Color_Symbols) */
    Fl_Color ident;      /* 識別子・変数名              (Appearance_Color_Identifiers) */
    Fl_Color special;    /* 文字/文字列/日時/bool リテラル (Appearance_Color_Special_Literals) */
    Fl_Color si_pfx;     /* SI 接頭語文字               (Appearance_Color_SI_Prefix) */
    Fl_Color paren[4];   /* 括弧深さ別 (0-3)            (Appearance_Color_Parenthesis_1..4) */
    Fl_Color error;      /* エラーテキスト              (Appearance_Color_Error) */

    /* UI クローム (メインウィンドウ・ダイアログ・メニュー等) */
    Fl_Color ui_win_bg;  /* メインウィンドウ背景 */
    Fl_Color ui_bg;      /* ダイアログ・タブ背景 */
    Fl_Color ui_input;   /* 入力欄背景 */
    Fl_Color ui_btn;     /* ボタン背景 */
    Fl_Color ui_menu;    /* メニューバー・ツールバー背景 */
    Fl_Color ui_text;    /* UI テキスト */
    Fl_Color ui_label;   /* UI ラベル (やや暗め) */
    Fl_Color ui_dim;     /* 無効状態のアイコン等 */
} CalcyxColors;

/* ---- カラープリセット ---- */

enum ColorPreset {
    COLOR_PRESET_OTAKU_BLACK = 0,
    COLOR_PRESET_GYAKUBARI_WHITE,
    COLOR_PRESET_SABOTEN_GREY,
    COLOR_PRESET_SABOTEN_WHITE,
    COLOR_PRESET_USER_DEFINED,
    COLOR_PRESET_COUNT,
};

struct ColorPresetInfo {
    const char *id;
    const char *label;
};

extern const ColorPresetInfo COLOR_PRESET_INFO[COLOR_PRESET_COUNT];

/* ---- グローバル設定 ---- */

extern CalcyxColors g_colors;
extern int g_color_preset;

void colors_init_defaults(CalcyxColors *c);
void colors_init_preset(CalcyxColors *c, int preset);
void colors_apply_fl_scheme();
