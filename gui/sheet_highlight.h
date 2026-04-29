// sheet_highlight — シート行の syntax highlight + 桁区切り計算ヘルパー.
//
// SheetView (シート全体の draw) と SheetLineInput (フォーカス行の編集欄) で
// 共有する. 元々 SheetView.cpp 内の static 関数群だったものを切り出した.

#pragma once

#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <vector>

extern "C" {
#include "types/val.h"
}

/* 行全体の左パディング (PAD) — 描画 X 位置の基準. */
inline constexpr int SHEET_ROW_PAD = 3;

/* 桁区切り用のピクセルシフトを計算する.
 * shifts[i] は文字 i の描画前に加算する累積オフセット. */
void calc_separator_shifts(const char *text, int len, val_fmt_t fmt,
                            std::vector<double> &shifts);

/* 式テキスト内の数値リテラルに対するセパレータシフトを計算する.
 * tok ごとに calc_separator_shifts を呼ぶ集約版. */
void calc_expr_separator_shifts(const char *expr, int len,
                                 std::vector<double> &shifts);

/* 文字位置 i の x 座標 (シフト込み) を返す. */
double char_pos_to_x(const char *text, int i,
                      const std::vector<double> &shifts);

/* x 座標から最も近い文字位置を返す (マウスクリック用). */
int x_to_char_pos(const char *text, int len, double target_x,
                   const std::vector<double> &shifts);

/* 文字ごとの fg/bg 色配列に従ってテキストを描画する.
 * sep_shifts != nullptr のときは桁区切りシフトを反映する. */
void draw_colored_spans(const char *text, int len,
                         const Fl_Color *fg, const Fl_Color *bg,
                         int text_x, int row_y, int row_h,
                         const double *sep_shifts = nullptr);

/* 式テキストを syntax highlight + 桁区切りで描画する.
 * sep_fmt: -1 (default) → 式とみなす. それ以外 → 結果値とみなしフォーマットで桁区切り. */
void draw_expr_highlighted(const char *expr,
                            int text_x,
                            int clip_x, int clip_y, int clip_w, int clip_h,
                            val_fmt_t sep_fmt = (val_fmt_t)-1);
