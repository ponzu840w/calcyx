/* 移植元: Calctus/Model/Parsers/Lexer.cs
 *          Calctus/Model/Formats/ (各フォーマッタの Parse メソッド) */

#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* --- token_t --- */

void tok_free(token_t *t) {
    if (t && t->val) {
        val_free(t->val);
        t->val = NULL;
    }
}

/* --- 内部ヘルパー --- */

static bool is_digit_base(char c, int base) {
    if (base <= 10) return c >= '0' && c < '0' + base;
    return isdigit(c) || (tolower(c) >= 'a' && tolower(c) < 'a' + base - 10);
}

/* DigitLen: s[0..] から数字部 (アンダースコア区切り込み) を読む */
static int read_digits(const char *s, int base) {
    int i = 0;
    if (!is_digit_base(s[i], base)) return 0;
    while (is_digit_base(s[i], base) || (s[i] == '_' && is_digit_base(s[i+1], base)))
        i++;
    return i;
}

/* アンダースコアを除去して整数文字列を作る */
static int strip_underscores(const char *src, int len, char *dst, int dstlen) {
    int j = 0;
    for (int i = 0; i < len && j < dstlen - 1; i++) {
        if (src[i] != '_') dst[j++] = src[i];
    }
    dst[j] = '\0';
    return j;
}

/* 省略可能な指数部 e±nnn を読む。戻り値はその長さ (0 なら指数なし) */
static int read_exponent(const char *s) {
    if (s[0] != 'e' && s[0] != 'E') return 0;
    int i = 1;
    if (s[i] == '+' || s[i] == '-') i++;
    int dlen = read_digits(s + i, 10);
    if (dlen == 0) return 0;
    return i + dlen;
}

/* --- SI prefix (ryzafpnum_kMGTPEZYR) と対応する指数 --- */
static const char SI_PREFIXES[] = "ryzafpnum_kMGTPEZYR";
#define SI_OFFSET 9
static int si_prefix_index(char c) {
    if (c == '\0') return INT32_MIN;  /* strchr が null terminator にマッチするのを防ぐ */
    const char *p = strchr(SI_PREFIXES, c);
    if (!p) return INT32_MIN;
    return (int)(p - SI_PREFIXES) - SI_OFFSET;
}

/* --- Binary prefix (_kMGTPEZYR) と対応する指数 --- */
static const char BIN_PREFIXES[] = "_kMGTPEZYR";
static int bin_prefix_index(char c) {
    const char *p = strchr(BIN_PREFIXES, c);
    if (!p) return -1;
    return (int)(p - BIN_PREFIXES);
}

/* --- 数値フォーマット試行関数 ---
 * 戻り値: マッチした文字数 (0 = マッチなし)
 * out_val: 成功時に値をセット
 */

/* CStyleHex: 0[xX][0-9a-fA-F]+ */
static int try_hex(const char *s, val_t **out) {
    if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) return 0;
    int dlen = read_digits(s + 2, 16);
    if (dlen == 0) return 0;
    int total = 2 + dlen;
    char digits[64]; strip_underscores(s + 2, dlen, digits, sizeof(digits));
    int64_t v = (int64_t)strtoull(digits, NULL, 16);
    *out = val_new_i64(v, FMT_HEX);
    return total;
}

/* CStyleBin: 0[bB][01]+ */
static int try_bin(const char *s, val_t **out) {
    if (s[0] != '0' || (s[1] != 'b' && s[1] != 'B')) return 0;
    int dlen = read_digits(s + 2, 2);
    if (dlen == 0) return 0;
    int total = 2 + dlen;
    char digits[128]; strip_underscores(s + 2, dlen, digits, sizeof(digits));
    int64_t v = (int64_t)strtoull(digits, NULL, 2);
    *out = val_new_i64(v, FMT_BIN);
    return total;
}

/* CStyleOct: 0[0-7]+ */
static int try_oct(const char *s, val_t **out) {
    if (s[0] != '0') return 0;
    int dlen = read_digits(s + 1, 8);
    if (dlen == 0) return 0;
    int total = 1 + dlen;
    char digits[64]; strip_underscores(s + 1, dlen, digits, sizeof(digits));
    int64_t v = (int64_t)strtoll(digits, NULL, 8);
    *out = val_new_i64(v, FMT_OCT);
    return total;
}

/* CStyleReal: [0-9]*\.[0-9]+([eE][+-]?[0-9]+)?
 * (整数部は省略可能だが小数部必須) */
static int try_real(const char *s, val_t **out) {
    int i = 0;
    int int_len = read_digits(s, 10);
    i += int_len;
    if (s[i] != '.') return 0;
    i++;
    int frac_len = read_digits(s + i, 10);
    if (frac_len == 0) return 0;
    i += frac_len;
    int exp_len = read_exponent(s + i);
    i += exp_len;
    /* 文字列から real_t を作る */
    char buf[128]; strip_underscores(s, i, buf, sizeof(buf));
    real_t r; real_from_str(&r, buf);
    *out = val_new_real(&r, FMT_REAL);
    return i;
}

/* CStyleInt: [1-9][0-9]*|0 (with optional exponent) */
static int try_int(const char *s, val_t **out) {
    int int_len = read_digits(s, 10);
    if (int_len == 0) return 0;
    int i = int_len;
    int exp_len = read_exponent(s + i);
    i += exp_len;
    char buf[64]; strip_underscores(s, i, buf, sizeof(buf));
    real_t r; real_from_str(&r, buf);
    *out = val_new_real(&r, FMT_INT);
    return i;
}

/* SiPrefixed: number + SI prefix char
 * prefixes: ryzafpnum_kMGTPEZYR; 値 = number * 10^(index*3) */
static int try_si(const char *s, val_t **out) {
    int i = 0;
    int int_len = read_digits(s, 10);
    i += int_len;
    bool has_frac = false;
    if (s[i] == '.') {
        i++;
        int frac_len = read_digits(s + i, 10);
        if (frac_len == 0) { return 0; }
        i += frac_len;
        has_frac = true;
    }
    if (int_len == 0 && !has_frac) return 0;
    /* SI prefix char: must NOT be followed by 'i' (that would be binary prefix) */
    char pc = s[i];
    int pidx = si_prefix_index(pc);
    if (pidx == INT32_MIN) return 0;
    /* '_' as unity means no suffix needed, but we still consume it */
    if (s[i+1] == 'i') return 0; /* binary prefix takes priority */
    if (isalpha((unsigned char)s[i+1]) || isdigit((unsigned char)s[i+1])) return 0;
    i++; /* consume prefix char */
    /* value = number_str * 10^(pidx*3) */
    char num_buf[64]; strip_underscores(s, i - 1, num_buf, sizeof(num_buf));
    real_t base; real_from_str(&base, num_buf);
    /* multiply by 10^(pidx*3) */
    char exp_str[32]; snprintf(exp_str, sizeof(exp_str), "1E%d", pidx * 3);
    real_t scale; real_from_str(&scale, exp_str);
    real_t result; real_mul(&result, &base, &scale);
    *out = val_new_real(&result, FMT_SI_PREFIX);
    return i;
}

/* BinaryPrefixed: number + [_kMGTPEZYR] + 'i'
 * 値 = number * 2^(index*10) */
static int try_binpfx(const char *s, val_t **out) {
    int i = 0;
    int int_len = read_digits(s, 10);
    i += int_len;
    bool has_frac = false;
    if (s[i] == '.') {
        i++;
        int frac_len = read_digits(s + i, 10);
        if (frac_len == 0) return 0;
        i += frac_len;
        has_frac = true;
    }
    if (int_len == 0 && !has_frac) return 0;
    char pc = s[i];
    int pidx = bin_prefix_index(pc);
    if (pidx < 0) return 0;
    if (s[i+1] != 'i') return 0;
    i += 2; /* consume prefix + 'i' */
    char num_buf[64]; strip_underscores(s, i - 2, num_buf, sizeof(num_buf));
    real_t base; real_from_str(&base, num_buf);
    /* multiply by 2^(pidx*10): pidx は 1..5 なので最大 2^50 — int64_t で十分 */
    real_t scale; real_from_i64(&scale, (int64_t)1 << (pidx * 10));
    real_t result; real_mul(&result, &base, &scale);
    *out = val_new_real(&result, FMT_BIN_PREFIX);
    return i;
}

/* CStyleChar: '.' または '\x' エスケープシーケンス */
static int try_char(const char *s, val_t **out) {
    if (s[0] != '\'') return 0;
    int i = 1;
    int64_t code;
    if (s[i] == '\\') {
        i++;
        switch (s[i]) {
            case 'a':  code = '\a'; i++; break;
            case 'b':  code = '\b'; i++; break;
            case 'f':  code = '\f'; i++; break;
            case 'n':  code = '\n'; i++; break;
            case 'r':  code = '\r'; i++; break;
            case 't':  code = '\t'; i++; break;
            case 'v':  code = '\v'; i++; break;
            case '\\': code = '\\'; i++; break;
            case '\'': code = '\''; i++; break;
            case '0':  code = '\0'; i++; break;
            case 'x': { /* \xNN */
                i++;
                char h[3] = {s[i], s[i+1], 0}; i += 2;
                code = strtoll(h, NULL, 16);
                break;
            }
            case 'u': { /* \uNNNN */
                i++;
                char h[5] = {s[i], s[i+1], s[i+2], s[i+3], 0}; i += 4;
                code = strtoll(h, NULL, 16);
                break;
            }
            case 'o': { /* \oNNN (octal) */
                i++;
                char h[4] = {s[i], s[i+1], s[i+2], 0}; i += 3;
                code = strtoll(h, NULL, 8);
                break;
            }
            default: return 0;
        }
    } else if (s[i] != '\'' && s[i] != '\0') {
        /* UTF-8 デコード: マルチバイト文字を Unicode コードポイントに変換 */
        unsigned char c0 = (unsigned char)s[i];
        if (c0 < 0x80) {
            code = c0; i++;
        } else if ((c0 & 0xE0) == 0xC0) {  /* 2バイト */
            code = (c0 & 0x1F);
            if ((unsigned char)s[i+1] != '\0') { code = (code << 6) | ((unsigned char)s[i+1] & 0x3F); i += 2; }
            else i++;
        } else if ((c0 & 0xF0) == 0xE0) {  /* 3バイト */
            code = (c0 & 0x0F);
            if ((unsigned char)s[i+1] != '\0') { code = (code << 6) | ((unsigned char)s[i+1] & 0x3F); i++; }
            if ((unsigned char)s[i+1] != '\0') { code = (code << 6) | ((unsigned char)s[i+1] & 0x3F); i++; }
            i++;
        } else if ((c0 & 0xF8) == 0xF0) {  /* 4バイト */
            code = (c0 & 0x07);
            for (int b = 0; b < 3; b++) {
                if ((unsigned char)s[i+1] != '\0') { code = (code << 6) | ((unsigned char)s[i+1] & 0x3F); i++; }
            }
            i++;
        } else {
            code = c0; i++;
        }
    } else {
        return 0;
    }
    if (s[i] != '\'') return 0;
    i++;
    *out = val_new_i64(code, FMT_CHAR);
    return i;
}

/* CStyleString: "..." (エスケープ対応) */
static int try_string(const char *s, val_t **out) {
    if (s[0] != '"') return 0;
    int i = 1;
    char buf[TOK_TEXT_MAX];
    int j = 0;
    while (s[i] && s[i] != '"' && j < (int)sizeof(buf) - 1) {
        if (s[i] == '\\') {
            i++;
            switch (s[i]) {
                case 'n':  buf[j++] = '\n'; break;
                case 'r':  buf[j++] = '\r'; break;
                case 't':  buf[j++] = '\t'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"':  buf[j++] = '"';  break;
                case '0':  buf[j++] = '\0'; break;
                default:   buf[j++] = s[i]; break;
            }
        } else {
            buf[j++] = s[i];
        }
        i++;
    }
    if (s[i] != '"') return 0;
    i++;
    buf[j] = '\0';
    *out = val_new_str(buf);
    return i;
}

/* WebColor: #rgb (3桁) または #rrggbb (6桁) */
static int try_webcolor(const char *s, val_t **out) {
    if (s[0] != '#') return 0;
    int dlen = read_digits(s + 1, 16);
    if (dlen != 3 && dlen != 6) return 0;
    int total = 1 + dlen;
    char digits[8]; strip_underscores(s + 1, dlen, digits, sizeof(digits));
    int64_t v = (int64_t)strtoll(digits, NULL, 16);
    if (dlen == 3) {
        /* #rgb → #rrggbb: 各ニブルを複製 */
        int r = (int)((v >> 8) & 0xF); r |= r << 4;
        int g = (int)((v >> 4) & 0xF); g |= g << 4;
        int b = (int)( v       & 0xF); b |= b << 4;
        v = ((int64_t)r << 16) | ((int64_t)g << 8) | b;
    }
    *out = val_new_i64(v, FMT_WEB_COLOR);
    return total;
}

/* DateTime: #yyyy/m/d[ h:m:s]# (移植元: DateTimeFormatter.cs) */
static int try_datetime(const char *s, val_t **out) {
    if (s[0] != '#') return 0;
    /* 閉じ # を探す */
    const char *end = strchr(s + 1, '#');
    if (!end) return 0;
    int inner_len = (int)(end - s - 1);
    if (inner_len < 8 || inner_len > 30) return 0;
    char inner[32];
    memcpy(inner, s + 1, (size_t)inner_len);
    inner[inner_len] = '\0';
    /* 数字と / : . スペースのみ許可 */
    for (int i = 0; i < inner_len; i++) {
        char c = inner[i];
        if (!isdigit((unsigned char)c) && c!='/' && c!=':' && c!='.' && c!=' ') return 0;
    }
    int yr=0, mo=0, dy=0, hr=0, mi=0, sc=0;
    int parsed = sscanf(inner, "%d/%d/%d %d:%d:%d", &yr, &mo, &dy, &hr, &mi, &sc);
    if (parsed < 3) return 0;  /* 日付のみでも可 */
    struct tm t; memset(&t, 0, sizeof(t));
    t.tm_year = yr - 1900;
    t.tm_mon  = mo - 1;
    t.tm_mday = dy;
    t.tm_hour = hr;
    t.tm_min  = mi;
    t.tm_sec  = sc;
    t.tm_isdst = -1;
    time_t ts = mktime(&t);
    if (ts == (time_t)-1) return 0;  /* 不正な日付リテラル: トークン化失敗 */
    real_t r;
    real_from_i64(&r, (int64_t)ts);
    *out = val_new_real(&r, FMT_DATETIME);
    return inner_len + 2;  /* #...# 全体の長さ */
}

/* --- 演算子記号テーブル --- */

static const char *OP_SYMBOLS[] = {
    /* 3文字以上 (長い順) */
    ">>>", "<<<", "..=", "...",
    /* 2文字 */
    ">>", "<<", ">=", "<=", "==", "!=", "&&", "||", "+|", "..", "=>", "//",
    /* 1文字 */
    ">", "<", "=", "+", "-", "*", "/", "%", "^", "&", "|", "!", "~", "$",
    NULL
};

/* 演算子の最長マッチを試みる */
static int try_op(const char *s) {
    int best = 0;
    for (int i = 0; OP_SYMBOLS[i]; i++) {
        int len = (int)strlen(OP_SYMBOLS[i]);
        if (strncmp(s, OP_SYMBOLS[i], (size_t)len) == 0) {
            if (len > best) best = len;
        }
    }
    return best;
}

/* --- レキサー --- */

void lexer_init(lexer_t *lx, const char *src) {
    lx->src      = src;
    lx->pos      = 0;
    lx->len      = (int)strlen(src);
    lx->eos_read = false;
}

bool lexer_eos(lexer_t *lx) {
    /* 空白スキップ */
    while (lx->pos < lx->len &&
           (lx->src[lx->pos] == ' '  || lx->src[lx->pos] == '\t' ||
            lx->src[lx->pos] == '\r' || lx->src[lx->pos] == '\n' ||
            (unsigned char)lx->src[lx->pos] == 0xE3 ||  /* U+3000 IDEOGRAPHIC SPACE UTF-8 先頭 */
            (unsigned char)lx->src[lx->pos] == 0x80 ||
            (unsigned char)lx->src[lx->pos] == 0x80)) {
        /* U+3000 IDEOGRAPHIC SPACE (全角スペース) = E3 80 80 */
        if ((unsigned char)lx->src[lx->pos] == 0xE3 &&
            (unsigned char)lx->src[lx->pos+1] == 0x80 &&
            (unsigned char)lx->src[lx->pos+2] == 0x80) {
            lx->pos += 3;
        } else {
            lx->pos++;
        }
    }
    return lx->pos >= lx->len;
}

void lexer_pop(lexer_t *lx, token_t *t) {
    t->val = NULL;

    /* 空白スキップ */
    if (lexer_eos(lx)) {
        t->type = TOK_EOS;
        t->pos  = lx->pos;
        t->text[0] = '\0';
        lx->eos_read = true;
        return;
    }

    t->pos = lx->pos;
    const char *s = lx->src + lx->pos;

    /* --- 数値リテラル: 全フォーマットを試して最長マッチを採用 --- */
    {
        typedef int (*try_fn)(const char *, val_t **);
        try_fn fns[] = {
            try_hex, try_bin, try_oct, try_real, try_int,
            try_si, try_binpfx, try_char, try_string, try_datetime, try_webcolor,
            NULL
        };
        int best_len = 0;
        val_t *best_val = NULL;
        for (int i = 0; fns[i]; i++) {
            val_t *v = NULL;
            int n = fns[i](s, &v);
            if (n > best_len) {
                if (best_val) val_free(best_val);
                best_len = n;
                best_val = v;
            } else if (v) {
                val_free(v);
            }
        }
        if (best_len > 0) {
            t->type = TOK_NUM_LIT;
            int tlen = best_len < TOK_TEXT_MAX - 1 ? best_len : TOK_TEXT_MAX - 1;
            memcpy(t->text, s, (size_t)tlen);
            t->text[tlen] = '\0';
            t->val = best_val;
            lx->pos += best_len;
            return;
        }
    }

    /* --- 識別子 / キーワード / bool リテラル / ワード --- */
    if (lexer_is_id_start(s[0])) {
        int i = 0;
        while (lexer_is_id_follow(s[i])) i++;
        int tlen = i < TOK_TEXT_MAX - 1 ? i : TOK_TEXT_MAX - 1;
        memcpy(t->text, s, (size_t)tlen);
        t->text[tlen] = '\0';
        lx->pos += i;
        if (strcmp(t->text, "def") == 0) {
            t->type = TOK_KEYWORD;
        } else if (strcmp(t->text, "true") == 0 || strcmp(t->text, "false") == 0) {
            t->type = TOK_BOOL_LIT;
        } else {
            t->type = TOK_WORD;
        }
        return;
    }

    /* --- 演算子記号 --- */
    {
        int n = try_op(s);
        if (n > 0) {
            t->type = TOK_OP;
            int tlen = n < TOK_TEXT_MAX - 1 ? n : TOK_TEXT_MAX - 1;
            memcpy(t->text, s, (size_t)tlen);
            t->text[tlen] = '\0';
            lx->pos += n;
            return;
        }
    }

    /* --- 一般記号 --- */
    if (strchr("()[],:?", s[0])) {
        t->type    = TOK_SYMBOL;
        t->text[0] = s[0];
        t->text[1] = '\0';
        lx->pos++;
        return;
    }

    /* --- 未知のトークン (エラー扱い: 1文字読み捨て) --- */
    t->type    = TOK_EMPTY;
    t->text[0] = s[0];
    t->text[1] = '\0';
    lx->pos++;
}

/* --- トークンキュー --- */

void tok_queue_init(tok_queue_t *q) {
    q->head  = 0;
    q->count = 0;
}

void tok_queue_free(tok_queue_t *q) {
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % TOK_QUEUE_MAX;
        tok_free(&q->tokens[idx]);
    }
    q->head  = 0;
    q->count = 0;
}

void tok_queue_push_back(tok_queue_t *q, const token_t *t) {
    if (q->count >= TOK_QUEUE_MAX) return;  /* キューフル: トークンを捨てる */
    int idx = (q->head + q->count) % TOK_QUEUE_MAX;
    q->tokens[idx] = *t;  /* shallow copy; val ownership transferred */
    q->count++;
}

const token_t *tok_queue_peek(const tok_queue_t *q) {
    static token_t eos_tok = { TOK_EOS, 0, "[EOS]", NULL };
    if (q->count == 0) return &eos_tok;
    return &q->tokens[q->head % TOK_QUEUE_MAX];
}

token_t tok_queue_pop(tok_queue_t *q) {
    static token_t eos_tok = { TOK_EOS, 0, "[EOS]", NULL };
    if (q->count == 0) return eos_tok;
    token_t t = q->tokens[q->head % TOK_QUEUE_MAX];
    q->head = (q->head + 1) % TOK_QUEUE_MAX;
    q->count--;
    return t;
}

/* --- 全体トークン化 --- */

void lexer_tokenize(const char *src, tok_queue_t *q) {
    tok_queue_init(q);
    lexer_t lx;
    lexer_init(&lx, src);
    for (;;) {
        token_t t;
        lexer_pop(&lx, &t);
        tok_queue_push_back(q, &t);
        if (t.type == TOK_EOS) break;
    }
}

/* --- ユーティリティ --- */

bool lexer_is_id_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

bool lexer_is_id_follow(char c) {
    return isalnum((unsigned char)c) || c == '_';
}
