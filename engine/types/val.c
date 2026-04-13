/* 移植元: Calctus/Model/Types/Val.cs, RealVal.cs, FracVal.cs,
 *          BoolVal.cs, StrVal.cs, NullVal.cs, ArrayVal.cs */

#include "val.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>

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
    if (!a || !b) return NULL;
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
        if (!real_is_integer(&b->real_v)) return NULL;  /* 非整数は型エラー */
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
    int sh = val_as_int(b);
    if (sh < 0 || sh >= 64) return val_new_i64(0, a->fmt);
    return val_new_i64((int64_t)((uint64_t)val_as_long(a) << sh), a->fmt);
}

val_t *val_lsr(const val_t *a, const val_t *b) {
    int sh = val_as_int(b);
    if (sh < 0 || sh >= 64) return val_new_i64(0, a->fmt);
    return val_new_i64((int64_t)((uint64_t)val_as_long(a) >> sh), a->fmt);
}

val_t *val_asl(const val_t *a, const val_t *b) {
    /* Arithmetic Shift Left: MSB (符号ビット) を保持したまま下位 63 ビットを左シフト。
     * Calctus の ASL セマンティクスに従い、符号ビットは入力値のまま固定する。 */
    int64_t av = val_as_long(a);
    int sh = val_as_int(b);
    if (sh < 0 || sh >= 64) return val_new_i64(0, a->fmt);
    int64_t sign    = av & (int64_t)(1ULL << 63);              /* MSB を保存 */
    int64_t shifted = (int64_t)((uint64_t)av << sh)            /* 左シフト   */
                      & (int64_t)0x7fffffffffffffffLL;          /* MSB をクリア */
    return val_new_i64(sign | shifted, a->fmt);                /* MSB を戻す */
}

val_t *val_asr(const val_t *a, const val_t *b) {
    int64_t av = val_as_long(a);
    int sh = val_as_int(b);
    if (sh < 0) return val_new_i64(0, a->fmt);
    if (sh >= 63) return val_new_i64(av < 0 ? -1 : 0, a->fmt);
    return val_new_i64(av >> sh, a->fmt);
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
        if (b->type == VAL_STR) {
            result = (strcmp(ua->str_v, b->str_v) == 0);
        }
        /* 型が一致しない場合は false のまま */
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
    /* 文字列比較 */
    if (a->type == VAL_STR && b->type == VAL_STR)
        return val_new_bool(strcmp(a->str_v, b->str_v) > 0);
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

/* 実数値を fmt に従ってフォーマット */
static void real_to_str_fmt(const real_t *r, val_fmt_t fmt, char *buf, size_t buflen) {
    int64_t iv;
    switch (fmt) {
        case FMT_HEX:
            if (real_is_integer(r)) {
                iv = real_to_i64(r);
                if (iv < 0)       snprintf(buf, buflen, "-0x%llX", (unsigned long long)(-iv));
                else              snprintf(buf, buflen, "0x%llX",  (unsigned long long)iv);
            } else { real_to_str(r, buf, buflen); }
            break;
        case FMT_BIN:
            if (real_is_integer(r)) {
                iv = real_to_i64(r);
                if (iv == 0) { snprintf(buf, buflen, "0b0"); break; }
                char tmp[70]; int pos = 0;
                uint64_t uv = (iv < 0) ? (uint64_t)(-iv) : (uint64_t)iv;
                while (uv) { tmp[pos++] = '0' + (int)(uv & 1); uv >>= 1; }
                size_t p = 0;
                if (iv < 0 && p < buflen-1) buf[p++] = '-';
                if (p + 2 < buflen) { buf[p++] = '0'; buf[p++] = 'b'; }
                for (int i = pos-1; i >= 0 && p < buflen-1; i--) buf[p++] = tmp[i];
                buf[p] = '\0';
            } else { real_to_str(r, buf, buflen); }
            break;
        case FMT_OCT:
            if (real_is_integer(r)) {
                iv = real_to_i64(r);
                if (iv < 0) snprintf(buf, buflen, "-0%llo", (unsigned long long)(-iv));
                else        snprintf(buf, buflen, "0%llo",  (unsigned long long)iv);
            } else { real_to_str(r, buf, buflen); }
            break;
        case FMT_CHAR:
            if (real_is_integer(r)) {
                iv = real_to_i64(r);
                /* UTF-8 エンコード */
                size_t p = 0;
                if      (iv < 0x80)   { buf[p++] = (char)iv; }
                else if (iv < 0x800)  { buf[p++] = (char)(0xC0|(iv>>6)); buf[p++] = (char)(0x80|(iv&0x3F)); }
                else if (iv < 0x10000){ buf[p++] = (char)(0xE0|(iv>>12)); buf[p++] = (char)(0x80|((iv>>6)&0x3F)); buf[p++] = (char)(0x80|(iv&0x3F)); }
                else                  { buf[p++] = (char)(0xF0|(iv>>18)); buf[p++] = (char)(0x80|((iv>>12)&0x3F)); buf[p++] = (char)(0x80|((iv>>6)&0x3F)); buf[p++] = (char)(0x80|(iv&0x3F)); }
                buf[p] = '\0';
            } else { real_to_str(r, buf, buflen); }
            break;
        case FMT_SI_PREFIX: {
            double d = real_to_double(r);
            static const char *si_pfx[] = { "R","Y","Z","E","P","T","G","M","k","","m","u","n","p","f","a","z","y","r" };
            static const int   si_exp[] = { 27,24,21,18,15,12, 9, 6, 3, 0,-3,-6,-9,-12,-15,-18,-21,-24,-27 };
            double ad = d < 0 ? -d : d;
            for (int i = 0; i < 19; i++) {
                double base = 1.0; for (int j=0; j<(si_exp[i]<0?-si_exp[i]:si_exp[i]); j++) { if(si_exp[i]>0) base*=10; else base/=10; }
                if (ad >= base * 0.999999 || i == 9) {
                    double v = d / base;
                    if (v == (int64_t)v) snprintf(buf, buflen, "%lld%s", (long long)(int64_t)v, si_pfx[i]);
                    else                snprintf(buf, buflen, "%.6g%s", v, si_pfx[i]);
                    /* 末尾の余分な0を除去 */
                    break;
                }
            }
            if (!buf[0]) real_to_str(r, buf, buflen);
            break;
        }
        case FMT_BIN_PREFIX: {
            double d = real_to_double(r);
            static const char *bi_pfx[] = { "Yi","Zi","Ei","Pi","Ti","Gi","Mi","Ki","" };
            static const int   bi_shft[] = {  80,  70,  60,  50,  40,  30,  20,  10,  0 };
            double ad = d < 0 ? -d : d;
            for (int i = 0; i < 9; i++) {
                double base = 1.0; for (int j=0; j<bi_shft[i]; j++) base*=2;
                if (ad >= base * 0.999999 || i == 8) {
                    double v = d / base;
                    if (v == (int64_t)v) snprintf(buf, buflen, "%lld%s", (long long)(int64_t)v, bi_pfx[i]);
                    else                snprintf(buf, buflen, "%.6g%s", v, bi_pfx[i]);
                    break;
                }
            }
            if (!buf[0]) real_to_str(r, buf, buflen);
            break;
        }
        case FMT_DATETIME: {
            time_t ts = (time_t)real_to_i64(r);
            struct tm *t = localtime(&ts);
            snprintf(buf, buflen, "#%04d/%02d/%02d %02d:%02d:%02d#",
                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                     t->tm_hour, t->tm_min, t->tm_sec);
            break;
        }
        case FMT_WEB_COLOR:
            if (real_is_integer(r)) {
                iv = real_to_i64(r);
                if (iv < 0 || iv > 0xFFFFFF) {
                    /* RGB 範囲外は 16 進数 */
                    if (iv < 0) snprintf(buf, buflen, "-0x%llX", (unsigned long long)(-iv));
                    else        snprintf(buf, buflen,  "0x%llX", (unsigned long long)iv);
                } else {
                    snprintf(buf, buflen, "#%06X", (unsigned int)iv);
                }
            } else { real_to_str(r, buf, buflen); }
            break;
        default:
            real_to_str(r, buf, buflen);
            break;
    }
}

void val_to_str(const val_t *v, char *buf, size_t buflen) {
    if (!v || buflen == 0) return;
    if (buflen > 0) buf[0] = '\0';
    switch (v->type) {
        case VAL_REAL: {
            real_to_str_fmt(&v->real_v, v->fmt, buf, buflen);
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
            bool truncated = false;
            for (int i = 0; i < v->arr_len; i++) {
                char elem[128];
                val_to_str(v->arr_items[i], elem, sizeof(elem));
                size_t elen = strlen(elem);
                /* セパレータ + 要素 + ']' が収まるか確認 (余裕: "..." + ']' = 4) */
                size_t need = (i > 0 ? 2 : 0) + elen + 4;
                if (pos + need >= buflen) { truncated = true; break; }
                if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }
                memcpy(buf + pos, elem, elen); pos += elen;
            }
            if (truncated && pos + 3 < buflen) {
                memcpy(buf + pos, "...", 3); pos += 3;
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

/* ======================================================
 * NumberFormatter
 * 移植元: Calctus/Model/Formats/NumberFormatter.cs - RealToString()
 * ====================================================== */

/* C# decimal.ToString("0.###...#") 相当。
 * decimal_len 桁で HALF_UP 丸め (C# MidpointRounding.AwayFromZero) した後、
 * 末尾ゼロを除去する。 */
static void format_plain(const real_t *r, int decimal_len, char *buf, size_t buflen) {
    mpd_context_t fmt_ctx = real_ctx;
    fmt_ctx.round = MPD_ROUND_HALF_UP;

    char fmt_str[32];
    snprintf(fmt_str, sizeof(fmt_str), ".%df", decimal_len < 0 ? 0 : decimal_len);

    char *s = mpd_format((mpd_t *)&r->mpd, fmt_str, &fmt_ctx);
    if (!s) { real_to_str(r, buf, buflen); return; }

    /* 小数部の末尾ゼロを除去し、末尾の '.' も除去 */
    char *dot = strchr(s, '.');
    if (dot) {
        int i = (int)strlen(s) - 1;
        while (i > (int)(dot - s) && s[i] == '0') i--;
        if (s[i] == '.') i--;
        s[i + 1] = '\0';
    }

    strncpy(buf, s, buflen - 1);
    buf[buflen - 1] = '\0';
    mpd_free(s);
}

/* 10^n (n >= 0) を out に書く */
static void real_pow10_pos(real_t *out, int n) {
    real_t base;
    real_from_i64(&base, 10);
    real_pown(out, &base, (int64_t)n);
}

/* 移植元: NumberFormatter.cs - NumberFormatter.RealToString() */
void real_to_str_with_settings(const real_t *r, const fmt_settings_t *fs,
                                char *buf, size_t buflen) {
    if (real_is_zero(r)) { snprintf(buf, buflen, "0"); return; }

    int exp = (int)mpd_adjexp((mpd_t *)&r->mpd);

    char frac_buf[512];

    if (fs->e_notation && exp >= fs->e_positive_min) {
        int eexp = exp;
        if (fs->e_alignment) eexp = (int)floor(eexp / 3.0) * 3;
        real_t pow_ten, frac;
        real_pow10_pos(&pow_ten, eexp);
        real_div(&frac, r, &pow_ten);
        format_plain(&frac, fs->decimal_len, frac_buf, sizeof(frac_buf));
        snprintf(buf, buflen, "%se%d", frac_buf, eexp);
    } else if (fs->e_notation && exp <= fs->e_negative_max) {
        int eexp = exp;
        if (fs->e_alignment) eexp = (int)floor(eexp / 3.0) * 3;
        real_t pow_ten, frac;
        real_pow10_pos(&pow_ten, -eexp);
        real_mul(&frac, r, &pow_ten);
        format_plain(&frac, fs->decimal_len, frac_buf, sizeof(frac_buf));
        snprintf(buf, buflen, "%se%d", frac_buf, eexp);
    } else {
        format_plain(r, fs->decimal_len, buf, buflen);
    }
}
