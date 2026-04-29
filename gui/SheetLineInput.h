// シンタックスハイライト付き Fl_Input (SheetView のフォーカス行)。
//   editor_mode=true  → 左辺 (編集、 矢印/Enter は親へ、 live_eval)
//   editor_mode=false → 右辺 (read-only, 同じ描画ルール)

#pragma once

#include <FL/Fl_Input.H>

extern "C" {
#include "types/val.h"
}

class SheetLineInput : public Fl_Input {
    bool      editor_mode_;
    Fl_Color  override_color_;  // 0 以外: シンタックスハイライトを使わず単色描画
    val_fmt_t result_fmt_ = FMT_REAL;  // 結果値のフォーマット (セパレータ用、 右辺のみ)

public:
    SheetLineInput(int x, int y, int w, int h, bool editor_mode = true);

    void set_override_color(Fl_Color c) { override_color_ = c; }
    void set_result_fmt(val_fmt_t f)    { result_fmt_ = f; }

    void draw() override;
    int  handle(int event) override;
};
