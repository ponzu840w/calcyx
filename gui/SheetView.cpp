// 移植元: Calctus/UI/SheetView.cs (簡略版)

#include "SheetView.h"
#include "PasteOptionForm.h"
#include "colors.h"
#include "settings_globals.h"
#include "crash_handler.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cctype>
#include <vector>

// カラーは g_colors (colors.h) を参照する。
static const int PAD = 3;

// --- ハイライト描画ユーティリティ ---
// 桁区切り用のピクセルシフトを計算する。
// shifts[i] は文字 i の描画前に加算する累積オフセット。
// gap は挿入する隙間ピクセル幅 (フォントサイズの 1/3 程度)。
static void calc_separator_shifts(const char *text, int len, val_fmt_t fmt,
                                   std::vector<double> &shifts) {
    shifts.assign(len, 0.0);
    if (len == 0) return;

    // 数値フォーマット以外はセパレータ不要
    if (fmt != FMT_REAL && fmt != FMT_INT && fmt != FMT_HEX &&
        fmt != FMT_BIN  && fmt != FMT_OCT)
        return;

    bool is_hex_family = (fmt == FMT_HEX || fmt == FMT_BIN || fmt == FMT_OCT);
    if (is_hex_family ? !g_sep_hex : !g_sep_thousands) return;

    // ユーザーが既に _ を入れていたらスキップ
    for (int i = 0; i < len; i++)
        if (text[i] == '_' || text[i] == ',') return;

    int group = is_hex_family ? 4 : 3;
    double gap = fl_width("0", 1) * 0.35;

    // プレフィックス ("0x", "0b", "0") をスキップ
    int start = 0;
    if (is_hex_family && len >= 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X' || text[1] == 'b' || text[1] == 'B'))
        start = 2;
    else if (fmt == FMT_OCT && len >= 1 && text[0] == '0')
        start = 1;

    // 整数部の終端を探す (小数点, 'e', 'E', 末尾)
    int int_end = len;
    int frac_start = -1;
    for (int i = start; i < len; i++) {
        if (text[i] == '.') { int_end = i; frac_start = i + 1; break; }
        if ((text[i] == 'e' || text[i] == 'E') && fmt != FMT_HEX) { int_end = i; break; }
    }

    // acc はセクション間で累積する (整数部→小数点→小数部→指数部)
    double acc = 0.0;

    // 整数部: 右端基準で group 桁ごとにギャップ
    int digits = int_end - start;
    if (digits > group) {
        for (int i = start; i < int_end; i++) {
            int from_right = int_end - i;
            if (from_right % group == 0 && i > start)
                acc += gap;
            shifts[i] = acc;
        }
    }

    // 整数部以降 (小数点, 小数部, 指数部) に整数部のシフトを伝搬
    for (int i = int_end; i < len; i++)
        shifts[i] = acc;

    // 小数部: 左端から group 桁ごとにギャップ (10進のみ)
    if (!is_hex_family && frac_start >= 0 && frac_start < len) {
        int count = 0;
        int frac_end = frac_start;
        for (int i = frac_start; i < len; i++) {
            if (!isdigit((unsigned char)text[i])) break;
            if (count > 0 && count % group == 0) acc += gap;
            count++;
            shifts[i] = acc;
            frac_end = i + 1;
        }
        // 指数部 ('e' 以降) に小数部のシフトを伝搬
        for (int i = frac_end; i < len; i++)
            shifts[i] = acc;
    }
}

// fg/bg colors[i] は文字 i のカラー。bg == 0 は透明。
// text_x: テキスト描画開始 x 座標 (PAD 込み)
// sep_shifts: 桁区切り用のピクセルオフセット (nullptr なら区切りなし)
static void draw_colored_spans(const char *text, int len,
                                const Fl_Color *fg, const Fl_Color *bg,
                                int text_x, int row_y, int row_h,
                                const double *sep_shifts = nullptr) {
    if (len <= 0) return;
    int baseline = row_y + (row_h + fl_height() - fl_descent() * 2) / 2;

    auto xpos = [&](int i) -> double {
        double x = fl_width(text, i);
        if (sep_shifts && i < len) x += sep_shifts[i];
        else if (sep_shifts && i == len && len > 0) x += sep_shifts[len - 1];
        return x;
    };

    // 第1パス: 背景色スパン
    for (int i = 0; i < len; ) {
        Fl_Color bc = bg[i];
        int j = i;
        while (j < len && bg[j] == bc) j++;
        if (bc != (Fl_Color)0) {
            double x1 = xpos(i);
            double x2 = xpos(j);
            fl_color(bc);
            fl_rectf(text_x + (int)x1, row_y, (int)(x2 - x1 + 0.5), row_h);
        }
        i = j;
    }

    // 第2パス: 前景色テキストスパン
    // sep_shifts 使用時は、シフト値が変わるポイントでもスパンを分割する
    // UTF-8 マルチバイトシーケンスの途中では分割しない
    for (int i = 0; i < len; ) {
        Fl_Color fc = fg[i];
        int j = i + 1;
        while (j < len && fg[j] == fc &&
               (!sep_shifts || sep_shifts[j] == sep_shifts[i]))
            j++;
        // UTF-8 継続バイト (10xxxxxx) で切らないよう後ろに延ばす
        while (j < len && ((unsigned char)text[j] & 0xC0) == 0x80) j++;
        fl_color(fc);
        fl_draw(text + i, j - i, text_x + (int)xpos(i), baseline);
        i = j;
    }
}

// 式テキスト内の数値リテラルに対するセパレータシフトを計算する。
// shifts は文字ごとの累積ピクセルオフセット。シフトが不要なら空を返す。
static void calc_expr_separator_shifts(const char *expr, int len,
                                        std::vector<double> &shifts) {
    shifts.clear();
    if (len == 0 || (!g_sep_thousands && !g_sep_hex)) return;

    struct NumTokInfo { int pos; int tlen; val_fmt_t fmt; };
    std::vector<NumTokInfo> num_tokens;

    tok_queue_t q;
    tok_queue_init(&q);
    lexer_tokenize(expr, &q);
    for (;;) {
        const token_t *peek = tok_queue_peek(&q);
        if (!peek || peek->type == TOK_EOS || peek->type == TOK_EMPTY) break;
        token_t tok = tok_queue_pop(&q);
        int p  = tok.pos;
        int tl = (int)strlen(tok.text);
        if (p >= 0 && p < len && tok.type == TOK_NUM_LIT && tok.val) {
            val_fmt_t vfmt = tok.val->fmt;
            int end = std::min(p + tl, len);
            if (vfmt == FMT_REAL || vfmt == FMT_INT ||
                vfmt == FMT_HEX || vfmt == FMT_BIN || vfmt == FMT_OCT)
                num_tokens.push_back({p, end - p, vfmt});
        }
        tok_free(&tok);
    }
    tok_queue_free(&q);

    if (num_tokens.empty()) return;

    shifts.assign(len, 0.0);
    double carry = 0.0;
    int prev_end = 0;
    for (auto &nt : num_tokens) {
        for (int i = prev_end; i < nt.pos && i < len; i++)
            shifts[i] = carry;
        std::vector<double> tok_shifts;
        calc_separator_shifts(expr + nt.pos, nt.tlen, nt.fmt, tok_shifts);
        double max_tok_shift = 0.0;
        for (int i = 0; i < nt.tlen; i++) {
            shifts[nt.pos + i] = carry + tok_shifts[i];
            if (tok_shifts[i] > max_tok_shift) max_tok_shift = tok_shifts[i];
        }
        carry += max_tok_shift;
        prev_end = nt.pos + nt.tlen;
    }
    for (int i = prev_end; i < len; i++)
        shifts[i] = carry;
}

// 文字位置 i のピクセル x 座標を返す (セパレータシフト込み)
static double char_pos_to_x(const char *text, int i, const std::vector<double> &shifts) {
    double xw = fl_width(text, i);
    if (!shifts.empty()) {
        if (i < (int)shifts.size()) xw += shifts[i];
        else if (!shifts.empty()) xw += shifts.back();
    }
    return xw;
}

// ピクセル x 座標から文字位置を返す (セパレータシフト込み)
static int x_to_char_pos(const char *text, int len, double target_x,
                          const std::vector<double> &shifts) {
    for (int i = 0; i < len; i++) {
        double cx = char_pos_to_x(text, i, shifts);
        double cw = fl_width(text + i, 1);
        if (target_x <= cx + cw / 2.0) return i;
    }
    return len;
}

// 式テキストをトークンハイライト付きで描画する。
// text_x: テキスト描画開始 x (PAD込み)
// clip_* : クリップ矩形
// sep_fmt: 桁区切り対象のフォーマット (結果値描画時のみ指定, -1 で無効)
static void draw_expr_highlighted(const char *expr,
                                   int text_x,
                                   int clip_x, int clip_y, int clip_w, int clip_h,
                                   val_fmt_t sep_fmt = (val_fmt_t)-1) {
    int len = (int)strlen(expr);
    if (len == 0) return;

    std::vector<Fl_Color> fg(len, g_colors.text);
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
                for (int i = p; i < end; i++) fg[i] = g_colors.ident;
                break;

            case TOK_BOOL_LIT:
                for (int i = p; i < end; i++) fg[i] = g_colors.special;
                break;

            case TOK_NUM_LIT:
                if (tok.val) {
                    val_fmt_t vfmt = tok.val->fmt;
                    if (vfmt == FMT_SI_PREFIX) {
                        if (end - 1 >= p) fg[end - 1] = g_colors.si_pfx;
                    } else if (vfmt == FMT_BIN_PREFIX) {
                        if (end - 2 >= p) fg[end - 2] = g_colors.si_pfx;
                        if (end - 1 >= p) fg[end - 1] = g_colors.si_pfx;
                    } else if (vfmt == FMT_WEB_COLOR) {
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
                        for (int i = p; i < end; i++) fg[i] = g_colors.special;
                    } else {
                        for (int i = p + 1; i < end; i++) {
                            if ((expr[i] == 'e' || expr[i] == 'E') &&
                                isdigit((unsigned char)expr[i-1])) {
                                for (int k = i; k < end; k++) fg[k] = g_colors.si_pfx;
                                break;
                            }
                        }
                    }
                }
                break;

            case TOK_OP:
            case TOK_KEYWORD:
                for (int i = p; i < end; i++) fg[i] = g_colors.symbol;
                break;

            case TOK_SYMBOL:
                if (tl == 1 && (tok.text[0] == '(' || tok.text[0] == ')')) {
                    int d = paren_depth;
                    if (tok.text[0] == ')' && d > 0) d--;
                    fg[p] = g_colors.paren[d % 4];
                    if (tok.text[0] == '(') paren_depth++;
                    else if (paren_depth > 0) paren_depth--;
                } else {
                    for (int i = p; i < end; i++) fg[i] = g_colors.symbol;
                }
                break;

            default:
                break;
        }
        tok_free(&tok);
    }
    tok_queue_free(&q);

    // セパレータシフト計算
    std::vector<double> shifts;

    if (sep_fmt != (val_fmt_t)-1) {
        calc_separator_shifts(expr, len, sep_fmt, shifts);
    } else {
        calc_expr_separator_shifts(expr, len, shifts);
    }

    fl_push_clip(clip_x, clip_y, clip_w, clip_h);
    fl_font(g_font_id, g_font_size);
    if (!shifts.empty()) {
        draw_colored_spans(expr, len, fg.data(), bg.data(), text_x, clip_y, clip_h, shifts.data());
    } else {
        draw_colored_spans(expr, len, fg.data(), bg.data(), text_x, clip_y, clip_h);
    }
    fl_pop_clip();
}
// SheetLineInput: シンタックスハイライト付き Fl_Input。
// editor_mode=true  → 左辺 (式編集、Up/Down/Enter を親に委譲、live_eval 呼び出し)
// editor_mode=false → 右辺 (読み取り専用、同じ描画ルール)
class SheetLineInput : public Fl_Input {
    bool      editor_mode_;
    Fl_Color  override_color_;  // 0 以外: シンタックスハイライトを使わず単色描画
    val_fmt_t result_fmt_ = FMT_REAL;  // 結果値のフォーマット (セパレータ用, 右辺のみ)

public:
    SheetLineInput(int x, int y, int w, int h, bool editor_mode = true)
        : Fl_Input(x, y, w, h), editor_mode_(editor_mode), override_color_(0)
    {
        if (!editor_mode_) readonly(1);
    }

    void set_override_color(Fl_Color c) { override_color_ = c; }
    void set_result_fmt(val_fmt_t f) { result_fmt_ = f; }

    void draw() override {
        fl_font(textfont(), textsize());

        const char *txt = value();
        int len = (int)strlen(txt);
        int p1 = std::min(insert_position(), mark());
        int p2 = std::max(insert_position(), mark());
        int sx = xscroll();
        int tx = x() + PAD - sx;

        // セパレータシフト計算
        std::vector<double> shifts;
        if (editor_mode_)
            calc_expr_separator_shifts(txt, len, shifts);
        else
            calc_separator_shifts(txt, len, result_fmt_, shifts);

        // 背景
        fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), g_colors.sel_bg);

        // 選択ハイライト背景 (フォーカスがある時のみ)
        if (p1 != p2 && Fl::focus() == this) {
            int sx1 = tx + (int)char_pos_to_x(txt, p1, shifts);
            int sx2 = tx + (int)char_pos_to_x(txt, p2, shifts);
            fl_push_clip(x(), y(), w(), h());
            fl_color(FL_SELECTION_COLOR);
            fl_rectf(sx1, y() + 1, sx2 - sx1, h() - 2);
            fl_pop_clip();
        }

        // テキスト描画
        if (override_color_ != (Fl_Color)0) {
            fl_push_clip(x(), y(), w(), h());
            fl_font(g_font_id, g_font_size);
            fl_color(override_color_);
            int baseline = y() + (h() + fl_height() - fl_descent() * 2) / 2;
            fl_draw(txt, tx, baseline);
            fl_pop_clip();
        } else {
            draw_expr_highlighted(txt, tx, x(), y(), w(), h(),
                                  editor_mode_ ? (val_fmt_t)-1 : result_fmt_);
        }

        // カーソル (非選択時のみ)
        if (Fl::focus() == this && p1 == p2) {
            int cx = tx + (int)char_pos_to_x(txt, insert_position(), shifts);
            if (cx >= x() && cx < x() + w()) {
                fl_color(cursor_color());
                fl_line(cx, y() + 2, cx, y() + h() - 2);
            }
        }
    }

    int handle(int event) override {
        if (event == FL_KEYBOARD) {
            int key   = Fl::event_key();
            bool ctrl  = Fl::event_state(FL_CTRL)  != 0;
            bool meta  = Fl::event_state(FL_COMMAND) != 0;
            bool shift = Fl::event_state(FL_SHIFT)  != 0;

            if (editor_mode_) {
                auto *sv = static_cast<SheetView *>(parent());
                // ポップアップが表示中: Up/Down/Tab/Escape を補完ナビに使う
                if (sv->popup_->is_shown()) {
                    if (key == FL_Up)   { sv->popup_->select_prev(); return 1; }
                    if (key == FL_Down) { sv->popup_->select_next(); return 1; }
                    if (key == FL_Tab || key == FL_Enter || key == FL_KP_Enter)
                        { sv->completion_confirm(); return 1; }
                    if (key == FL_Escape) { sv->completion_hide();   return 1; }
                }
                if (ctrl && key == ' ') {
                    sv->completion_update();
                    return 1;
                }
                // Tab: 左辺 → 右辺へフォーカス移動
                if (key == FL_Tab && !shift) {
                    sv->completion_hide();
                    sv->focus_result();
                    return 1;
                }
                // Shift+Tab: 前行の右辺 (なければ左辺) へ (Tab の逆方向)
                if (key == FL_Tab && shift) {
                    sv->completion_hide();
                    sv->shift_tab_from_editor();
                    return 1;
                }
            } else {
                // 右辺での Tab: 次行の左辺へ
                if (key == FL_Tab && !shift) {
                    static_cast<SheetView *>(parent())->tab_from_result();
                    return 1;
                }
                // 右辺での Shift+Tab: 同じ行の左辺へ (Tab の逆方向)
                if (key == FL_Tab && shift) {
                    static_cast<SheetView *>(parent())->shift_tab_from_result();
                    return 1;
                }
            }

            // Up/Down/Enter は左右どちらの欄からも SheetView::handle() に委譲
            if (key == FL_Up || key == FL_Down ||
                key == FL_Enter || key == FL_KP_Enter) {
                if (editor_mode_) static_cast<SheetView *>(parent())->completion_hide();
                return 0;
            }
            // Escape: ウィンドウが閉じないよう消費 (ポップアップなし時)
            if (key == FL_Escape) return 1;

            if (editor_mode_) {
                // Ctrl/Cmd+Delete/BackSpace: 行削除 → SheetView に委譲
                if ((ctrl || meta) && (key == FL_Delete || key == FL_BackSpace)) return 0;
                // Ctrl/Cmd+Shift+Up/Down: 行スワップ → SheetView に委譲
                if ((ctrl || meta) && shift && (key == FL_Up || key == FL_Down)) return 0;
                // Shift+Del/BS (修飾なし): 行削除・上移動 → SheetView に委譲
                if (shift && !ctrl && !meta && (key == FL_Delete || key == FL_BackSpace)) return 0;
                // 空行での BackSpace: 行削除・上移動 → SheetView に委譲
                // (Prefs で無効化されている場合は editor 側で吸収させる)
                if (g_input_bs_delete_empty_row &&
                    !shift && !ctrl && !meta && key == FL_BackSpace && size() == 0)
                    return 0;
            }
            // Cmd+Z / Cmd+Y / Cmd+Shift+Z は SheetView::handle() に委譲 (Undo/Redo)
            if (meta && (key == 'z' || key == 'y')) {
                return 0;
            }
        }
        // 括弧自動閉じ
        if (event == FL_KEYBOARD && editor_mode_ && g_input_auto_close_brackets) {
            const char *t = Fl::event_text();
            if (t && t[0] && !t[1]) {
                char close = 0;
                if (t[0] == '(') close = ')';
                else if (t[0] == '[') close = ']';
                else if (t[0] == '{') close = '}';
                if (close) {
                    int pos = insert_position();
                    char pair[3] = { t[0], close, '\0' };
                    replace(pos, mark(), pair, 2);
                    insert_position(pos + 1);
                    if (parent()) static_cast<SheetView *>(parent())->live_eval();
                    return 1;
                }
            }
        }

        // ペースト時: 改行なし→通常挿入、改行あり→複数行ペーストダイアログ
        if (event == FL_PASTE && editor_mode_) {
            const char *txt = Fl::event_text();
            int tlen = Fl::event_length();
            bool has_nl = false;
            for (int i = 0; i < tlen; i++) {
                if (txt[i] == '\n' || txt[i] == '\r') { has_nl = true; break; }
            }
            if (has_nl) {
                static_cast<SheetView *>(parent())->multiline_paste(
                    std::string(txt, tlen));
            } else {
                replace(insert_position(), mark(), txt, tlen);
                if (parent()) static_cast<SheetView *>(parent())->live_eval();
            }
            return 1;
        }

        // マウスクリック/ドラッグ: セパレータシフトを考慮して文字位置を計算
        if (event == FL_PUSH || event == FL_DRAG) {
            const char *txt = value();
            int len = (int)strlen(txt);
            std::vector<double> shifts;
            if (editor_mode_)
                calc_expr_separator_shifts(txt, len, shifts);
            else
                calc_separator_shifts(txt, len, result_fmt_, shifts);
            if (!shifts.empty()) {
                fl_font(textfont(), textsize());
                int sx = xscroll();
                double click_x = Fl::event_x() - x() - PAD + sx;
                int pos = x_to_char_pos(txt, len, click_x, shifts);
                if (event == FL_PUSH) {
                    if (Fl::event_state(FL_SHIFT))
                        insert_position(pos, mark());
                    else
                        insert_position(pos, pos);
                    Fl::focus(this);
                } else {
                    insert_position(pos, mark());
                }
                if (parent()) {
                    parent()->redraw();
                    if (editor_mode_)
                        static_cast<SheetView *>(parent())->completion_hide();
                }
                return 1;
            }
        }

        int ret = Fl_Input::handle(event);
        if (ret && parent()) {
            if (editor_mode_ && event == FL_KEYBOARD) {
                auto *sv = static_cast<SheetView *>(parent());
                sv->live_eval();
                if (g_input_auto_completion)
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

// --- SheetView ---
SheetView::SheetView(int x, int y, int w, int h, bool preview)
    : Fl_Group(x, y, w, h), preview_mode_(preview)
{
    box(FL_FLAT_BOX);
    color(g_colors.bg);

    model_ = sheet_model_new();
    apply_limits();
    refresh_row_views();

    // eq_pos_ / eq_w_ の初期値 (update_layout() で上書きされる)
    fl_font(g_font_id, g_font_size);
    eq_w_   = (int)fl_width("==") + 4;
    eq_pos_ = (w - sb_w_) * 3 / 5;

    if (preview_mode_) {
        vscroll_ = new Fl_Scrollbar(x + w - sb_w_, y, sb_w_, h);
        vscroll_->type(FL_VERTICAL);
        vscroll_->linesize(1);
        vscroll_->visible_focus(0);
        vscroll_->callback([](Fl_Widget *wb, void *d) {
            auto *sv = static_cast<SheetView *>(d);
            sv->scroll_top_ = static_cast<Fl_Scrollbar *>(wb)->value();
            sv->redraw();
        }, this);
        editor_ = new SheetLineInput(0, 0, 1, 1);
        editor_->hide();
        auto *rd = new SheetLineInput(0, 0, 1, 1, false);
        rd->hide();
        result_display_ = rd;
        end();
        focused_row_ = -1;
        return;
    }

    // 縦スクロールバー
    vscroll_ = new Fl_Scrollbar(x + w - sb_w_, y, sb_w_, h);
    vscroll_->type(FL_VERTICAL);
    vscroll_->linesize(1);
    vscroll_->visible_focus(0);  // Tab キー順から除外
    vscroll_->callback([](Fl_Widget *wb, void *d) {
        auto *sv = static_cast<SheetView *>(d);
        sv->scroll_top_ = static_cast<Fl_Scrollbar *>(wb)->value();
        sv->place_editor();
        sv->redraw();
    }, this);

    // フォーカス行の式エディタ
    editor_ = new SheetLineInput(x, y, expr_w(), ROW_H);
    editor_->box(FL_FLAT_BOX);
    editor_->color(g_colors.sel_bg);
    editor_->textcolor(g_colors.accent);
    editor_->textfont(g_font_id);
    editor_->textsize(g_font_size);
    editor_->cursor_color(g_colors.accent);
    editor_->when(0);

    // フォーカス行の結果表示（右辺: 読み取り専用 SheetLineInput、左辺と同じスタイル）
    auto *rd = new SheetLineInput(result_x(), y, result_w(), ROW_H, false);
    rd->box(FL_FLAT_BOX);
    rd->color(g_colors.sel_bg);
    rd->textcolor(g_colors.accent);
    rd->textfont(g_font_id);
    rd->textsize(g_font_size);
    rd->cursor_color(g_colors.accent);
    rd->when(0);
    result_display_ = rd;

    end();

    // popup_ は MainWindow が生成して set してくれる (初期値 nullptr)

    sync_scroll();
    place_editor();
}

SheetView::~SheetView() {
    sheet_model_free(model_);
    // popup_ は MainWindow が所有・削除する
}

void SheetView::apply_limits() {
    sheet_eval_limits_t lim;
    lim.max_array_length  = g_limit_max_array_length;
    lim.max_string_length = g_limit_max_string_length;
    lim.max_call_depth    = g_limit_max_call_depth;
    sheet_model_set_limits(model_, lim);
}

void SheetView::refresh_row_views() {
    int n = sheet_model_row_count(model_);
    if ((int)row_views_.size() != n) row_views_.resize(n);
}

sheet_view_state_t SheetView::capture_view_state() const {
    return { focused_row_, editor_->insert_position() };
}

void SheetView::restore_view_state(sheet_view_state_t vs) {
    int r = std::clamp(vs.focused_row, 0, sheet_model_row_count(model_) - 1);
    focus_row(r);
    editor_->insert_position(std::min(vs.cursor_pos, (int)editor_->size()));
}
void SheetView::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);
    vscroll_->resize(x + w - sb_w_, y, sb_w_, h);
    update_layout();   // ウィンドウ幅変化に合わせて = 位置を再計算
    sync_scroll();
    place_editor();
}

void SheetView::set_sb_w(int w) {
    if (w < 4) w = 4;
    if (w == sb_w_) return;
    sb_w_ = w;
    vscroll_->resize(x() + this->w() - sb_w_, y(), sb_w_, h());
    update_layout();
    sync_scroll();
    place_editor();
    redraw();
}

int SheetView::row_at_y(int fy) const {
    int n = sheet_model_row_count(model_);
    int cum = 0;
    for (int i = scroll_top_; i < n; i++) {
        cum += row_h(i);
        if (fy < cum) return i;
    }
    return n - 1;
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
    bool wrapped    = (focused_row_ < (int)row_views_.size()) && row_views_[focused_row_].wrapped;
    const char *res = sheet_model_row_result(model_, focused_row_);
    bool visible    = sheet_model_row_visible(model_, focused_row_);

    editor_->show();
    if (wrapped) {
        // 折り返し: 式は上段 full-width、結果は下段右カラム
        editor_->resize(x(), ry, sheet_w(), ROW_H);
        update_result_display();
        result_display_->resize(result_x(), ry + ROW_H, result_w(), ROW_H);
    } else if (!res || !res[0] || !visible) {
        // 結果なし / 非表示: エディタは全幅
        editor_->resize(x(), ry, sheet_w(), ROW_H);
        update_result_display();  // result_display_ を hide する
    } else {
        editor_->resize(x(), ry, expr_w(), ROW_H);
        update_result_display();
        result_display_->resize(result_x(), ry, result_w(), ROW_H);
    }
}

void SheetView::sync_scroll() {
    int n = sheet_model_row_count(model_);
    // 総ピクセル高を求める
    int total_px = 0;
    for (int i = 0; i < n; i++) total_px += row_h(i);

    bool need_sb = total_px > h();
    bool sb_changed = false;
    if (need_sb && !vscroll_->visible()) { vscroll_->show(); place_editor(); sb_changed = true; }
    if (!need_sb && vscroll_->visible()) { vscroll_->hide(); scroll_top_ = 0; place_editor(); sb_changed = true; }
    if (sb_changed) update_layout();

    // 下から積み上げて h() に収まる最大 scroll_top_ を求める
    int cum = 0, max_top = 0;
    for (int i = n - 1; i >= 0; i--) {
        cum += row_h(i);
        if (cum > h()) { max_top = i + 1; break; }
    }
    scroll_top_ = std::clamp(scroll_top_, 0, max_top);
    // スライダサイズ: 表示ピクセル / 総ピクセル
    int vis_px = std::min(h(), total_px);
    vscroll_->bounds(0, max_top);
    vscroll_->slider_size(total_px > 0 ? (double)vis_px / total_px : 1.0);
    vscroll_->value(scroll_top_);
}

// 結果表示ウィジェットを focused_row_ の内容に合わせて更新
void SheetView::update_result_display() {
    auto *rd = static_cast<SheetLineInput *>(result_display_);

    int n = sheet_model_row_count(model_);
    if (focused_row_ < 0 || focused_row_ >= n) {
        rd->hide();
        return;
    }
    const char *result = sheet_model_row_result(model_, focused_row_);
    bool visible       = sheet_model_row_visible(model_, focused_row_);
    bool error         = sheet_model_row_error(model_, focused_row_);
    val_fmt_t fmt      = sheet_model_row_fmt(model_, focused_row_);
    size_t rlen        = result ? strlen(result) : 0;
    if (rlen == 0 || !visible) {
        rd->hide();
        return;
    }

    rd->value(result);

    if (error) {
        // エラーは単色描画
        rd->color(g_colors.sel_bg);
        rd->set_override_color(g_colors.error);
    } else if (fmt == FMT_WEB_COLOR && rlen == 7 && result[0] == '#') {
        // WebColor: 背景をその色に、シンタックスハイライトは無効化してテキスト色を調整
        unsigned int rgb = 0;
        if (sscanf(result + 1, "%6x", &rgb) == 1) {
            int r8 = (rgb >> 16) & 0xFF;
            int g8 = (rgb >>  8) & 0xFF;
            int b8 =  rgb        & 0xFF;
            rd->color(fl_rgb_color(r8, g8, b8));
            int lum = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
            rd->set_override_color(lum < 128 ? FL_WHITE : FL_BLACK);
        }
    } else {
        // 通常: シンタックスハイライト (draw_expr_highlighted が自動判定)
        rd->color(g_colors.sel_bg);
        rd->set_override_color((Fl_Color)0);
        rd->set_result_fmt(fmt);
    }
    rd->show();
    rd->redraw();
}
void SheetView::eval_all() {
    apply_limits();
    sheet_model_eval_all(model_);
    refresh_row_views();
    update_layout();  // 結果が変わったので = 位置を再計算

    char *snap = sheet_model_build_snapshot(model_);
    if (snap) {
        crash_handler_save_sheet(snap);
        free(snap);
    }
}
// 移植元: Calctus/UI/SheetView.cs - validateLayout()
// 全行の式幅・答え幅を計測し、最大値から "=" カラム位置を算出する。
// セパレータシフト込みの表示幅を返す
static int display_width_with_sep(const char *text, const std::vector<double> &shifts) {
    int len = (int)strlen(text);
    if (len == 0) return 0;
    double w = fl_width(text);
    if (!shifts.empty()) w += shifts.back();
    return (int)w;
}

void SheetView::update_layout() {
    fl_font(g_font_id, g_font_size);

    // "=" カラム幅: オリジナルは "==" の文字幅
    eq_w_ = (int)fl_width("==") + 4;

    int avail = sheet_w();
    int min_eq_pos = std::min(eq_w_, avail / 5);

    int max_expr_w = 0;
    int max_ans_w  = 0;
    bool has_result = false;
    int n = sheet_model_row_count(model_);
    for (int i = 0; i < n; i++) {
        const char *expr = sheet_model_row_expr(model_, i);
        const char *res  = sheet_model_row_result(model_, i);
        bool error       = sheet_model_row_error(model_, i);
        bool visible     = sheet_model_row_visible(model_, i);
        val_fmt_t rfmt   = sheet_model_row_fmt(model_, i);
        if (!res[0] || !visible) continue;
        has_result = true;
        if (expr[0]) {
            std::vector<double> shifts;
            calc_expr_separator_shifts(expr, (int)strlen(expr), shifts);
            max_expr_w = std::max(max_expr_w, display_width_with_sep(expr, shifts) + PAD * 2);
        }
        if (!error) {
            std::vector<double> shifts;
            calc_separator_shifts(res, (int)strlen(res), rfmt, shifts);
            max_ans_w = std::max(max_ans_w, display_width_with_sep(res, shifts) + PAD * 2);
        }
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
    if ((int)row_views_.size() != n) row_views_.resize(n);
    for (int i = 0; i < n; i++) {
        const char *expr = sheet_model_row_expr(model_, i);
        const char *res  = sheet_model_row_result(model_, i);
        bool visible     = sheet_model_row_visible(model_, i);
        if (!res[0] || !expr[0] || !visible) { row_views_[i].wrapped = false; continue; }
        std::vector<double> shifts;
        calc_expr_separator_shifts(expr, (int)strlen(expr), shifts);
        row_views_[i].wrapped = display_width_with_sep(expr, shifts) > eq_pos_;
    }
}

void SheetView::preview_set_exprs(const std::vector<std::string> &exprs) {
    std::vector<const char *> ptrs;
    ptrs.reserve(exprs.size());
    for (auto &e : exprs) ptrs.push_back(e.c_str());
    sheet_model_set_rows(model_, ptrs.empty() ? nullptr : ptrs.data(), (int)ptrs.size());
    eval_all();
    sync_scroll();
    redraw();
}

void SheetView::apply_font() {
    editor_->textfont(g_font_id);
    editor_->textsize(g_font_size);
    result_display_->textfont(g_font_id);
    result_display_->textsize(g_font_size);
    if (popup_) popup_->apply_colors();
    update_layout();
    redraw();
}

void SheetView::live_eval() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ >= 0 && focused_row_ < n) {
        sheet_model_set_row_expr_raw(model_, focused_row_, editor_->value());
    }
    eval_all();          // update_layout() を内部で呼ぶ
    place_editor();      // = 位置が変わった場合にウィジェットを再配置
    update_result_display();
    redraw();
    if (row_change_cb_) row_change_cb_(row_change_data_);
}

bool SheetView::has_uncommitted_edit() const {
    if (focused_row_ < 0 || focused_row_ >= sheet_model_row_count(model_)) return false;
    return std::string(editor_->value()) != original_expr_;
}

void SheetView::commit() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ >= 0 && focused_row_ < n) {
        const std::string new_expr(editor_->value());
        if (new_expr != original_expr_) {
            const std::string old_expr = original_expr_;
            sheet_op_t undo_op { SHEET_OP_CHANGE_EXPR, focused_row_, old_expr.c_str() };
            sheet_op_t redo_op { SHEET_OP_CHANGE_EXPR, focused_row_, new_expr.c_str() };
            sheet_view_state_t vs = capture_view_state();
            sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
            original_expr_ = new_expr;  // 二重登録防止 (commit 後)
            refresh_row_views();
            update_layout();
            sync_scroll();
            place_editor();
            update_result_display();
            redraw();
            if (row_change_cb_) row_change_cb_(row_change_data_);
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
    int n = sheet_model_row_count(model_);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    focused_row_ = idx;
    const char *expr = sheet_model_row_expr(model_, idx);
    original_expr_   = expr;
    editor_->value(expr);
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

const char *SheetView::current_fmt_name() const {
    return sheet_model_current_fmt_name(model_, focused_row_);
}

// 編集中の内容を model に先に反映 (insert/delete 前の準備)
void SheetView::flush_editor_to_model() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ >= 0 && focused_row_ < n)
        sheet_model_set_row_expr_raw(model_, focused_row_, editor_->value());
}

void SheetView::insert_row(int after) {
    flush_editor_to_model();
    int n = sheet_model_row_count(model_);
    int ins = std::min(after + 1, n);
    sheet_op_t undo_op { SHEET_OP_DELETE, ins, "" };
    sheet_op_t redo_op { SHEET_OP_INSERT, ins, "" };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
    refresh_row_views();
    update_layout();
    sync_scroll();
    focus_row(ins);
}

void SheetView::delete_row(int idx, bool move_up) {
    flush_editor_to_model();
    int n = sheet_model_row_count(model_);
    if (n <= 1) {
        const char *cur = sheet_model_row_expr(model_, 0);
        if (!cur || !cur[0]) return;
        std::string old_expr = cur;
        sheet_op_t undo_op { SHEET_OP_CHANGE_EXPR, 0, old_expr.c_str() };
        sheet_op_t redo_op { SHEET_OP_CHANGE_EXPR, 0, "" };
        sheet_view_state_t vs = capture_view_state();
        sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
        refresh_row_views();
        update_layout();
        sync_scroll();
        focus_row(0);
        return;
    }
    std::string deleted_expr = sheet_model_row_expr(model_, idx);
    sheet_op_t undo_op { SHEET_OP_INSERT, idx, deleted_expr.c_str() };
    sheet_op_t redo_op { SHEET_OP_DELETE, idx, "" };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
    refresh_row_views();
    update_layout();
    sync_scroll();
    int n_after = sheet_model_row_count(model_);
    int target = move_up ? std::max(0, idx - 1)
                         : std::min(idx, n_after - 1);
    focus_row(target);
}

void SheetView::apply_fmt(const char *func_name) {
    int n = sheet_model_row_count(model_);
    if (focused_row_ < 0 || focused_row_ >= n) return;
    char *body_c = sheet_model_strip_formatter(editor_->value());
    std::string body = body_c ? body_c : "";
    free(body_c);
    std::string new_expr = (func_name && func_name[0])
                           ? std::string(func_name) + "(" + body + ")"
                           : body;
    editor_->value(new_expr.c_str());
    editor_->insert_position(editor_->size());
    commit();
}
void SheetView::draw() {
    fl_push_no_clip();
    fl_push_clip(x(), y(), w(), h());

    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), g_colors.bg);

    const int ew  = expr_w();
    const int eqx = eq_col_x();
    const int rx  = result_x();
    const int rw  = result_w();

    fl_font(g_font_id, g_font_size);

    // 結果値を指定位置に描画するラムダ
    auto draw_result_at = [&](const char *result, val_fmt_t rfmt, bool error,
                               bool visible, int ary, int abaseline) {
        if (!result || !result[0] || !visible) return;
        size_t rlen = strlen(result);
        if (error) {
            fl_push_clip(rx, ary, rw, ROW_H);
            fl_font(g_font_id, g_font_size);
            fl_color(g_colors.error);
            fl_draw(result, rx + PAD, abaseline);
            fl_pop_clip();
        } else if (rfmt == FMT_WEB_COLOR && rlen == 7 && result[0] == '#') {
            unsigned int rgb = 0;
            if (sscanf(result + 1, "%6x", &rgb) == 1) {
                int r8 = (rgb >> 16) & 0xFF;
                int g8 = (rgb >>  8) & 0xFF;
                int b8 =  rgb        & 0xFF;
                Fl_Color bc = fl_rgb_color(r8, g8, b8);
                int lum = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
                Fl_Color fc = lum < 128 ? FL_WHITE : FL_BLACK;
                int text_w = std::min((int)fl_width(result) + PAD * 2, rw);
                fl_draw_box(FL_FLAT_BOX, rx, ary, text_w, ROW_H, bc);
                fl_push_clip(rx, ary, rw, ROW_H);
                fl_font(g_font_id, g_font_size);
                fl_color(fc);
                fl_draw(result, rx + PAD, abaseline);
                fl_pop_clip();
            }
        } else {
            draw_expr_highlighted(result, rx + PAD, rx, ary, rw, ROW_H, rfmt);
        }
    };

    int ry = y();
    int n = sheet_model_row_count(model_);
    for (int i = scroll_top_; i < n; i++) {
        if (ry >= y() + h()) break;
        const char *expr   = sheet_model_row_expr(model_, i);
        const char *result = sheet_model_row_result(model_, i);
        val_fmt_t   rfmt   = sheet_model_row_fmt(model_, i);
        bool        error  = sheet_model_row_error(model_, i);
        bool        visible= sheet_model_row_visible(model_, i);
        bool        wrapped = (i < (int)row_views_.size()) && row_views_[i].wrapped;
        int         rh     = row_h(i);

        if (wrapped) {
            int ry2    = ry + ROW_H;
            int bl_bot = ry2 + (ROW_H + fl_height() - fl_descent() * 2) / 2;

            if (i == focused_row_)
                fl_draw_box(FL_FLAT_BOX, x(), ry, sheet_w(), rh, g_colors.sel_bg);

            if (expr[0])
                draw_expr_highlighted(expr, x() + PAD, x(), ry, sheet_w(), ROW_H);

            if (g_show_rowlines) {
                fl_color(g_colors.rowline);
                fl_line(x(), ry + ROW_H - 1, x() + sheet_w(), ry + ROW_H - 1);
            }

            if (result[0] && !error && visible) {
                fl_font(g_font_id, g_font_size);
                fl_color(g_colors.symbol);
                fl_draw("=", eqx + (eq_w_ - (int)fl_width("=")) / 2, bl_bot);
            }

            draw_result_at(result, rfmt, error, visible, ry2, bl_bot);

            if (g_show_rowlines) {
                fl_color(g_colors.rowline);
                fl_line(x(), ry2 + ROW_H - 1, x() + sheet_w(), ry2 + ROW_H - 1);
            }

        } else {
            int baseline = ry + (ROW_H + fl_height() - fl_descent() * 2) / 2;
            bool has_result = result[0] && visible;

            if (i == focused_row_)
                fl_draw_box(FL_FLAT_BOX, x(), ry, has_result ? ew + eq_w_ : sheet_w(), ROW_H, g_colors.sel_bg);

            if (expr[0]) {
                if (has_result)
                    draw_expr_highlighted(expr, x() + PAD, x(), ry, ew, ROW_H);
                else
                    draw_expr_highlighted(expr, x() + PAD, x(), ry, sheet_w(), ROW_H);
            }

            if (has_result && !error) {
                fl_font(g_font_id, g_font_size);
                fl_color(g_colors.symbol);
                fl_draw("=", eqx + (eq_w_ - (int)fl_width("=")) / 2, baseline);
            }

            draw_result_at(result, rfmt, error, visible, ry, baseline);

            if (g_show_rowlines) {
                fl_color(g_colors.rowline);
                fl_line(x(), ry + ROW_H - 1, x() + sheet_w(), ry + ROW_H - 1);
            }
        }

        ry += rh;
    }

    // 子ウィジェット (editor_, result_display_, vscroll_) を描画
    draw_children();

    fl_pop_clip();
    fl_pop_clip();
}
int SheetView::handle(int event) {
    if (preview_mode_) {
        if (event == FL_MOUSEWHEEL) {
            int n = sheet_model_row_count(model_);
            scroll_top_ = std::clamp(scroll_top_ + Fl::event_dy(), 0,
                                     std::max(0, n - 1));
            sync_scroll();
            redraw();
            return 1;
        }
        return Fl_Group::handle(event);
    }
    if (event == FL_KEYBOARD) {
        int key   = Fl::event_key();
        bool shift = Fl::event_state(FL_SHIFT) != 0;
        bool ctrl  = Fl::event_state(FL_CTRL)  != 0;

        bool meta = Fl::event_state(FL_COMMAND) != 0;
        bool cmd  = meta || ctrl;  // macOS=Command, Win/Linux=Ctrl どちらでも反応
        if (meta && key == 'z' && !shift) { undo(); return 1; }
        if (meta && (key == 'y' || (key == 'z' && shift))) { redo(); return 1; }

        // ---- Cmd/Ctrl + Shift + ... ----
        if (cmd && shift) {
            if (key == FL_Up)   { move_row_up();   return 1; }
            if (key == FL_Down) { move_row_down(); return 1; }
            if (key == FL_Delete || key == FL_BackSpace) { clear_all(); return 1; }
            if (key == 'c' || key == 'C') { copy_all_to_clipboard(); return 1; }
            if (key == 'n' || key == 'N') {
                // 現在時刻を epoch 秒としてカーソル位置に挿入
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)time(nullptr));
                int p = editor_->insert_position(), m = editor_->mark();
                editor_->replace(std::min(p,m), std::max(p,m), buf, (int)strlen(buf));
                editor_->insert_position((int)(std::min(p,m) + strlen(buf)));
                live_eval();
                return 1;
            }
        }

        // ---- Enter ----
        if ((key == FL_Enter || key == FL_KP_Enter) && !cmd) {
            commit();
            if (shift) {
                insert_row(focused_row_ - 1);
            } else {
                int n = sheet_model_row_count(model_);
                const char *res = (focused_row_ >= 0 && focused_row_ < n)
                                  ? sheet_model_row_result(model_, focused_row_) : "";
                bool error   = (focused_row_ >= 0 && focused_row_ < n)
                                  && sheet_model_row_error(model_, focused_row_);
                bool visible = (focused_row_ >= 0 && focused_row_ < n)
                                  && sheet_model_row_visible(model_, focused_row_);
                bool has_result = res && res[0] && !error && visible;
                insert_row(focused_row_);
                if (has_result) {
                    editor_->value("ans");
                    editor_->insert_position(3, 0);
                    live_eval();
                }
            }
            redraw();
            return 1;
        }

        // ---- 行削除（上へ移動） ----
        if (shift && !ctrl && (key == FL_Delete || key == FL_BackSpace)) {
            delete_row_up(); return 1;
        }
        if (key == FL_BackSpace && !shift && !ctrl && editor_->size() == 0
            && g_input_bs_delete_empty_row) {
            delete_row_up(); return 1;
        }

        // ---- 上下移動 ----
        if (key == FL_Up && !ctrl) {
            commit();
            if (focused_row_ > 0) focus_row(focused_row_ - 1);
            return 1;
        }
        if (key == FL_Down && !ctrl) {
            commit();
            if (focused_row_ + 1 >= sheet_model_row_count(model_)) {
                insert_row(focused_row_);
            } else {
                focus_row(focused_row_ + 1);
            }
            return 1;
        }

        // ---- Ctrl+Del/BS: 行削除（元の動作を維持） ----
        if (ctrl && !shift && (key == FL_Delete || key == FL_BackSpace)) {
            delete_row(focused_row_);
            return 1;
        }

        // ---- フォーマットショートカット ----
        if (key == FL_F + 8)  { apply_fmt(nullptr);  return 1; }  // Auto
        if (key == FL_F + 9)  { apply_fmt("dec");    return 1; }
        if (key == FL_F + 10) { apply_fmt("hex");    return 1; }
        if (key == FL_F + 11) { apply_fmt("bin");    return 1; }
        if (key == FL_F + 12) { apply_fmt("si");     return 1; }

        // ---- F5: 強制再計算 ----
        if (key == FL_F + 5) {
            eval_all();
            redraw();
            return 1;
        }
    }

    if (event == FL_PUSH) {
        int ex = Fl::event_x();
        int ey = Fl::event_y();
        if (ex < x() + sheet_w()) {
            int n = sheet_model_row_count(model_);
            int clicked = row_at_y(ey - y());
            if (clicked >= 0 && clicked < n && clicked != focused_row_) {
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
bool SheetView::load_file(const char *path) {
    if (!sheet_model_load_file(model_, path)) return false;
    refresh_row_views();
    update_layout();
    scroll_top_  = 0;
    focused_row_ = 0;
    sync_scroll();
    focus_row(0);
    redraw();
    return true;
}

bool SheetView::save_file(const char *path) {
    return sheet_model_save_file(model_, path);
}

// Undo / Redo は sheet_model に委譲
void SheetView::undo() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ >= 0 && focused_row_ < n) {
        const std::string current(editor_->value());
        if (current != original_expr_) {
            // 未コミット編集を先に commit → undo スタックに積む。
            // 続く sheet_model_undo() がそれを pop し、typed 内容は redo スタック側に
            // 残るので Ctrl+Y で復元できる。
            commit();
        }
    }
    sheet_view_state_t vs;
    if (!sheet_model_undo(model_, &vs)) return;
    refresh_row_views();
    update_layout();
    sync_scroll();
    restore_view_state(vs);
    redraw();
}

void SheetView::redo() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ >= 0 && focused_row_ < n) {
        const std::string current(editor_->value());
        if (current != original_expr_) {
            // 未コミット編集を先に commit (commit 内で redo スタックは truncate される)。
            // 結果として直後の sheet_model_redo は no-op となり、typing が黙って
            // 消える事故を防ぐ。
            commit();
        }
    }
    sheet_view_state_t vs;
    if (!sheet_model_redo(model_, &vs)) return;
    refresh_row_views();
    update_layout();
    sync_scroll();
    restore_view_state(vs);
    redraw();
}

void SheetView::test_type_and_commit(const char *expr) {
    editor_->value(expr);
    commit();
}

// --- 入力補完 ---
// sheet_model から C 形式の候補リストを取得し、C++ 側 Candidate に変換する。
static std::vector<Candidate> build_candidates_from_model(sheet_model_t *model) {
    const sheet_candidate_t *arr = nullptr;
    int n = sheet_model_build_candidates(model, &arr);
    std::vector<Candidate> out;
    out.reserve(n);
    for (int i = 0; i < n; i++) {
        out.push_back({ arr[i].id ? arr[i].id : "",
                        arr[i].label ? arr[i].label : "",
                        arr[i].description ? arr[i].description : "",
                        arr[i].is_function });
    }
    return out;
}

void SheetView::completion_update() {
    if (!editor_->visible()) return;

    const char *text = editor_->value();
    int pos = editor_->insert_position();

    if (pos == 0 || !lexer_is_id_follow(text[pos - 1])) {
        completion_hide();
        return;
    }

    int start = pos;
    while (start > 0 && lexer_is_id_follow(text[start - 1])) start--;

    if (!lexer_is_id_start(text[start])) {
        completion_hide();
        return;
    }

    std::string key(text + start, pos - start);

    if (!popup_->is_shown()) {
        popup_->set_all(build_candidates_from_model(model_));

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
    int pos = editor_->insert_position();

    // 識別子の先頭を探す
    int start = pos;
    while (start > 0 && lexer_is_id_follow(text[start - 1])) start--;

    // 置換テキストを構築
    std::string rep = c->id;
    if (c->is_function) rep += "(";

    editor_->replace(start, pos, rep.c_str(), (int)rep.size());
    // カーソルを括弧内 (関数) または末尾 (変数) に置く
    editor_->insert_position((int)(start + rep.size()));

    completion_hide();
    live_eval();
}

void SheetView::completion_hide() {
    if (popup_->is_shown()) popup_->hide_popup();
}

// ---- 行削除・移動 ----

void SheetView::delete_row_up() {
    delete_row(focused_row_, true);
}

void SheetView::insert_row_below() {
    commit();
    insert_row(focused_row_);
    redraw();
}

void SheetView::insert_row_above() {
    commit();
    insert_row(focused_row_ - 1);
    redraw();
}

void SheetView::delete_current_row() {
    delete_row(focused_row_);
}

void SheetView::move_row_up() {
    if (focused_row_ <= 0) return;
    flush_editor_to_model();
    int a = focused_row_ - 1, b = focused_row_;
    std::string ea = sheet_model_row_expr(model_, a);
    std::string eb = sheet_model_row_expr(model_, b);
    sheet_op_t undo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, ea.c_str() },
        { SHEET_OP_CHANGE_EXPR, b, eb.c_str() },
    };
    sheet_op_t redo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, eb.c_str() },
        { SHEET_OP_CHANGE_EXPR, b, ea.c_str() },
    };
    sheet_model_commit(model_, undo_ops, 2, redo_ops, 2, capture_view_state());
    refresh_row_views();
    update_layout();
    sync_scroll();
    focus_row(a);
}

void SheetView::move_row_down() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ < 0 || focused_row_ >= n - 1) return;
    flush_editor_to_model();
    int a = focused_row_, b = focused_row_ + 1;
    std::string ea = sheet_model_row_expr(model_, a);
    std::string eb = sheet_model_row_expr(model_, b);
    sheet_op_t undo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, ea.c_str() },
        { SHEET_OP_CHANGE_EXPR, b, eb.c_str() },
    };
    sheet_op_t redo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, eb.c_str() },
        { SHEET_OP_CHANGE_EXPR, b, ea.c_str() },
    };
    sheet_model_commit(model_, undo_ops, 2, redo_ops, 2, capture_view_state());
    refresh_row_views();
    update_layout();
    sync_scroll();
    focus_row(b);
}

void SheetView::clear_all() {
    flush_editor_to_model();
    int n = sheet_model_row_count(model_);
    const char *first = sheet_model_row_expr(model_, 0);
    if (n == 1 && (!first || !first[0])) return;

    // 行の式を文字列として保持 (sheet_op_t は const char* を参照するため)
    std::vector<std::string> exprs;
    exprs.reserve(n);
    for (int i = 0; i < n; i++)
        exprs.push_back(sheet_model_row_expr(model_, i));

    std::vector<sheet_op_t> undo_ops;
    undo_ops.push_back({ SHEET_OP_CHANGE_EXPR, 0, exprs[0].c_str() });
    for (int i = 1; i < n; i++)
        undo_ops.push_back({ SHEET_OP_INSERT, i, exprs[i].c_str() });

    std::vector<sheet_op_t> redo_ops;
    redo_ops.push_back({ SHEET_OP_CHANGE_EXPR, 0, "" });
    for (int i = n - 1; i >= 1; i--)
        redo_ops.push_back({ SHEET_OP_DELETE, i, "" });

    sheet_model_commit(model_,
                       undo_ops.data(), (int)undo_ops.size(),
                       redo_ops.data(), (int)redo_ops.size(),
                       capture_view_state());
    refresh_row_views();
    update_layout();
    sync_scroll();
    focus_row(0);
}

// 移植元: Calctus/UI/Sheets/SheetView.cs - MultilinePaste()
void SheetView::multiline_paste(const std::string &text) {
    PasteOptionForm *dlg = new PasteOptionForm(text);
    bool ok = dlg->run();
    std::vector<std::string> lines;
    if (ok) lines = dlg->result_lines();
    delete dlg;

    if (!ok || lines.empty()) return;

    commit();  // 現在行の編集を確定

    int insert_at = focused_row_;

    std::vector<sheet_op_t> redo_ops;
    redo_ops.reserve(lines.size());
    for (int i = 0; i < (int)lines.size(); i++)
        redo_ops.push_back({ SHEET_OP_INSERT, insert_at + i, lines[i].c_str() });

    std::vector<sheet_op_t> undo_ops;
    undo_ops.reserve(lines.size());
    for (int i = (int)lines.size() - 1; i >= 0; i--)
        undo_ops.push_back({ SHEET_OP_DELETE, insert_at + i, "" });

    sheet_model_commit(model_,
                       undo_ops.data(), (int)undo_ops.size(),
                       redo_ops.data(), (int)redo_ops.size(),
                       capture_view_state());
    refresh_row_views();
    update_layout();
    sync_scroll();
    focus_row(insert_at + (int)lines.size() - 1);
    place_editor();
    redraw();
}

void SheetView::copy_all_to_clipboard() const {
    std::string text;
    int n = sheet_model_row_count(model_);
    for (int i = 0; i < n; i++) {
        const char *expr   = sheet_model_row_expr(model_, i);
        const char *result = sheet_model_row_result(model_, i);
        text += expr ? expr : "";
        if (result && result[0]) {
            text += " = ";
            text += result;
        }
        text += "\n";
    }
    Fl::copy(text.c_str(), (int)text.size(), 1);
}

void SheetView::focus_result() {
    if (result_display_->visible()) {
        Fl::focus(result_display_);
        result_display_->insert_position(result_display_->size(), 0);
        redraw();
    } else {
        commit();
        if (focused_row_ + 1 >= sheet_model_row_count(model_))
            insert_row(focused_row_);
        else
            focus_row(focused_row_ + 1);
    }
}

void SheetView::tab_from_result() {
    commit();
    if (focused_row_ + 1 >= sheet_model_row_count(model_))
        insert_row(focused_row_);
    else
        focus_row(focused_row_ + 1);
}

void SheetView::shift_tab_from_editor() {
    commit();
    if (focused_row_ <= 0) return;  // 最上段では何もしない
    focus_row(focused_row_ - 1);
    // focus_row は editor_ にフォーカスを置くので、前行に可視な結果があれば
    // result_display_ に上書きして Tab 順序を反転させる。
    if (result_display_->visible()) {
        Fl::focus(result_display_);
        result_display_->insert_position(result_display_->size(), 0);
        redraw();
    }
}

void SheetView::shift_tab_from_result() {
    // 同じ行の左辺に戻す (focused_row_ は変えない)。通常の Tab 移動と同様、
    // カーソルは末尾に置き選択状態は作らない (insert_position の単引数版)。
    Fl::focus(editor_);
    editor_->insert_position(editor_->size());
    redraw();
}
