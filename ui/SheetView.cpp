// 移植元: Calctus/UI/SheetView.cs (簡略版)

#include "SheetView.h"
#include "builtin_docs.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <vector>

// ---- カラー (移植元: Calctus/Settings.cs Appearance_Color_*) ----
static const Fl_Color C_BG         = fl_rgb_color( 22,  22,  22);
static const Fl_Color C_SEL        = fl_rgb_color( 38,  42,  55);
static const Fl_Color C_SEP        = fl_rgb_color( 55,  55,  65);
static const Fl_Color C_ROWLINE    = fl_rgb_color( 32,  32,  36);
static const Fl_Color C_TEXT       = fl_rgb_color(255, 255, 255);
static const Fl_Color C_SYMBOL     = fl_rgb_color( 64, 192, 255);
static const Fl_Color C_IDENT      = fl_rgb_color(192, 255, 128);
static const Fl_Color C_SPECIAL    = fl_rgb_color(255, 192,  64);
static const Fl_Color C_SI_PFXCHAR = fl_rgb_color(224, 160, 255);
static const Fl_Color C_PAREN[4]   = {
    fl_rgb_color( 64, 192, 255),
    fl_rgb_color(192, 128, 255),
    fl_rgb_color(255, 128, 192),
    fl_rgb_color(255, 192,  64),
};
static const Fl_Color C_RESULT     = fl_rgb_color(100, 220, 100);
static const Fl_Color C_ERROR      = fl_rgb_color(255, 100, 100);
static const Fl_Color C_CURSOR     = fl_rgb_color(180, 200, 255);
static const int PAD = 3;

// ================================================================
// ハイライト描画ユーティリティ
// ================================================================

// fg/bg colors[i] は文字 i のカラー。bg == 0 は透明。
// text_x: テキスト描画開始 x 座標 (PAD 込み)
static void draw_colored_spans(const char *text, int len,
                                const Fl_Color *fg, const Fl_Color *bg,
                                int text_x, int row_y, int row_h) {
    if (len <= 0) return;
    int baseline = row_y + (row_h + fl_height() - fl_descent() * 2) / 2;

    // 第1パス: 背景色スパン
    for (int i = 0; i < len; ) {
        Fl_Color bc = bg[i];
        int j = i;
        while (j < len && bg[j] == bc) j++;
        if (bc != (Fl_Color)0) {
            double x1 = fl_width(text, i);
            double x2 = fl_width(text, j);
            fl_color(bc);
            fl_rectf(text_x + (int)x1, row_y, (int)(x2 - x1 + 0.5), row_h);
        }
        i = j;
    }

    // 第2パス: 前景色テキストスパン
    for (int i = 0; i < len; ) {
        Fl_Color fc = fg[i];
        int j = i;
        while (j < len && fg[j] == fc) j++;
        fl_color(fc);
        fl_draw(text + i, j - i, text_x + (int)fl_width(text, i), baseline);
        i = j;
    }
}

// 式テキストをトークンハイライト付きで描画する。
// text_x: テキスト描画開始 x (PAD込み)
// clip_* : クリップ矩形
static void draw_expr_highlighted(const char *expr,
                                   int text_x,
                                   int clip_x, int clip_y, int clip_w, int clip_h) {
    int len = (int)strlen(expr);
    if (len == 0) return;

    std::vector<Fl_Color> fg(len, C_TEXT);
    std::vector<Fl_Color> bg(len, (Fl_Color)0);

    tok_queue_t q;
    tok_queue_init(&q);
    lexer_tokenize(expr, &q);

    int paren_depth = 0;
    for (;;) {
        const token_t *peek = tok_queue_peek(&q);
        if (!peek || peek->type == TOK_EOS || peek->type == TOK_EMPTY) break;
        token_t tok = tok_queue_pop(&q);

        int p  = tok.pos;
        int tl = (int)strlen(tok.text);
        if (p < 0 || p >= len) { tok_free(&tok); continue; }
        int end = std::min(p + tl, len);

        switch (tok.type) {
            case TOK_WORD:
                for (int i = p; i < end; i++) fg[i] = C_IDENT;
                break;

            case TOK_BOOL_LIT:
                for (int i = p; i < end; i++) fg[i] = C_SPECIAL;
                break;

            case TOK_NUM_LIT:
                if (tok.val) {
                    val_fmt_t vfmt = tok.val->fmt;
                    if (vfmt == FMT_SI_PREFIX) {
                        if (end - 1 >= p) fg[end - 1] = C_SI_PFXCHAR;
                    } else if (vfmt == FMT_BIN_PREFIX) {
                        if (end - 2 >= p) fg[end - 2] = C_SI_PFXCHAR;
                        if (end - 1 >= p) fg[end - 1] = C_SI_PFXCHAR;
                    } else if (vfmt == FMT_WEB_COLOR) {
                        // トークンテキストから色値をパース
                        unsigned int rgb = 0;
                        const char *hex = tok.text + 1;
                        int hlen = (int)strlen(hex);
                        if (hlen == 6) {
                            sscanf(hex, "%6x", &rgb);
                        } else if (hlen == 3) {
                            unsigned int r4 = 0, g4 = 0, b4 = 0;
                            sscanf(hex, "%1x%1x%1x", &r4, &g4, &b4);
                            rgb = ((r4 | (r4 << 4)) << 16) |
                                  ((g4 | (g4 << 4)) <<  8) |
                                   (b4 | (b4 << 4));
                        }
                        Fl_Color bc = fl_rgb_color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
                        int lum = (int)(((rgb>>16)&0xFF)*299 + ((rgb>>8)&0xFF)*587 + (rgb&0xFF)*114) / 1000;
                        Fl_Color fc = lum < 128 ? FL_WHITE : FL_BLACK;
                        for (int i = p; i < end; i++) { bg[i] = bc; fg[i] = fc; }
                    } else if (vfmt == FMT_CHAR || vfmt == FMT_STRING || vfmt == FMT_DATETIME) {
                        for (int i = p; i < end; i++) fg[i] = C_SPECIAL;
                    } else {
                        // 指数部 (e/E) ハイライト: 直前が数字の場合のみ
                        for (int i = p + 1; i < end; i++) {
                            if ((expr[i] == 'e' || expr[i] == 'E') &&
                                isdigit((unsigned char)expr[i-1])) {
                                for (int k = i; k < end; k++) fg[k] = C_SI_PFXCHAR;
                                break;
                            }
                        }
                    }
                }
                break;

            case TOK_OP:
            case TOK_KEYWORD:
                for (int i = p; i < end; i++) fg[i] = C_SYMBOL;
                break;

            case TOK_SYMBOL:
                if (tl == 1 && (tok.text[0] == '(' || tok.text[0] == ')')) {
                    int d = paren_depth;
                    if (tok.text[0] == ')' && d > 0) d--;
                    fg[p] = C_PAREN[d % 4];
                    if (tok.text[0] == '(') paren_depth++;
                    else if (paren_depth > 0) paren_depth--;
                } else {
                    for (int i = p; i < end; i++) fg[i] = C_SYMBOL;
                }
                break;

            default:
                break;
        }
        tok_free(&tok);
    }
    tok_queue_free(&q);

    fl_push_clip(clip_x, clip_y, clip_w, clip_h);
    fl_font(FL_COURIER, 13);
    draw_colored_spans(expr, len, fg.data(), bg.data(), text_x, clip_y, clip_h);
    fl_pop_clip();
}


// ================================================================
// SheetLineInput: シンタックスハイライト付き Fl_Input。
// editor_mode=true  → 左辺 (式編集、Up/Down/Enter を親に委譲、live_eval 呼び出し)
// editor_mode=false → 右辺 (読み取り専用、同じ描画ルール)
// ================================================================
class SheetLineInput : public Fl_Input {
    bool     editor_mode_;
    Fl_Color override_color_;  // 0 以外: シンタックスハイライトを使わず単色描画

public:
    SheetLineInput(int x, int y, int w, int h, bool editor_mode = true)
        : Fl_Input(x, y, w, h), editor_mode_(editor_mode), override_color_(0)
    {
        if (!editor_mode_) readonly(1);
    }

    void set_override_color(Fl_Color c) { override_color_ = c; }

    void draw() override {
        fl_font(textfont(), textsize());

        const char *txt = value();
        int p1 = std::min(position(), mark());
        int p2 = std::max(position(), mark());
        int sx = xscroll();
        int tx = x() + PAD - sx;

        // 背景
        fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), C_SEL);

        // 選択ハイライト背景
        if (p1 != p2) {
            int sx1 = tx + (int)fl_width(txt, p1);
            int sx2 = tx + (int)fl_width(txt, p2);
            fl_push_clip(x(), y(), w(), h());
            fl_color(FL_SELECTION_COLOR);
            fl_rectf(sx1, y() + 1, sx2 - sx1, h() - 2);
            fl_pop_clip();
        }

        // テキスト描画
        if (override_color_ != (Fl_Color)0) {
            // エラーなど単色描画
            fl_push_clip(x(), y(), w(), h());
            fl_font(FL_COURIER, 13);
            fl_color(override_color_);
            int baseline = y() + (h() + fl_height() - fl_descent() * 2) / 2;
            fl_draw(txt, tx, baseline);
            fl_pop_clip();
        } else {
            draw_expr_highlighted(txt, tx, x(), y(), w(), h());
        }

        // カーソル (非選択時のみ)
        if (Fl::focus() == this && p1 == p2) {
            int cx = tx + (int)fl_width(txt, position());
            if (cx >= x() && cx < x() + w()) {
                fl_color(cursor_color());
                fl_line(cx, y() + 2, cx, y() + h() - 2);
            }
        }
    }

    int handle(int event) override {
        if (event == FL_KEYBOARD) {
            int key  = Fl::event_key();
            bool ctrl = Fl::event_state(FL_CTRL) != 0;
            bool meta = Fl::event_state(FL_COMMAND) != 0;

            if (editor_mode_) {
                auto *sv = static_cast<SheetView *>(parent());
                // ポップアップが表示中: Up/Down/Tab/Escape を補完ナビに使う
                if (sv->popup_->is_shown()) {
                    if (key == FL_Up)   { sv->popup_->select_prev(); return 1; }
                    if (key == FL_Down) { sv->popup_->select_next(); return 1; }
                    if (key == FL_Tab)  { sv->completion_confirm();  return 1; }
                    if (key == FL_Escape) { sv->completion_hide();   return 1; }
                    // Ctrl+Space: 補完を明示的に開く（ポップアップ非表示時も同様）
                }
                if (ctrl && key == ' ') {
                    sv->completion_update();
                    return 1;
                }
            }

            // Up/Down/Enter は左右どちらの欄からも SheetView::handle() に委譲
            if (key == FL_Up || key == FL_Down ||
                key == FL_Enter || key == FL_KP_Enter) {
                if (editor_mode_) static_cast<SheetView *>(parent())->completion_hide();
                return 0;
            }
            // Ctrl+Delete/BackSpace は左辺のみ (行削除)
            if (editor_mode_ && ctrl && (key == FL_Delete || key == FL_BackSpace)) {
                return 0;
            }
            // Cmd+Z / Cmd+Y / Cmd+Shift+Z は SheetView::handle() に委譲 (Undo/Redo)
            if (meta && (key == 'z' || key == 'y')) {
                return 0;
            }
        }
        // ペースト時: 改行を空白に置換してから挿入（オリジナルと同じ動作）
        if (event == FL_PASTE && editor_mode_) {
            const char *txt = Fl::event_text();
            int tlen = Fl::event_length();
            std::string clean;
            clean.reserve(tlen);
            for (int i = 0; i < tlen; i++) {
                char c = txt[i];
                clean += (c == '\n' || c == '\r') ? ' ' : c;
            }
            replace(position(), mark(), clean.c_str(), (int)clean.size());
            if (parent()) static_cast<SheetView *>(parent())->live_eval();
            return 1;
        }

        int ret = Fl_Input::handle(event);
        if (ret && parent()) {
            if (editor_mode_ && event == FL_KEYBOARD) {
                auto *sv = static_cast<SheetView *>(parent());
                sv->live_eval();
                sv->completion_update();
            }
            if (event == FL_PUSH || event == FL_DRAG || event == FL_RELEASE) {
                parent()->redraw();
                if (editor_mode_)
                    static_cast<SheetView *>(parent())->completion_hide();
            }
        }
        return ret;
    }
};

// ================================================================
// SheetView
// ================================================================

SheetView::SheetView(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    box(FL_FLAT_BOX);
    color(C_BG);

    // eq_pos_ / eq_w_ の初期値 (update_layout() で上書きされる)
    fl_font(FL_COURIER, 13);
    eq_w_   = (int)fl_width("==") + 4;
    eq_pos_ = (w - SB_W) * 3 / 5;

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

    // フォーカス行の式エディタ
    editor_ = new SheetLineInput(x, y, expr_w(), ROW_H);
    editor_->box(FL_FLAT_BOX);
    editor_->color(C_SEL);
    editor_->textcolor(C_CURSOR);
    editor_->textfont(FL_COURIER);
    editor_->textsize(13);
    editor_->cursor_color(C_CURSOR);
    editor_->when(0);

    // フォーカス行の結果表示（右辺: 読み取り専用 SheetLineInput、左辺と同じスタイル）
    auto *rd = new SheetLineInput(result_x(), y, result_w(), ROW_H, false);
    rd->box(FL_FLAT_BOX);
    rd->color(C_SEL);
    rd->textcolor(C_CURSOR);
    rd->textfont(FL_COURIER);
    rd->textsize(13);
    rd->cursor_color(C_CURSOR);
    rd->when(0);
    result_display_ = rd;

    end();

    // popup_ は MainWindow が生成して set してくれる (初期値 nullptr)

    eval_ctx_init(&ctx_);
    builtin_register_all(&ctx_);

    rows_.push_back(Row{});
    sync_scroll();
    place_editor();
}

SheetView::~SheetView() {
    eval_ctx_free(&ctx_);
    // popup_ は MainWindow が所有・削除する
}

// ----------------------------------------------------------------
void SheetView::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);
    vscroll_->resize(x + w - SB_W, y, SB_W, h);
    update_layout();   // ウィンドウ幅変化に合わせて = 位置を再計算
    sync_scroll();
    place_editor();
}

int SheetView::row_at_y(int fy) const {
    int cum = 0;
    for (int i = scroll_top_; i < (int)rows_.size(); i++) {
        cum += row_h(i);
        if (fy < cum) return i;
    }
    return (int)rows_.size() - 1;
}

void SheetView::place_editor() {
    // focused_row_ の画面上 y 位置を累積計算
    if (focused_row_ < scroll_top_) {
        editor_->hide();
        result_display_->hide();
        return;
    }
    int cum = 0;
    for (int i = scroll_top_; i < focused_row_; i++)
        cum += row_h(i);
    if (cum >= h()) {
        editor_->hide();
        result_display_->hide();
        return;
    }
    int ry = y() + cum;
    const Row &row = rows_[focused_row_];

    editor_->show();
    if (row.wrapped) {
        // 折り返し: 式は上段 full-width、結果は下段右カラム
        editor_->resize(x(), ry, sheet_w(), ROW_H);
        update_result_display();
        result_display_->resize(result_x(), ry + ROW_H, result_w(), ROW_H);
    } else if (row.result.empty()) {
        // 結果なし: エディタは全幅
        editor_->resize(x(), ry, sheet_w(), ROW_H);
        update_result_display();  // result_display_ を hide する
    } else {
        editor_->resize(x(), ry, expr_w(), ROW_H);
        update_result_display();
        result_display_->resize(result_x(), ry, result_w(), ROW_H);
    }
}

void SheetView::sync_scroll() {
    int n = (int)rows_.size();
    // 下から積み上げて h() に収まる最大 scroll_top_ を求める
    int cum = 0, max_top = 0;
    for (int i = n - 1; i >= 0; i--) {
        cum += row_h(i);
        if (cum > h()) { max_top = i + 1; break; }
    }
    scroll_top_ = std::clamp(scroll_top_, 0, max_top);
    // スライダサイズ: 表示ピクセル / 総ピクセル
    int total_px = 0;
    for (int i = 0; i < n; i++) total_px += row_h(i);
    int vis_px = std::min(h(), total_px);
    vscroll_->bounds(0, max_top);
    vscroll_->slider_size(total_px > 0 ? (double)vis_px / total_px : 1.0);
    vscroll_->value(scroll_top_);
}

// 結果表示ウィジェットを focused_row_ の内容に合わせて更新
void SheetView::update_result_display() {
    auto *rd = static_cast<SheetLineInput *>(result_display_);

    if (focused_row_ < 0 || focused_row_ >= (int)rows_.size()) {
        rd->hide();
        return;
    }
    const Row &row = rows_[focused_row_];
    if (row.result.empty()) {
        rd->hide();
        return;
    }

    rd->value(row.result.c_str());

    if (row.error) {
        // エラーは単色描画
        rd->color(C_SEL);
        rd->set_override_color(C_ERROR);
    } else if (row.result_fmt == FMT_WEB_COLOR &&
               row.result.size() == 7 && row.result[0] == '#') {
        // WebColor: 背景をその色に、シンタックスハイライトは無効化してテキスト色を調整
        unsigned int rgb = 0;
        if (sscanf(row.result.c_str() + 1, "%6x", &rgb) == 1) {
            int r8 = (rgb >> 16) & 0xFF;
            int g8 = (rgb >>  8) & 0xFF;
            int b8 =  rgb        & 0xFF;
            rd->color(fl_rgb_color(r8, g8, b8));
            int lum = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
            rd->set_override_color(lum < 128 ? FL_WHITE : FL_BLACK);
        }
    } else {
        // 通常: シンタックスハイライト (draw_expr_highlighted が自動判定)
        rd->color(C_SEL);
        rd->set_override_color((Fl_Color)0);
    }
    rd->show();
    rd->redraw();
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
            row.result_fmt = FMT_REAL;
            continue;
        }
        ctx_.has_error    = false;
        ctx_.error_msg[0] = '\0';
        char errmsg[256]  = "";
        val_t *v = eval_str(row.expr.c_str(), &ctx_, errmsg, sizeof(errmsg));
        if (v) {
            char buf[512];
            val_to_str(v, buf, sizeof(buf));
            row.result_fmt = v->fmt;
            row.result = buf;
            row.error  = false;
            val_free(v);
        } else {
            row.result     = errmsg[0] ? errmsg : "error";
            row.error      = true;
            row.result_fmt = FMT_REAL;
            ctx_.has_error    = false;
            ctx_.error_msg[0] = '\0';
        }
    }
    update_layout();  // 結果が変わったので = 位置を再計算
}

// ----------------------------------------------------------------
// 移植元: Calctus/UI/SheetView.cs - validateLayout()
// 全行の式幅・答え幅を計測し、最大値から "=" カラム位置を算出する。
void SheetView::update_layout() {
    fl_font(FL_COURIER, 13);

    // "=" カラム幅: オリジナルは "==" の文字幅
    eq_w_ = (int)fl_width("==") + 4;

    int avail = sheet_w();
    int min_eq_pos = std::min(eq_w_, avail / 5);

    int max_expr_w = 0;
    int max_ans_w  = 0;
    bool has_result = false;
    for (const auto &row : rows_) {
        if (row.result.empty()) continue;
        has_result = true;
        if (!row.expr.empty())
            max_expr_w = std::max(max_expr_w, (int)fl_width(row.expr.c_str()) + PAD * 2);
        max_ans_w = std::max(max_ans_w, (int)fl_width(row.result.c_str()) + PAD * 2);
    }

    // 結果が1行もない場合は eq_pos_ を変更しない (オリジナル: itemsHaveAns が空なら何もしない)
    if (has_result) {
        int new_eq_pos;
        if (max_expr_w + max_ans_w + eq_w_ < avail) {
            new_eq_pos = std::max(min_eq_pos, max_expr_w);
        } else {
            new_eq_pos = std::max(min_eq_pos, avail - max_ans_w - eq_w_);
        }
        new_eq_pos = std::min(new_eq_pos, avail - eq_w_ * 2);
        new_eq_pos = std::max(new_eq_pos, min_eq_pos);
        eq_pos_ = new_eq_pos;
    }

    // 式幅が eq_pos_ を超える行は2行レイアウト (移植元: SheetViewItem.GetPreferredSize)
    for (auto &row : rows_) {
        row.wrapped = !row.result.empty() && !row.expr.empty() &&
                      (int)fl_width(row.expr.c_str()) > eq_pos_;
    }
}

void SheetView::live_eval() {
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size()) {
        rows_[focused_row_].expr = editor_->value();
    }
    eval_all();          // update_layout() を内部で呼ぶ
    place_editor();      // = 位置が変わった場合にウィジェットを再配置
    update_result_display();
    redraw();
}

void SheetView::commit() {
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size()) {
        const std::string new_expr(editor_->value());
        if (new_expr != original_expr_) {
            UndoEntry e;
            e.view_state = capture_view_state();
            e.undo_ops.push_back({ UndoOpType::ChangeExpr, focused_row_, original_expr_ });
            e.redo_ops.push_back({ UndoOpType::ChangeExpr, focused_row_, new_expr });
            original_expr_ = new_expr;  // 二重登録防止
            commit_undo_entry(std::move(e));
            return;
        }
    }
    eval_all();
    place_editor();
    update_result_display();
    redraw();
    if (row_change_cb_) row_change_cb_(row_change_data_);
}

void SheetView::focus_row(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= (int)rows_.size()) idx = (int)rows_.size() - 1;
    focused_row_ = idx;
    original_expr_ = rows_[idx].expr;  // Undo 比較用に記録
    editor_->value(rows_[idx].expr.c_str());
    editor_->insert_position(editor_->size());

    if (focused_row_ < scroll_top_) {
        scroll_top_ = focused_row_;
    } else {
        // focused_row_ が可視範囲に収まるか確認
        int cum = 0;
        for (int i = scroll_top_; i <= focused_row_; i++)
            cum += row_h(i);
        if (cum > h()) {
            // 収まらないので focused_row_ が下端に来るよう scroll_top_ を調整
            scroll_top_ = focused_row_;
            cum = row_h(focused_row_);
            while (scroll_top_ > 0) {
                int ph = row_h(scroll_top_ - 1);
                if (cum + ph <= h()) { scroll_top_--; cum += ph; }
                else break;
            }
        }
    }

    sync_scroll();
    place_editor();
    redraw();
    Fl::focus(editor_);
    if (row_change_cb_) row_change_cb_(row_change_data_);
}

void SheetView::set_row_change_cb(void (*cb)(void *), void *data) {
    row_change_cb_  = cb;
    row_change_data_ = data;
}

// 移植元: SheetViewItem.ReplaceFormatterFunction の逆: 現在の式のラッパー関数名を返す
const char *SheetView::current_fmt_name() const {
    static const char *const FMTFUNCS[] = {
        "dec","hex","bin","oct","si","kibi","char", nullptr
    };
    if (focused_row_ < 0 || focused_row_ >= (int)rows_.size()) return nullptr;
    const std::string &expr = rows_[focused_row_].expr;
    size_t start = expr.find_first_not_of(' ');
    if (start == std::string::npos) return nullptr;
    for (int i = 0; FMTFUNCS[i]; i++) {
        size_t fnlen = strlen(FMTFUNCS[i]);
        if (expr.compare(start, fnlen, FMTFUNCS[i]) != 0) continue;
        size_t p = start + fnlen;
        while (p < expr.size() && expr[p] == ' ') p++;
        if (p < expr.size() && expr[p] == '(') return FMTFUNCS[i];
    }
    return nullptr;
}

void SheetView::insert_row(int after) {
    // 現在の編集内容を rows_ に反映してから view_state をキャプチャ
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size())
        rows_[focused_row_].expr = editor_->value();
    int ins = std::min(after + 1, (int)rows_.size());
    UndoEntry e;
    e.view_state = capture_view_state();
    e.undo_ops.push_back({ UndoOpType::Delete,  ins, "" });
    e.redo_ops.push_back({ UndoOpType::Insert,  ins, "" });
    commit_undo_entry(std::move(e));
    focus_row(ins);
}

void SheetView::delete_row(int idx) {
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size())
        rows_[focused_row_].expr = editor_->value();
    if ((int)rows_.size() <= 1) {
        if (rows_[0].expr.empty()) return;
        UndoEntry e;
        e.view_state = capture_view_state();
        e.undo_ops.push_back({ UndoOpType::ChangeExpr, 0, rows_[0].expr });
        e.redo_ops.push_back({ UndoOpType::ChangeExpr, 0, "" });
        commit_undo_entry(std::move(e));
        focus_row(0);
        return;
    }
    const std::string deleted_expr = rows_[idx].expr;
    UndoEntry e;
    e.view_state = capture_view_state();
    e.undo_ops.push_back({ UndoOpType::Insert, idx, deleted_expr });
    e.redo_ops.push_back({ UndoOpType::Delete, idx, "" });
    commit_undo_entry(std::move(e));
    focus_row(std::min(idx, (int)rows_.size() - 1));
}

// 移植元: Calctus/UI/Sheets/SheetViewItem.cs - ReplaceFormatterFunction()
// フォーマッタ関数 (hex/bin/oct/dec/si/kibi/char) のラッパーを検出して除去する
static std::string strip_formatter(const std::string &expr) {
    static const char *const FMTFUNCS[] = {
        "dec","hex","bin","oct","si","kibi","char", nullptr
    };
    size_t start = expr.find_first_not_of(' ');
    if (start == std::string::npos) return expr;
    for (int i = 0; FMTFUNCS[i]; i++) {
        size_t fnlen = strlen(FMTFUNCS[i]);
        if (expr.compare(start, fnlen, FMTFUNCS[i]) != 0) continue;
        size_t p = start + fnlen;
        while (p < expr.size() && expr[p] == ' ') p++;
        if (p >= expr.size() || expr[p] != '(') continue;
        p++; // '(' をスキップ
        while (p < expr.size() && expr[p] == ' ') p++;
        // 末尾の ')' を探す
        size_t last = expr.find_last_not_of(' ');
        if (last == std::string::npos || expr[last] != ')') continue;
        std::string body = expr.substr(p, last - p);
        size_t blen = body.find_last_not_of(' ');
        return (blen != std::string::npos) ? body.substr(0, blen + 1) : body;
    }
    return expr;
}

void SheetView::apply_fmt(const char *func_name) {
    if (focused_row_ < 0 || focused_row_ >= (int)rows_.size()) return;
    std::string body = strip_formatter(editor_->value());
    std::string new_expr = (func_name && func_name[0])
                           ? std::string(func_name) + "(" + body + ")"
                           : body;
    // rows_ はまだ更新しない → commit() が original_expr_ との差分を検出して Undo エントリを作る
    editor_->value(new_expr.c_str());
    editor_->insert_position(editor_->size());
    commit();
}

// ----------------------------------------------------------------
void SheetView::draw() {
    fl_push_no_clip();
    fl_push_clip(x(), y(), w(), h());

    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), C_BG);

    const int ew  = expr_w();
    const int eqx = eq_col_x();
    const int rx  = result_x();
    const int rw  = result_w();

    fl_font(FL_COURIER, 13);

    // 結果値を指定位置に描画するラムダ (非フォーカス行・折り返し下段どちらにも使用)
    auto draw_result_at = [&](const Row &row, int ary, int abaseline) {
        if (row.result.empty()) return;
        if (row.error) {
            fl_push_clip(rx, ary, rw, ROW_H);
            fl_font(FL_COURIER, 13);
            fl_color(C_ERROR);
            fl_draw(row.result.c_str(), rx + PAD, abaseline);
            fl_pop_clip();
        } else if (row.result_fmt == FMT_WEB_COLOR &&
                   row.result.size() == 7 && row.result[0] == '#') {
            unsigned int rgb = 0;
            if (sscanf(row.result.c_str() + 1, "%6x", &rgb) == 1) {
                int r8 = (rgb >> 16) & 0xFF;
                int g8 = (rgb >>  8) & 0xFF;
                int b8 =  rgb        & 0xFF;
                Fl_Color bc = fl_rgb_color(r8, g8, b8);
                int lum = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
                Fl_Color fc = lum < 128 ? FL_WHITE : FL_BLACK;
                fl_draw_box(FL_FLAT_BOX, rx, ary, rw, ROW_H, bc);
                fl_push_clip(rx, ary, rw, ROW_H);
                fl_font(FL_COURIER, 13);
                fl_color(fc);
                fl_draw(row.result.c_str(), rx + PAD, abaseline);
                fl_pop_clip();
            }
        } else {
            draw_expr_highlighted(row.result.c_str(), rx + PAD, rx, ary, rw, ROW_H);
        }
    };

    int ry = y();
    for (int i = scroll_top_; i < (int)rows_.size(); i++) {
        if (ry >= y() + h()) break;
        const Row &row = rows_[i];
        int rh = row_h(i);

        if (row.wrapped) {
            // ---- 折り返しレイアウト: 上段=式(全幅), 下段="="+結果 ----
            int ry2      = ry + ROW_H;
            int bl_top   = ry  + (ROW_H + fl_height() - fl_descent() * 2) / 2;
            int bl_bot   = ry2 + (ROW_H + fl_height() - fl_descent() * 2) / 2;

            // 背景 (2行分)
            if (i == focused_row_)
                fl_draw_box(FL_FLAT_BOX, x(), ry, sheet_w(), rh, C_SEL);

            // 上段: 式 (全幅)
            if (!row.expr.empty())
                draw_expr_highlighted(row.expr.c_str(), x() + PAD, x(), ry, sheet_w(), ROW_H);

            // 上段/下段の境界線
            fl_color(C_ROWLINE);
            fl_line(x(), ry + ROW_H - 1, x() + sheet_w(), ry + ROW_H - 1);

            // 下段: "=" + 結果
            if (!row.result.empty() && !row.error) {
                fl_font(FL_COURIER, 13);
                fl_color(C_SYMBOL);
                fl_draw("=", eqx + (eq_w_ - (int)fl_width("=")) / 2, bl_bot);
            }

            // 下段の結果 (フォーカス行は result_display_ が上書き)
            draw_result_at(row, ry2, bl_bot);

            // 下段の区切り線
            fl_color(C_ROWLINE);
            fl_line(x(), ry2 + ROW_H - 1, x() + sheet_w(), ry2 + ROW_H - 1);

        } else {
            // ---- 通常1行レイアウト ----
            int baseline = ry + (ROW_H + fl_height() - fl_descent() * 2) / 2;
            bool has_result = !row.result.empty();

            // フォーカス行の背景: 結果なしなら全幅、あれば式+"="カラムのみ (結果は result_display_ が担当)
            if (i == focused_row_)
                fl_draw_box(FL_FLAT_BOX, x(), ry, has_result ? ew + eq_w_ : sheet_w(), ROW_H, C_SEL);

            // 式テキスト: 結果なしなら全幅で描画
            if (!row.expr.empty()) {
                if (has_result)
                    draw_expr_highlighted(row.expr.c_str(), x() + PAD, x(), ry, ew, ROW_H);
                else
                    draw_expr_highlighted(row.expr.c_str(), x() + PAD, x(), ry, sheet_w(), ROW_H);
            }

            // "=" 記号 (縦線なし)
            if (has_result && !row.error) {
                fl_font(FL_COURIER, 13);
                fl_color(C_SYMBOL);
                fl_draw("=", eqx + (eq_w_ - (int)fl_width("=")) / 2, baseline);
            }

            // 右カラム: 結果値 (フォーカス行は result_display_ が上書き)
            draw_result_at(row, ry, baseline);

            // 横区切り線
            fl_color(C_ROWLINE);
            fl_line(x(), ry + ROW_H - 1, x() + sheet_w(), ry + ROW_H - 1);
        }

        ry += rh;
    }

    // 子ウィジェット (editor_, result_display_, vscroll_) を描画
    draw_children();

    fl_pop_clip();
    fl_pop_clip();
}

// ----------------------------------------------------------------
int SheetView::handle(int event) {
    if (event == FL_KEYBOARD) {
        int key   = Fl::event_key();
        bool shift = Fl::event_state(FL_SHIFT) != 0;
        bool ctrl  = Fl::event_state(FL_CTRL)  != 0;

        bool meta = Fl::event_state(FL_COMMAND) != 0;
        if (meta && key == 'z' && !shift) { undo(); return 1; }
        if (meta && (key == 'y' || (key == 'z' && shift))) { redo(); return 1; }

        if (key == FL_Enter && !ctrl) {
            commit();  // 式変更があれば Undo エントリを作成
            if (shift) {
                insert_row(focused_row_);
            } else {
                if (focused_row_ + 1 >= (int)rows_.size()) {
                    insert_row(focused_row_);
                } else {
                    focus_row(focused_row_ + 1);
                }
            }
            redraw();
            return 1;
        }
        if (key == FL_Up) {
            commit();
            if (focused_row_ > 0) focus_row(focused_row_ - 1);
            return 1;
        }
        if (key == FL_Down) {
            commit();
            if (focused_row_ + 1 >= (int)rows_.size()) {
                insert_row(focused_row_);
            } else {
                focus_row(focused_row_ + 1);
            }
            return 1;
        }
        if (ctrl && (key == FL_Delete || key == FL_BackSpace)) {
            delete_row(focused_row_);
            return 1;
        }
    }

    if (event == FL_PUSH) {
        int ex = Fl::event_x();
        int ey = Fl::event_y();
        if (ex < x() + sheet_w()) {
            int clicked = row_at_y(ey - y());
            if (clicked >= 0 && clicked < (int)rows_.size() && clicked != focused_row_) {
                commit();
                focus_row(clicked);
                return 1;
            }
        }
    }

    if (event == FL_MOUSEWHEEL) {
        scroll_top_ = std::max(0, scroll_top_ + Fl::event_dy() * 3);
        sync_scroll();   // max_top を正しく計算してクランプ・スクロールバー同期
        place_editor();
        redraw();
        return 1;
    }

    return Fl_Group::handle(event);
}

// ----------------------------------------------------------------
bool SheetView::load_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    rows_.clear();
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        Row row;
        row.expr = line;
        rows_.push_back(row);
    }
    fclose(fp);

    if (rows_.empty()) rows_.push_back(Row{});

    undo_buf_.clear();
    undo_idx_ = 0;

    eval_all();
    scroll_top_  = 0;
    focused_row_ = 0;
    sync_scroll();
    focus_row(0);
    redraw();
    return true;
}

bool SheetView::save_file(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    for (const auto &row : rows_)
        fprintf(fp, "%s\n", row.expr.c_str());
    fclose(fp);
    return true;
}

// ----------------------------------------------------------------
// Undo / Redo (移植元: Calctus/UI/SheetOperator.cs)
// ----------------------------------------------------------------

SheetView::UndoViewState SheetView::capture_view_state() const {
    return { focused_row_, editor_->insert_position() };
}

void SheetView::apply_ops(const std::vector<UndoOp> &ops) {
    for (const auto &op : ops) {
        switch (op.type) {
            case UndoOpType::Insert: {
                Row r; r.expr = op.expr;
                rows_.insert(rows_.begin() + op.index, r);
                break;
            }
            case UndoOpType::Delete:
                rows_.erase(rows_.begin() + op.index);
                if (rows_.empty()) rows_.push_back(Row{});
                break;
            case UndoOpType::ChangeExpr:
                rows_[op.index].expr = op.expr;
                break;
        }
    }
}

void SheetView::commit_undo_entry(UndoEntry entry) {
    // カーソル以降の Redo 履歴を消去
    undo_buf_.resize(undo_idx_);
    // redo_ops を実際に rows_ へ適用
    apply_ops(entry.redo_ops);
    undo_buf_.push_back(std::move(entry));
    undo_idx_++;
    // 深さ制限
    if ((int)undo_buf_.size() > UNDO_DEPTH) {
        undo_buf_.erase(undo_buf_.begin());
        undo_idx_--;
    }
    eval_all();
    sync_scroll();
    place_editor();
    update_result_display();
    redraw();
    if (row_change_cb_) row_change_cb_(row_change_data_);
}

void SheetView::undo() {
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size()) {
        const std::string current(editor_->value());
        if (current != original_expr_) {
            // 未コミット編集がある → まずそれだけを取り消す (スタックは消費しない)
            rows_[focused_row_].expr = original_expr_;
            editor_->value(original_expr_.c_str());
            editor_->insert_position((int)original_expr_.size());
            eval_all();
            place_editor();
            update_result_display();
            redraw();
            return;
        }
        rows_[focused_row_].expr = editor_->value();
    }
    if (!can_undo()) return;
    undo_idx_--;
    const auto &entry = undo_buf_[undo_idx_];
    apply_ops(entry.undo_ops);
    eval_all();
    sync_scroll();
    int r = std::clamp(entry.view_state.focused_row, 0, (int)rows_.size() - 1);
    focus_row(r);
    editor_->insert_position(std::min(entry.view_state.cursor_pos, (int)editor_->size()));
    redraw();
}

void SheetView::test_type_and_commit(const char *expr) {
    editor_->value(expr);
    commit();
}

void SheetView::redo() {
    if (!can_redo()) return;
    if (focused_row_ >= 0 && focused_row_ < (int)rows_.size())
        rows_[focused_row_].expr = editor_->value();
    const auto &entry = undo_buf_[undo_idx_];
    apply_ops(entry.redo_ops);
    undo_idx_++;
    // 次 entry の view_state (またはなければ現 entry) に基づいてフォーカスを復元
    int r = (undo_idx_ < (int)undo_buf_.size())
            ? undo_buf_[undo_idx_].view_state.focused_row
            : entry.view_state.focused_row;
    r = std::clamp(r, 0, (int)rows_.size() - 1);
    eval_all();
    sync_scroll();
    focus_row(r);
    redraw();
}

// ================================================================
// 入力補完
// ================================================================

// 引数情報を含むラベル文字列を生成
static std::string make_label(const char *name, int n_params) {
    if (n_params == 0)  return std::string(name) + "()";
    if (n_params == 1)  return std::string(name) + "(x)";
    if (n_params == 2)  return std::string(name) + "(x, y)";
    if (n_params == 3)  return std::string(name) + "(x, y, z)";
    return std::string(name) + "(...)";  // variadic or 4+
}

// builtin_enum_main/extra のコールバック: Candidate を追加 (重複 id はスキップ)
struct EnumCtx { std::vector<Candidate> *out; };

static void enum_cb(const char *name, int n_params, void *userdata) {
    auto *ec = static_cast<EnumCtx *>(userdata);
    // 同名重複をスキップ
    for (auto &c : *ec->out)
        if (c.id == name) return;
    const char *doc = builtin_doc(name);
    ec->out->push_back({ name, make_label(name, n_params),
                         doc ? doc : "", true });
}

std::vector<Candidate> SheetView::build_candidates() const {
    std::vector<Candidate> result;
    EnumCtx ec{ &result };
    builtin_enum_main (enum_cb, &ec);
    builtin_enum_extra(enum_cb, &ec);

    // コンテキスト内のユーザー定義変数・定数 (builtin 関数は除く)
    for (int i = 0; i < ctx_.n_vars; i++) {
        const eval_var_t &v = ctx_.vars[i];
        if (!v.value) continue;
        if (v.value->type == VAL_FUNC && v.value->func_v && v.value->func_v->builtin)
            continue;  // 組み込み関数は builtin_enum で既に追加済み
        // 重複スキップ
        bool dup = false;
        for (auto &c : result) if (c.id == v.name) { dup = true; break; }
        if (dup) continue;

        bool is_func = (v.value->type == VAL_FUNC);
        std::string label;
        if (is_func && v.value->func_v) {
            label = make_label(v.name, v.value->func_v->n_params);
        } else {
            label = v.name;
        }
        result.push_back({ v.name, label, "", is_func });
    }

    // キーワード
    for (const char *kw : { "ans", "true", "false", "def" }) {
        bool dup = false;
        for (auto &c : result) if (c.id == kw) { dup = true; break; }
        if (!dup) result.push_back({ kw, kw, "", false });
    }

    // アルファベット順ソート
    std::sort(result.begin(), result.end(),
              [](const Candidate &a, const Candidate &b){ return a.id < b.id; });
    return result;
}

void SheetView::completion_update() {
    if (!editor_->visible()) return;

    const char *text = editor_->value();
    int pos = editor_->position();

    // カーソル直前が識別子継続文字でなければ補完しない
    if (pos == 0 || !lexer_is_id_follow(text[pos - 1])) {
        completion_hide();
        return;
    }

    // 識別子の先頭を探す
    int start = pos;
    while (start > 0 && lexer_is_id_follow(text[start - 1])) start--;

    // 先頭が識別子開始文字でなければ補完しない (数字だけの場合など)
    if (!lexer_is_id_start(text[start])) {
        completion_hide();
        return;
    }

    std::string key(text + start, pos - start);

    if (!popup_->is_shown()) {
        // 候補リストを再構築してから表示
        popup_->set_all(build_candidates());

        // カーソルのウィンドウ相対座標を計算
        // (Fl_Group ベースの popup_ はメインウィンドウ座標系を使う)
        fl_font(editor_->textfont(), editor_->textsize());
        int cur_pix  = (int)fl_width(text, pos);
        int wx       = editor_->x() + PAD + cur_pix;
        int wy_below = editor_->y() + editor_->h() + 2;
        int wy_above = editor_->y();
        popup_->show_at(wx, wy_below, wy_above, key);
    } else {
        popup_->update_key(key);
    }
}

void SheetView::completion_confirm() {
    const Candidate *c = popup_->selected();
    if (!c) { completion_hide(); return; }

    const char *text = editor_->value();
    int pos = editor_->position();

    // 識別子の先頭を探す
    int start = pos;
    while (start > 0 && lexer_is_id_follow(text[start - 1])) start--;

    // 置換テキストを構築
    std::string rep = c->id;
    if (c->is_function) rep += "(";

    editor_->replace(start, pos, rep.c_str(), (int)rep.size());
    // カーソルを括弧内 (関数) または末尾 (変数) に置く
    editor_->position((int)(start + rep.size()));

    completion_hide();
    live_eval();
}

void SheetView::completion_hide() {
    if (popup_->is_shown()) popup_->hide_popup();
}
