// see sheet_highlight.h. SheetView と SheetLineInput の両方から呼ばれる。

#include "sheet_highlight.h"
#include "colors.h"
#include "settings_globals.h"
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>

extern "C" {
#include "parser/lexer.h"
}

void calc_separator_shifts(const char *text, int len, val_fmt_t fmt,
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

    // 整数部の終端を探す (小数点、 'e', 'E', 末尾)
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

    // 整数部以降 (小数点、 小数部、 指数部) に整数部のシフトを伝搬
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

void draw_colored_spans(const char *text, int len,
                         const Fl_Color *fg, const Fl_Color *bg,
                         int text_x, int row_y, int row_h,
                         const double *sep_shifts) {
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

void calc_expr_separator_shifts(const char *expr, int len,
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

double char_pos_to_x(const char *text, int i, const std::vector<double> &shifts) {
    double xw = fl_width(text, i);
    if (!shifts.empty()) {
        if (i < (int)shifts.size()) xw += shifts[i];
        else if (!shifts.empty()) xw += shifts.back();
    }
    return xw;
}

int x_to_char_pos(const char *text, int len, double target_x,
                   const std::vector<double> &shifts) {
    for (int i = 0; i < len; i++) {
        double cx = char_pos_to_x(text, i, shifts);
        double cw = fl_width(text + i, 1);
        if (target_x <= cx + cw / 2.0) return i;
    }
    return len;
}

void draw_expr_highlighted(const char *expr,
                            int text_x,
                            int clip_x, int clip_y, int clip_w, int clip_h,
                            val_fmt_t sep_fmt) {
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
