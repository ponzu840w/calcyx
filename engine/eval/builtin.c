/* 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイル
 * (ExponentialFuncs, TrigonometricFuncs, RoundingFuncs,
 *  Absolute_SignFuncs, Min_MaxFuncs, Gcd_LcmFuncs, AssertionFuncs) */

#include "builtin.h"
#include "eval_ctx.h"
#include "../types/real.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

/* --- ユーティリティ --- */

/* real_t ↔ double 変換ヘルパー */
static double v_to_d(val_t *v) { return val_as_double(v); }
static val_t *d_to_v(double d, val_t *hint) {
    return val_new_double(d, hint ? hint->fmt : FMT_REAL);
}

/* 1引数 double関数ラッパー生成マクロ */
#define UNARY_DOUBLE_FN(cname, mathfn) \
static val_t *cname(val_t **a, int n, void *ctx) { \
    (void)ctx; (void)n; \
    return d_to_v(mathfn(v_to_d(a[0])), a[0]); \
}

/* 2引数 double関数ラッパー生成マクロ */
#define BINARY_DOUBLE_FN(cname, mathfn) \
static val_t *cname(val_t **a, int n, void *ctx) { \
    (void)ctx; (void)n; \
    return d_to_v(mathfn(v_to_d(a[0]), v_to_d(a[1])), a[0]); \
}

/* --- 数学関数 (移植元: ExponentialFuncs.cs, TrigonometricFuncs.cs) --- */

UNARY_DOUBLE_FN(bi_sqrt,  sqrt)
UNARY_DOUBLE_FN(bi_log2,  log2)
UNARY_DOUBLE_FN(bi_log10, log10)

/* bi_exp / bi_log: mpdecimal で計算 (E^x との一致のため) */
static val_t *bi_exp(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t ra, out;
    real_init(&ra); real_init(&out);
    val_as_real(&ra, a[0]);
    real_exp(&out, &ra);
    return val_new_real(&out, a[0]->fmt);
}
static val_t *bi_log(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t ra, out;
    real_init(&ra); real_init(&out);
    val_as_real(&ra, a[0]);
    real_ln(&out, &ra);
    return val_new_real(&out, a[0]->fmt);
}
UNARY_DOUBLE_FN(bi_sin,   sin)
UNARY_DOUBLE_FN(bi_cos,   cos)
UNARY_DOUBLE_FN(bi_tan,   tan)
UNARY_DOUBLE_FN(bi_asin,  asin)
UNARY_DOUBLE_FN(bi_acos,  acos)
UNARY_DOUBLE_FN(bi_atan,  atan)
UNARY_DOUBLE_FN(bi_sinh,  sinh)
UNARY_DOUBLE_FN(bi_cosh,  cosh)
UNARY_DOUBLE_FN(bi_tanh,  tanh)

/* bi_pow: mpdecimal 整数べき乗 (E^10 との一致のため) */
static val_t *bi_pow(val_t **a, int n, void *ctx) {
    (void)n;
    real_t ra, rb, out;
    real_init(&ra); real_init(&rb); real_init(&out);
    val_as_real(&ra, a[0]);
    val_as_real(&rb, a[1]);
    real_pow(&out, &ra, &rb);
    if (real_is_special(&out)) {
        EVAL_ERROR((eval_ctx_t *)ctx, 0, "Result too large.");
        return NULL;
    }
    return val_new_real(&out, a[0]->fmt);
}
BINARY_DOUBLE_FN(bi_atan2, atan2)

/* clog2: ceiling(log2(x)) */
static val_t *bi_clog2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    double v = v_to_d(a[0]);
    return d_to_v(ceil(log2(v)), a[0]);
}

/* clog10: ceiling(log10(x)) */
static val_t *bi_clog10(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    double v = v_to_d(a[0]);
    return d_to_v(ceil(log10(v)), a[0]);
}

/* --- 丸め関数 (移植元: RoundingFuncs.cs) --- */

UNARY_DOUBLE_FN(bi_floor, floor)
UNARY_DOUBLE_FN(bi_ceil,  ceil)
UNARY_DOUBLE_FN(bi_trunc, trunc)

static val_t *bi_round(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return d_to_v(round(v_to_d(a[0])), a[0]);
}

/* --- 絶対値・符号 (移植元: Absolute_SignFuncs.cs) --- */

static val_t *bi_abs(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    double v = v_to_d(a[0]);
    return d_to_v(fabs(v), a[0]);
}

static val_t *bi_sign(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    double v = v_to_d(a[0]);
    return val_new_i64(v > 0 ? 1 : (v < 0 ? -1 : 0), FMT_INT);
}

/* --- 最大・最小 (移植元: Min_MaxFuncs.cs) --- */

static val_t *bi_max(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n == 1 && a[0]->type == VAL_ARRAY) {
        /* max(array) */
        val_t **items = a[0]->arr_items;
        int len = a[0]->arr_len;
        if (len == 0) return val_new_null();
        double best = v_to_d(items[0]);
        int bi = 0;
        for (int i = 1; i < len; i++) {
            double v = v_to_d(items[i]);
            if (v > best) { best = v; bi = i; }
        }
        return val_dup(items[bi]);
    }
    if (n == 0) return val_new_null();
    double best = v_to_d(a[0]);
    int bi = 0;
    for (int i = 1; i < n; i++) {
        double v = v_to_d(a[i]);
        if (v > best) { best = v; bi = i; }
    }
    return val_dup(a[bi]);
}

static val_t *bi_min(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n == 1 && a[0]->type == VAL_ARRAY) {
        val_t **items = a[0]->arr_items;
        int len = a[0]->arr_len;
        if (len == 0) return val_new_null();
        double best = v_to_d(items[0]);
        int bi = 0;
        for (int i = 1; i < len; i++) {
            double v = v_to_d(items[i]);
            if (v < best) { best = v; bi = i; }
        }
        return val_dup(items[bi]);
    }
    if (n == 0) return val_new_null();
    double best = v_to_d(a[0]);
    int bi = 0;
    for (int i = 1; i < n; i++) {
        double v = v_to_d(a[i]);
        if (v < best) { best = v; bi = i; }
    }
    return val_dup(a[bi]);
}

/* --- GCD / LCM (移植元: Gcd_LcmFuncs.cs) --- */

static val_t *bi_gcd(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t ra, rb, rout;
    val_as_real(&ra, a[0]);
    val_as_real(&rb, a[1]);
    real_gcd(&rout, &ra, &rb);
    return val_new_real(&rout, FMT_INT);
}

static val_t *bi_lcm(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t ra, rb, g, prod, rout;
    val_as_real(&ra, a[0]);
    val_as_real(&rb, a[1]);
    real_gcd(&g, &ra, &rb);
    real_mul(&prod, &ra, &rb);
    real_divint(&rout, &prod, &g);
    return val_new_real(&rout, FMT_INT);
}

/* --- アサーション (移植元: AssertionFuncs.cs) --- */

static val_t *bi_assert(val_t **a, int n, void *ctx) {
    (void)n;
    eval_ctx_t *ectx = (eval_ctx_t *)ctx;
    if (!val_as_bool(a[0])) {
        char msg[256] = "Assertion failed";
        if (n >= 2 && a[1]->type == VAL_STR)
            snprintf(msg, sizeof(msg), "Assertion failed: %s", a[1]->str_v);
        EVAL_ERROR(ectx, 0, "%s", msg);
        return NULL;
    }
    return val_new_bool(true);
}

/* all(array): 配列のすべての要素が true か */
static val_t *bi_all(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(val_as_bool(a[0]));
    for (int i = 0; i < a[0]->arr_len; i++) {
        if (!val_as_bool(a[0]->arr_items[i])) return val_new_bool(false);
    }
    return val_new_bool(true);
}

/* any(array): 配列のいずれかの要素が true か */
static val_t *bi_any(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(val_as_bool(a[0]));
    for (int i = 0; i < a[0]->arr_len; i++) {
        if (val_as_bool(a[0]->arr_items[i])) return val_new_bool(true);
    }
    return val_new_bool(false);
}

/* --- フォーマット変換関数 (移植元: RepresentationFuncs.cs) --- */

static val_t *bi_datetime(val_t **a, int n, void *ctx){ (void)ctx;(void)n; return val_reformat(a[0], FMT_DATETIME); }

/* --- 日時変換 (移植元: DateTimeFuncs.cs) --- */

static val_t *bi_now(val_t **a, int n, void *ctx) {
    (void)a; (void)ctx; (void)n;
    real_t r;
    real_from_i64(&r, (int64_t)time(NULL));
    return val_new_real(&r, FMT_DATETIME);
}
static val_t *bi_fromDays(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 24*60*60);
    real_mul(&out, &r, &factor);
    return val_new_real(&out, FMT_DATETIME);
}
static val_t *bi_fromHours(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 60*60);
    real_mul(&out, &r, &factor);
    return val_new_real(&out, FMT_DATETIME);
}
static val_t *bi_fromMinutes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 60);
    real_mul(&out, &r, &factor);
    return val_new_real(&out, FMT_DATETIME);
}
static val_t *bi_fromSeconds(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return val_reformat(a[0], FMT_DATETIME);
}
static val_t *bi_toDays(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 24*60*60);
    real_div(&out, &r, &factor);
    return val_new_real(&out, FMT_REAL);
}
static val_t *bi_toHours(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 60*60);
    real_div(&out, &r, &factor);
    return val_new_real(&out, FMT_REAL);
}
static val_t *bi_toMinutes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    real_t r, factor, out;
    val_as_real(&r, a[0]); real_from_i64(&factor, 60);
    real_div(&out, &r, &factor);
    return val_new_real(&out, FMT_REAL);
}
static val_t *bi_toSeconds(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return val_reformat(a[0], FMT_REAL);
}
static val_t *bi_bin(val_t **a, int n, void *ctx)  { (void)ctx;(void)n; return val_reformat(a[0], FMT_BIN); }
static val_t *bi_dec(val_t **a, int n, void *ctx)  { (void)ctx;(void)n; return val_reformat(a[0], FMT_REAL); }
static val_t *bi_hex(val_t **a, int n, void *ctx)  { (void)ctx;(void)n; return val_reformat(a[0], FMT_HEX); }
static val_t *bi_oct(val_t **a, int n, void *ctx)  { (void)ctx;(void)n; return val_reformat(a[0], FMT_OCT); }
static val_t *bi_char(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_reformat(a[0], FMT_CHAR); }
static val_t *bi_kibi(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_reformat(a[0], FMT_BIN_PREFIX); }
static val_t *bi_si_fn(val_t **a, int n, void *ctx){ (void)ctx;(void)n; return val_reformat(a[0], FMT_SI_PREFIX); }

/* --- 乱数 (移植元: RandomFuncs.cs) --- */

static val_t *bi_rand0(val_t **a, int n, void *ctx) {
    (void)a; (void)n; (void)ctx;
    return val_new_double((double)rand() / (double)RAND_MAX, FMT_REAL);
}
static val_t *bi_rand2(val_t **a, int n, void *ctx) {
    (void)n; (void)ctx;
    double mn = val_as_double(a[0]), mx = val_as_double(a[1]);
    return val_new_double(mn + ((double)rand() / (double)RAND_MAX) * (mx - mn), FMT_REAL);
}
static val_t *bi_rand32(val_t **a, int n, void *ctx) {
    (void)a; (void)n; (void)ctx;
    return val_new_i64((int64_t)(int32_t)rand(), FMT_INT);
}
static val_t *bi_rand64(val_t **a, int n, void *ctx) {
    (void)a; (void)n; (void)ctx;
    int64_t v = ((int64_t)rand() << 32) | (int64_t)(uint32_t)rand();
    return val_new_i64(v, FMT_INT);
}

/* --- グラフ (移植元: PlottingFuncs.cs) — GUI未実装のためスタブ --- */

static val_t *bi_plot(val_t **a, int n, void *ctx) {
    (void)a; (void)n; (void)ctx;
    return val_new_null();
}

/* --- 組み込み関数テーブル --- */

typedef struct {
    const char   *name;
    int           n_params;  /* -1 = variadic */
    val_t       *(*fn)(val_t **args, int n, void *ctx);
    int           vec_arg_idx; /* 配列ブロードキャスト対象引数 (-1=なし、 0=第1引数、 ...) */
} builtin_entry_t;

static const builtin_entry_t BUILTIN_TABLE[] = {
    /* 指数・対数 (スカラー→スカラー、第1引数ブロードキャスト) */
    { "pow",    2, bi_pow,    -1 },
    { "sqrt",   1, bi_sqrt,    0 },
    { "exp",    1, bi_exp,     0 },
    { "log",    1, bi_log,     0 },
    { "log2",   1, bi_log2,    0 },
    { "log10",  1, bi_log10,   0 },
    { "clog2",  1, bi_clog2,   0 },
    { "clog10", 1, bi_clog10,  0 },
    /* 三角関数 */
    { "sin",    1, bi_sin,     0 },
    { "cos",    1, bi_cos,     0 },
    { "tan",    1, bi_tan,     0 },
    { "asin",   1, bi_asin,    0 },
    { "acos",   1, bi_acos,    0 },
    { "atan",   1, bi_atan,    0 },
    { "atan2",  2, bi_atan2,  -1 },
    { "sinh",   1, bi_sinh,    0 },
    { "cosh",   1, bi_cosh,    0 },
    { "tanh",   1, bi_tanh,    0 },
    /* 丸め */
    { "floor",  1, bi_floor,   0 },
    { "ceil",   1, bi_ceil,    0 },
    { "trunc",  1, bi_trunc,   0 },
    { "round",  1, bi_round,   0 },
    /* 絶対値・符号 */
    { "abs",    1, bi_abs,     0 },
    { "sign",   1, bi_sign,    0 },
    /* 最大・最小 (配列全体を受け取る) */
    { "max",   -1, bi_max,    -1 },
    { "min",   -1, bi_min,    -1 },
    /* GCD / LCM */
    { "gcd",    2, bi_gcd,    -1 },
    { "lcm",    2, bi_lcm,    -1 },
    /* アサーション (配列も受け取れる) */
    { "assert", -1, bi_assert,-1 },
    { "all",     1, bi_all,   -1 },
    { "any",     1, bi_any,   -1 },
    /* フォーマット変換 (移植元: RepresentationFuncs.cs) */
    { "datetime",  1, bi_datetime,    0 },
    { "now",       0, bi_now,        -1 },
    { "fromDays",  1, bi_fromDays,    0 },
    { "fromHours", 1, bi_fromHours,   0 },
    { "fromMinutes",1,bi_fromMinutes, 0 },
    { "fromSeconds",1,bi_fromSeconds, 0 },
    { "toDays",    1, bi_toDays,      0 },
    { "toHours",   1, bi_toHours,     0 },
    { "toMinutes", 1, bi_toMinutes,   0 },
    { "toSeconds", 1, bi_toSeconds,   0 },
    { "bin",     1, bi_bin,    0 },
    { "dec",     1, bi_dec,    0 },
    { "hex",     1, bi_hex,    0 },
    { "oct",     1, bi_oct,    0 },
    { "char",    1, bi_char,   0 },
    { "kibi",    1, bi_kibi,   0 },
    { "si",      1, bi_si_fn,  0 },
    /* 乱数 */
    { "rand",    0, bi_rand0, -1 },
    { "rand",    2, bi_rand2, -1 },
    { "rand32",  0, bi_rand32,-1 },
    { "rand64",  0, bi_rand64,-1 },
    /* グラフ (スタブ) */
    { "plot",    1, bi_plot,  -1 },
    { NULL, 0, NULL, -1 }
};

/* --- 公開 API --- */

static func_def_t *make_builtin(const builtin_entry_t *e) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    strncpy(fd->name, e->name, sizeof(fd->name) - 1);
    fd->n_params    = e->n_params;
    fd->vec_arg_idx = e->vec_arg_idx;
    fd->variadic    = (e->n_params == -1);
    fd->builtin     = e->fn;
    return fd;
}

func_def_t *builtin_find(const char *name, int n_args) {
    for (int i = 0; BUILTIN_TABLE[i].name; i++) {
        const builtin_entry_t *e = &BUILTIN_TABLE[i];
        if (strcmp(e->name, name) != 0) continue;
        if (e->n_params != -1 && e->n_params != n_args) continue;
        return make_builtin(e);
    }
    return NULL;
}

void builtin_enum_main(builtin_enum_cb cb, void *userdata) {
    for (int i = 0; BUILTIN_TABLE[i].name; i++) {
        cb(BUILTIN_TABLE[i].name, BUILTIN_TABLE[i].n_params, userdata);
    }
}

void builtin_register_all(eval_ctx_t *ctx) {
    for (int i = 0; BUILTIN_TABLE[i].name; i++) {
        const builtin_entry_t *e = &BUILTIN_TABLE[i];
        func_def_t *fd = make_builtin(e);
        if (!fd) continue;
        /* 変数として登録 (VAL_FUNC) */
        eval_var_t *v = eval_ctx_ref_var(ctx, e->name, true);
        if (v) {
            val_free(v->value);
            v->value    = val_new_func(fd);
            v->readonly = false;  /* ユーザが上書きできる */
        } else {
            func_def_free(fd);
            free(fd);
        }
    }
}
