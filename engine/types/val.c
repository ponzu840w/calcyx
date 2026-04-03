/* 移植元: Calctus/Model/Types/Val.cs, RealVal.cs, FracVal.cs,
 *          BoolVal.cs, StrVal.cs, NullVal.cs, ArrayVal.cs */

#include "val.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ======================================================
 * フォーマット選択 (FormatHint.Select)
 * ====================================================== */

typedef enum { PRI_WEAK = 0, PRI_STRONG = 1, PRI_ALWAYS_LEFT = 2 } fmt_pri_t;

static fmt_pri_t fmt_priority(val_fmt_t f) {
    switch (f) {
        case FMT_REAL:       return PRI_WEAK;
        case FMT_INT:        return PRI_WEAK;
        case FMT_SI_PREFIX:  return PRI_STRONG;
        case FMT_BIN_PREFIX: return PRI_STRONG;
        default:             return PRI_ALWAYS_LEFT;
    }
}

val_fmt_t val_fmt_select(val_fmt_t a, val_fmt_t b) {
    if (fmt_priority(a) == PRI_WEAK && fmt_priority(b) == PRI_STRONG)
        return b;
    return a;
}

/* ======================================================
 * 内部ヘルパー
 * ====================================================== */

static val_t *alloc_val(val_type_t type, val_fmt_t fmt) {
    val_t *v = (val_t *)calloc(1, sizeof(val_t));
    if (!v) return NULL;
    v->type = type;
    v->fmt  = fmt;
    return v;
}

/* ======================================================
 * 生成
 * ====================================================== */

val_t *val_new_real(const real_t *r, val_fmt_t fmt) {
    val_t *v = alloc_val(VAL_REAL, fmt);
    if (!v) return NULL;
    real_copy(&v->real_v, r);
    return v;
}

val_t *val_new_frac(const frac_t *f) {
    val_t *v = alloc_val(VAL_FRAC, FMT_REAL);
    if (!v) return NULL;
    frac_copy(&v->frac_v, f);
    return v;
}

val_t *val_new_bool(bool b) {
    val_t *v = alloc_val(VAL_BOOL, FMT_REAL);
    if (!v) return NULL;
    v->bool_v = b;
    return v;
}

val_t *val_new_str(const char *s) {
    val_t *v = alloc_val(VAL_STR, FMT_STRING);
    if (!v) return NULL;
    v->str_v = (char *)malloc(strlen(s) + 1);
    if (!v->str_v) { free(v); return NULL; }
    strcpy(v->str_v, s);
    return v;
}

val_t *val_new_null(void) {
    return alloc_val(VAL_NULL, FMT_REAL);
}

val_t *val_new_i64(int64_t n, val_fmt_t fmt) {
    real_t r;
    real_from_i64(&r, n);
    return val_new_real(&r, fmt);
}

val_t *val_new_double(double d, val_fmt_t fmt) {
    real_t r;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", d);
    real_from_str(&r, buf);
    return val_new_real(&r, fmt);
}

val_t *val_new_array(val_t **items, int len, val_fmt_t fmt) {
    val_t *v = alloc_val(VAL_ARRAY, fmt);
    if (!v) return NULL;
    if (len > 0) {
        v->arr_items = (val_t **)malloc((size_t)len * sizeof(val_t *));
        if (!v->arr_items) { free(v); return NULL; }
        for (int i = 0; i < len; i++)
            v->arr_items[i] = val_dup(items[i]);
    }
    v->arr_len = len;
    return v;
}

/* ======================================================
 * func_def_t (関数定義)
 * ====================================================== */

val_t *val_new_func(func_def_t *fd) {
    val_t *v = alloc_val(VAL_FUNC, FMT_REAL);
    if (!v) return NULL;
    v->func_v = fd;
    return v;
}

void func_def_free(func_def_t *f) {
    if (!f) return;
    if (f->param_names) {
        for (int i = 0; i < f->n_params; i++) free(f->param_names[i]);
        free(f->param_names);
        f->param_names = NULL;
    }
    if (f->free_body && f->body) {
        f->free_body(f->body);
        f->body = NULL;
    }
}

func_def_t *func_def_dup(const func_def_t *src) {
    if (!src) return NULL;
    func_def_t *d = (func_def_t *)malloc(sizeof(func_def_t));
    if (!d) return NULL;
    *d = *src;  /* 関数ポインタを含む浅いコピー */
    /* param_names のディープコピー */
    if (src->n_params > 0 && src->param_names) {
        d->param_names = (char **)calloc((size_t)src->n_params, sizeof(char *));
        if (!d->param_names) { free(d); return NULL; }
        for (int i = 0; i < src->n_params; i++) {
            d->param_names[i] = src->param_names[i] ? strdup(src->param_names[i]) : NULL;
        }
    } else {
        d->param_names = NULL;
    }
    /* body のディープコピー */
    if (src->dup_body && src->body) {
        d->body = src->dup_body(src->body);
    } else {
        d->body = NULL;
    }
    return d;
}

/* ======================================================
 * コピー / 解放
 * ====================================================== */

val_t *val_dup(const val_t *src) {
    if (!src) return NULL;
    switch (src->type) {
        case VAL_REAL:  return val_new_real(&src->real_v, src->fmt);
        case VAL_FRAC:  return val_new_frac(&src->frac_v);
        case VAL_BOOL:  { val_t *v = val_new_bool(src->bool_v); if (v) v->fmt = src->fmt; return v; }
        case VAL_STR:   return val_new_str(src->str_v);
        case VAL_NULL:  return val_new_null();
        case VAL_ARRAY: {
            val_t *v = alloc_val(VAL_ARRAY, src->fmt);
            if (!v) return NULL;
            if (src->arr_len > 0) {
                v->arr_items = (val_t **)malloc((size_t)src->arr_len * sizeof(val_t *));
                if (!v->arr_items) { free(v); return NULL; }
                for (int i = 0; i < src->arr_len; i++)
                    v->arr_items[i] = val_dup(src->arr_items[i]);
            }
            v->arr_len = src->arr_len;
            return v;
        }
        case VAL_FUNC: {
            func_def_t *fd = func_def_dup(src->func_v);
            if (!fd) return NULL;
            return val_new_func(fd);
        }
    }
    return NULL;
}

void val_free(val_t *v) {
    if (!v) return;
    switch (v->type) {
        case VAL_STR:
            free(v->str_v);
            break;
        case VAL_ARRAY:
            for (int i = 0; i < v->arr_len; i++)
                val_free(v->arr_items[i]);
            free(v->arr_items);
            break;
        case VAL_FUNC:
            if (v->func_v) {
                func_def_free(v->func_v);
                free(v->func_v);
            }
            break;
        default:
            break;
    }
    free(v);
}

/* ======================================================
 * フォーマット変換
 * ====================================================== */

val_t *val_reformat(const val_t *v, val_fmt_t fmt) {
    val_t *r = val_dup(v);
    if (r) r->fmt = fmt;
    return r;
}

/* ======================================================
 * 型変換
 * ====================================================== */

void val_as_real(real_t *out, const val_t *v) {
    if (v->type == VAL_REAL) {
        real_copy(out, &v->real_v);
    } else if (v->type == VAL_FRAC) {
        frac_to_real(out, &v->frac_v);
    } else {
        real_from_i64(out, 0);  /* fallback */
    }
}

void val_as_frac(frac_t *out, const val_t *v) {
    if (v->type == VAL_FRAC) {
        frac_copy(out, &v->frac_v);
    } else {
        real_t r;
        val_as_real(&r, v);
        frac_from_real(out, &r);
    }
}

int64_t val_as_long(const val_t *v) {
    real_t r;
    val_as_real(&r, v);
    return real_to_i64(&r);
}

int val_as_int(const val_t *v) {
    return (int)val_as_long(v);
}

double val_as_double(const val_t *v) {
    real_t r;
    val_as_real(&r, v);
    return real_to_double(&r);
}

bool val_as_bool(const val_t *v) {
    if (v->type == VAL_BOOL) return v->bool_v;
    if (v->type == VAL_REAL) {
        /* 非ゼロなら真 */
        real_t zero; real_from_i64(&zero, 0);
        return !real_eq(&v->real_v, &zero);
    }
    if (v->type == VAL_NULL) return false;
    return false;
}

/* UpConvert: 精度を右辺に合わせる (RealVal.OnUpConvert) */
val_t *val_upconvert(const val_t *a, const val_t *b) {
    if (a->type == VAL_REAL && b->type == VAL_REAL) return val_dup(a);
    if (a->type == VAL_REAL && b->type == VAL_FRAC) {
        /* REAL → FRAC: new frac(a_raw, 1) */
        real_t one;
        real_from_i64(&one, 1);
        frac_t f;
        frac_from_n_d(&f, &a->real_v, &one);
        return val_new_frac(&f);
    }
    if (a->type == VAL_FRAC && b->type == VAL_FRAC) return val_dup(a);
    if (a->type == VAL_FRAC && b->type == VAL_REAL) return val_dup(a);
    /* STR は STR のまま */
    if (a->type == VAL_STR || b->type == VAL_STR) {
        char buf[256]; val_to_str(a, buf, sizeof(buf));
        return val_new_str(buf);
    }
    return val_dup(a);
}

/* ======================================================
 * 単項演算
 * ====================================================== */

val_t *val_neg(const val_t *a) {
    if (a->type == VAL_REAL) {
        real_t r; real_neg(&r, &a->real_v);
        return val_new_real(&r, a->fmt);
    }
    if (a->type == VAL_FRAC) {
        frac_t f; frac_neg(&f, &a->frac_v);
        val_t *v = val_new_frac(&f); if (v) v->fmt = a->fmt;
        return v;
    }
    return NULL;
}

val_t *val_bit_not(const val_t *a) {
    int64_t n = ~val_as_long(a);
    return val_new_i64(n, a->fmt);
}

/* ======================================================
 * 2項演算の共通パターン
 * ======================================================
 * 1. UpConvert a を b に合わせる
 * 2. 演算実行
 * 3. フォーマット選択
 * ====================================================== */

/* REAL 同士の算術演算ヘルパー */
#define REAL_BINOP(name, op_fn)                                 \
    val_t *name(const val_t *a, const val_t *b) {              \
        val_t *ua = val_upconvert(a, b);                        \
        if (!ua) return NULL;                                   \
        if (ua->type == VAL_REAL) {                             \
            real_t ra, rb, rc;                                  \
            val_as_real(&ra, ua); val_as_real(&rb, b);          \
            op_fn(&rc, &ra, &rb);                               \
            val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);     \
            val_free(ua);                                       \
            return val_new_real(&rc, fmt);                      \
        }                                                       \
        if (ua->type == VAL_FRAC) {                             \
            frac_t fa, fb, fc;                                  \
            val_as_frac(&fa, ua); val_as_frac(&fb, b);          \
            frac_##op_fn(&fc, &fa, &fb);                        \
            val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);     \
            val_free(ua);                                       \
            val_t *v = val_new_frac(&fc);                       \
            if (v) v->fmt = fmt;                                \
            return v;                                           \
        }                                                       \
        val_free(ua); return NULL;                              \
    }

/* マクロでは frac_real_add が存在しないので手動実装 */

val_t *val_add(const val_t *a, const val_t *b) {
    /* 文字列連結 */
    if (a->type == VAL_STR || b->type == VAL_STR) {
        char sa[256], sb[256];
        val_to_str(a, sa, sizeof(sa));
        val_to_str(b, sb, sizeof(sb));
        char sc[512];
        snprintf(sc, sizeof(sc), "%s%s", sa, sb);
        return val_new_str(sc);
    }
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    if (ua->type == VAL_REAL) {
        real_t ra, rb, rc;
        real_copy(&ra, &ua->real_v); val_as_real(&rb, b);
        real_add(&rc, &ra, &rb);
        val_free(ua);
        return val_new_real(&rc, fmt);
    }
    if (ua->type == VAL_FRAC) {
        frac_t fa, fb, fc;
        frac_copy(&fa, &ua->frac_v); val_as_frac(&fb, b);
        frac_add(&fc, &fa, &fb);
        val_free(ua);
        val_t *v = val_new_frac(&fc); if (v) v->fmt = fmt;
        return v;
    }
    val_free(ua); return NULL;
}

val_t *val_sub(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    if (ua->type == VAL_REAL) {
        real_t ra, rb, rc;
        real_copy(&ra, &ua->real_v); val_as_real(&rb, b);
        real_sub(&rc, &ra, &rb);
        val_free(ua);
        return val_new_real(&rc, fmt);
    }
    if (ua->type == VAL_FRAC) {
        frac_t fa, fb, fc;
        frac_copy(&fa, &ua->frac_v); val_as_frac(&fb, b);
        frac_sub(&fc, &fa, &fb);
        val_free(ua);
        val_t *v = val_new_frac(&fc); if (v) v->fmt = fmt;
        return v;
    }
    val_free(ua); return NULL;
}

val_t *val_mul(const val_t *a, const val_t *b) {
    /* 文字列繰り返し: "abc" * 3 = "abcabcabc" */
    if (a->type == VAL_STR && b->type == VAL_REAL) {
        int n = (int)real_to_i64(&b->real_v);
        if (n <= 0) return val_new_str("");
        size_t slen = strlen(a->str_v);
        char *buf = (char *)malloc(slen * (size_t)n + 1);
        if (!buf) return NULL;
        buf[0] = '\0';
        for (int i = 0; i < n; i++) strcat(buf, a->str_v);
        val_t *r = val_new_str(buf);
        free(buf);
        return r;
    }
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    if (ua->type == VAL_REAL) {
        real_t ra, rb, rc;
        real_copy(&ra, &ua->real_v); val_as_real(&rb, b);
        real_mul(&rc, &ra, &rb);
        val_free(ua);
        return val_new_real(&rc, fmt);
    }
    if (ua->type == VAL_FRAC) {
        frac_t fa, fb, fc;
        frac_copy(&fa, &ua->frac_v); val_as_frac(&fb, b);
        frac_mul(&fc, &fa, &fb);
        val_free(ua);
        val_t *v = val_new_frac(&fc); if (v) v->fmt = fmt;
        return v;
    }
    val_free(ua); return NULL;
}

val_t *val_div(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    if (ua->type == VAL_REAL) {
        real_t ra, rb, rc;
        real_copy(&ra, &ua->real_v); val_as_real(&rb, b);
        real_div(&rc, &ra, &rb);
        val_free(ua);
        return val_new_real(&rc, fmt);
    }
    if (ua->type == VAL_FRAC) {
        frac_t fa, fb, fc;
        frac_copy(&fa, &ua->frac_v); val_as_frac(&fb, b);
        frac_div(&fc, &fa, &fb);
        val_free(ua);
        val_t *v = val_new_frac(&fc); if (v) v->fmt = fmt;
        return v;
    }
    val_free(ua); return NULL;
}

/* 整数除算: Truncate(a/b) */
val_t *val_idiv(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    real_t ra, rb, rc, rt;
    val_as_real(&ra, ua); val_as_real(&rb, b);
    real_div(&rt, &ra, &rb);
    /* Truncate: floor(|x|) * sign(x) */
    real_t zero; real_from_i64(&zero, 0);
    if (real_cmp(&rt, &zero) >= 0) {
        real_floor(&rc, &rt);
    } else {
        real_t abs_rt, fl, neg;
        real_abs(&abs_rt, &rt);
        real_floor(&fl, &abs_rt);
        real_neg(&rc, &fl);
    }
    val_free(ua);
    return val_new_real(&rc, fmt);
}

val_t *val_mod(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return NULL;
    val_fmt_t fmt = val_fmt_select(a->fmt, b->fmt);
    real_t ra, rb, rc;
    val_as_real(&ra, ua); val_as_real(&rb, b);
    real_rem(&rc, &ra, &rb);
    val_free(ua);
    return val_new_real(&rc, fmt);
}

/* ======================================================
 * ビット演算
 * ====================================================== */

val_t *val_bit_and(const val_t *a, const val_t *b) {
    return val_new_i64(val_as_long(a) & val_as_long(b), val_fmt_select(a->fmt, b->fmt));
}

val_t *val_bit_xor(const val_t *a, const val_t *b) {
    return val_new_i64(val_as_long(a) ^ val_as_long(b), val_fmt_select(a->fmt, b->fmt));
}

val_t *val_bit_or(const val_t *a, const val_t *b) {
    return val_new_i64(val_as_long(a) | val_as_long(b), val_fmt_select(a->fmt, b->fmt));
}

/* ======================================================
 * シフト演算
 * ====================================================== */

val_t *val_lsl(const val_t *a, const val_t *b) {
    return val_new_i64(val_as_long(a) << val_as_int(b), a->fmt);
}

val_t *val_lsr(const val_t *a, const val_t *b) {
    return val_new_i64((int64_t)((uint64_t)val_as_long(a) >> val_as_int(b)), a->fmt);
}

val_t *val_asl(const val_t *a, const val_t *b) {
    int64_t av = val_as_long(a);
    int sh = val_as_int(b);
    int64_t sign = av & (int64_t)(1ULL << 63);
    int64_t shifted = (av << sh) & (int64_t)0x7fffffffffffffffLL;
    return val_new_i64(sign | shifted, a->fmt);
}

val_t *val_asr(const val_t *a, const val_t *b) {
    return val_new_i64(val_as_long(a) >> val_as_int(b), a->fmt);
}

/* ======================================================
 * 比較演算
 * ====================================================== */

val_t *val_eq(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return val_new_bool(false);
    bool result = false;
    if (ua->type == VAL_REAL) {
        real_t ra, rb; val_as_real(&ra, ua); val_as_real(&rb, b);
        result = real_eq(&ra, &rb);
    } else if (ua->type == VAL_FRAC) {
        frac_t fa, fb; val_as_frac(&fa, ua); val_as_frac(&fb, b);
        result = frac_eq(&fa, &fb);
    } else if (ua->type == VAL_BOOL) {
        result = (ua->bool_v == val_as_bool(b));
    } else if (ua->type == VAL_STR) {
        result = (strcmp(ua->str_v, b->str_v) == 0);
    }
    val_free(ua);
    return val_new_bool(result);
}

val_t *val_ne(const val_t *a, const val_t *b) {
    val_t *e = val_eq(a, b);
    val_t *r = val_new_bool(!e->bool_v);
    val_free(e);
    return r;
}

val_t *val_gt(const val_t *a, const val_t *b) {
    val_t *ua = val_upconvert(a, b);
    if (!ua) return val_new_bool(false);
    bool result = false;
    if (ua->type == VAL_REAL) {
        real_t ra, rb; val_as_real(&ra, ua); val_as_real(&rb, b);
        result = (real_cmp(&ra, &rb) > 0);
    } else if (ua->type == VAL_FRAC) {
        frac_t fa, fb; val_as_frac(&fa, ua); val_as_frac(&fb, b);
        result = frac_gt(&fa, &fb);
    }
    val_free(ua);
    return val_new_bool(result);
}

val_t *val_lt(const val_t *a, const val_t *b) { return val_gt(b, a); }

val_t *val_ge(const val_t *a, const val_t *b) {
    val_t *g = val_gt(a, b), *e = val_eq(a, b);
    val_t *r = val_logic_or(g, e);
    val_free(g); val_free(e);
    return r;
}

val_t *val_le(const val_t *a, const val_t *b) { return val_ge(b, a); }

/* ======================================================
 * 論理演算
 * ====================================================== */

val_t *val_logic_and(const val_t *a, const val_t *b) {
    return val_new_bool(val_as_bool(a) && val_as_bool(b));
}

val_t *val_logic_or(const val_t *a, const val_t *b) {
    return val_new_bool(val_as_bool(a) || val_as_bool(b));
}

val_t *val_logic_not(const val_t *a) {
    return val_new_bool(!val_as_bool(a));
}

/* ======================================================
 * 文字列変換
 * ====================================================== */

void val_to_str(const val_t *v, char *buf, size_t buflen) {
    if (!v || buflen == 0) return;
    switch (v->type) {
        case VAL_REAL: {
            real_to_str(&v->real_v, buf, buflen);
            break;
        }
        case VAL_FRAC: {
            char sn[64], sd[64];
            real_to_str(&v->frac_v.nume, sn, sizeof(sn));
            real_to_str(&v->frac_v.deno, sd, sizeof(sd));
            snprintf(buf, buflen, "%s$%s", sn, sd);
            break;
        }
        case VAL_BOOL:
            snprintf(buf, buflen, "%s", v->bool_v ? "true" : "false");
            break;
        case VAL_STR:
            snprintf(buf, buflen, "%s", v->str_v);
            break;
        case VAL_NULL:
            snprintf(buf, buflen, "null");
            break;
        case VAL_ARRAY: {
            size_t pos = 0;
            if (pos < buflen - 1) buf[pos++] = '[';
            for (int i = 0; i < v->arr_len; i++) {
                if (i > 0 && pos + 2 < buflen) { buf[pos++] = ','; buf[pos++] = ' '; }
                char elem[128];
                val_to_str(v->arr_items[i], elem, sizeof(elem));
                size_t elen = strlen(elem);
                if (pos + elen < buflen - 2) { memcpy(buf + pos, elem, elen); pos += elen; }
            }
            if (pos < buflen - 1) buf[pos++] = ']';
            buf[pos] = '\0';
            break;
        }
        case VAL_FUNC:
            snprintf(buf, buflen, "<func:%s>",
                     (v->func_v && v->func_v->name[0]) ? v->func_v->name : "?");
            break;
    }
}
