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
} CalcyxColors;

/* ---- グローバル設定 ---- */

/* SheetView 等はこれを参照する。main() で colors_init_defaults() を呼ぶこと。 */
extern CalcyxColors g_colors;

/* デフォルト値で初期化する (calctus 移植元の値を使用) */
void colors_init_defaults(CalcyxColors *c);
