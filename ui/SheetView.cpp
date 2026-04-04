// 移植元: Calctus/UI/SheetView.cs (簡略版)

#include "SheetView.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <cstdio>

// ---- カラー ----
static const Fl_Color C_BG      = fl_rgb_color( 22,  22,  22);
static const Fl_Color C_SEL     = fl_rgb_color( 38,  42,  55);
static const Fl_Color C_EXPR    = fl_rgb_color(180, 200, 255);
static const Fl_Color C_RESULT  = fl_rgb_color(100, 220, 100);
static const Fl_Color C_ERROR   = fl_rgb_color(255, 100, 100);
static const Fl_Color C_SEP     = fl_rgb_color( 55,  55,  65);
static const Fl_Color C_ROWLINE = fl_rgb_color( 32,  32,  36);

// ----------------------------------------------------------------
SheetView::SheetView(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_FLAT_BOX);
    color(C_BG);

    // 縦スクロールバー
    vscroll_ = new Fl_Scrollbar(x + w - SB_W, y, SB_W, h);
    vscroll_->type(FL_VERTICAL);
    vscroll_->linesize(1);
    vscroll_->callback([](Fl_Widget *wb, void *d) {
        auto *sv = static_cast<SheetView *>(d);
        sv->scroll_top_ = static_cast<Fl_Scrollbar *>(wb)->value();
        sv->place_editor();
        sv->redraw();
    }, this);

    // 行エディタ（フォーカス行の式を編集する Fl_Input オーバーレイ）
    editor_ = new Fl_Input(x, y, expr_w(), ROW_H);
    editor_->box(FL_FLAT_BOX);
    editor_->color(C_SEL);
    editor_->textcolor(C_EXPR);
    editor_->textfont(FL_COURIER);
    editor_->textsize(13);
    editor_->cursor_color(C_EXPR);
    editor_->when(0);   // コールバック不要; キーイベントで制御

    end();

    // エンジン初期化
    eval_ctx_init(&ctx_);
    builtin_register_all(&ctx_);

    // 初期行
    rows_.push_back(Row{});
    sync_scroll();
    place_editor();
}

SheetView::~SheetView() {
    eval_ctx_free(&ctx_);
}

// ----------------------------------------------------------------
void SheetView::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);
    vscroll_->resize(x + w - SB_W, y, SB_W, h);
    editor_->resize(editor_->x(), editor_->y(), expr_w(), ROW_H);
    sync_scroll();
    place_editor();
}

int SheetView::row_at_y(int fy) const {
    return scroll_top_ + fy / ROW_H;
}

void SheetView::place_editor() {
    int rs = focused_row_ - scroll_top_;
    if (rs < 0 || rs * ROW_H >= h()) {
        editor_->hide();
        return;
    }
    editor_->show();
    editor_->resize(x(), y() + rs * ROW_H, expr_w(), ROW_H);
}

void SheetView::sync_scroll() {
    int total = (int)rows_.size();
    int vis   = std::max(1, h() / ROW_H);
    int max_top = std::max(0, total - vis);
    scroll_top_ = std::min(scroll_top_, max_top);
    vscroll_->bounds(0, max_top);
    vscroll_->slider_size(total > 0 ? (double)vis / total : 1.0);
    vscroll_->value(scroll_top_);
}

// ----------------------------------------------------------------
void SheetView::eval_all() {
    eval_ctx_free(&ctx_);
    eval_ctx_init(&ctx_);
    builtin_register_all(&ctx_);

    for (auto &row : rows_) {
        if (row.expr.empty()) {
            row.result.clear();
            row.error = false;
            continue;
        }
        ctx_.has_error   = false;
        ctx_.error_msg[0] = '\0';
        char errmsg[256] = "";
        val_t *v = eval_str(row.expr.c_str(), &ctx_, errmsg, sizeof(errmsg));
        if (v) {
            char buf[512];
            if (row.fmt != FMT_REAL) {
                val_t *fv = val_reformat(v, row.fmt);
                val_to_str(fv ? fv : v, buf, sizeof(buf));
                if (fv) val_free(fv);
            } else {
                val_to_str(v, buf, sizeof(buf));   // 自然フォーマットを維持
            }
            row.result = buf;
            row.error  = false;
            val_free(v);
        } else {
            row.result = errmsg[0] ? errmsg : "error";
            row.error  = true;
            ctx_.has_error   = false;
            ctx_.error_msg[0] = '\0';
        }
    }
}

void SheetView::commit() {
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size()) {
        rows_[focused_row_].expr = editor_->value();
    }
    eval_all();
    redraw();
}

void SheetView::focus_row(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= (int)rows_.size()) idx = (int)rows_.size() - 1;
    focused_row_ = idx;
    editor_->value(rows_[idx].expr.c_str());
    editor_->insert_position(editor_->size());

    int vis = std::max(1, h() / ROW_H);
    if (focused_row_ < scroll_top_)
        scroll_top_ = focused_row_;
    else if (focused_row_ >= scroll_top_ + vis)
        scroll_top_ = focused_row_ - vis + 1;

    sync_scroll();
    place_editor();
    redraw();
    Fl::focus(editor_);
}

void SheetView::insert_row(int after) {
    int ins = std::min(after + 1, (int)rows_.size());
    rows_.insert(rows_.begin() + ins, Row{});
    eval_all();
    sync_scroll();
    focus_row(ins);
}

void SheetView::delete_row(int idx) {
    if ((int)rows_.size() <= 1) {
        rows_[0] = Row{};
        editor_->value("");
        eval_all();
        redraw();
        return;
    }
    rows_.erase(rows_.begin() + idx);
    eval_all();
    sync_scroll();
    focus_row(std::min(idx, (int)rows_.size() - 1));
}

void SheetView::apply_fmt(val_fmt_t fmt) {
    if (focused_row_ < 0 || focused_row_ >= (int)rows_.size()) return;
    rows_[focused_row_].fmt = fmt;
    commit();
}

// ----------------------------------------------------------------
void SheetView::draw() {
    fl_push_clip(x(), y(), w(), h());

    // 背景
    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), C_BG);

    const int ew = expr_w();
    const int rw = sheet_w() - ew;
    const int rx = x() + ew;

    fl_font(FL_COURIER, 13);

    for (int i = scroll_top_; i < (int)rows_.size(); i++) {
        int ry = y() + (i - scroll_top_) * ROW_H;
        if (ry >= y() + h()) break;

        const Row &row = rows_[i];

        // フォーカス行の背景
        if (i == focused_row_)
            fl_draw_box(FL_FLAT_BOX, x(), ry, sheet_w(), ROW_H, C_SEL);

        // 式テキスト（フォーカス行は editor_ が描画）
        if (i != focused_row_ && !row.expr.empty()) {
            fl_color(C_EXPR);
            fl_draw(row.expr.c_str(),
                    x() + PAD, ry, ew - PAD, ROW_H,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        }

        // 縦区切り線
        fl_color(C_SEP);
        fl_line(rx, ry, rx, ry + ROW_H);

        // 結果テキスト
        if (!row.result.empty()) {
            fl_color(row.error ? C_ERROR : C_RESULT);
            std::string disp = row.error ? row.result : ("= " + row.result);
            fl_draw(disp.c_str(),
                    rx + PAD, ry, rw - PAD, ROW_H,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        }

        // 横区切り線
        fl_color(C_ROWLINE);
        fl_line(x(), ry + ROW_H - 1, x() + sheet_w(), ry + ROW_H - 1);
    }

    fl_pop_clip();

    // 子ウィジェット（editor_, vscroll_）を描画
    draw_children();
}

// ----------------------------------------------------------------
int SheetView::handle(int event) {
    // キーボードイベント: 特殊キーを横取り
    if (event == FL_KEYBOARD && Fl::focus() == editor_) {
        int key   = Fl::event_key();
        int shift = Fl::event_state(FL_SHIFT) != 0;
        int ctrl  = Fl::event_state(FL_CTRL)  != 0;

        if (key == FL_Enter && !ctrl) {
            rows_[focused_row_].expr = editor_->value();
            if (shift) {
                // Shift+Enter: 下に新規行を挿入
                insert_row(focused_row_);
            } else {
                eval_all();
                if (focused_row_ + 1 >= (int)rows_.size()) {
                    rows_.push_back(Row{});
                    sync_scroll();
                }
                focus_row(focused_row_ + 1);
            }
            redraw();
            return 1;
        }
        if (key == FL_Up && !ctrl) {
            commit();
            if (focused_row_ > 0) focus_row(focused_row_ - 1);
            return 1;
        }
        if (key == FL_Down && !ctrl) {
            commit();
            if (focused_row_ + 1 >= (int)rows_.size()) {
                rows_.push_back(Row{});
                sync_scroll();
            }
            focus_row(focused_row_ + 1);
            return 1;
        }
        // Ctrl+Delete: 現在行を削除
        if ((key == FL_Delete || key == FL_BackSpace) && ctrl) {
            delete_row(focused_row_);
            return 1;
        }
    }

    // マウスクリック: 行フォーカス
    if (event == FL_PUSH) {
        int ex = Fl::event_x();
        int ey = Fl::event_y();
        // スクロールバー領域はスキップ
        if (ex < x() + sheet_w()) {
            int clicked = row_at_y(ey - y());
            if (clicked >= 0 && clicked < (int)rows_.size()) {
                if (clicked != focused_row_) {
                    commit();
                    focus_row(clicked);
                    return 1;
                }
            }
        }
    }

    // マウスホイール: スクロール
    if (event == FL_MOUSEWHEEL) {
        int dy  = Fl::event_dy();
        int vis = std::max(1, h() / ROW_H);
        int max_top = std::max(0, (int)rows_.size() - vis);
        scroll_top_ = std::max(0, std::min(scroll_top_ + dy * 3, max_top));
        vscroll_->value(scroll_top_);
        place_editor();
        redraw();
        return 1;
    }

    return Fl_Group::handle(event);
}
