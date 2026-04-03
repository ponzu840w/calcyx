/* 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイル
 * (ExponentialFuncs, TrigonometricFuncs, RoundingFuncs,
 *  Absolute_SignFuncs, Min_MaxFuncs, Gcd_LcmFuncs, AssertionFuncs) */

#include "builtin.h"
#include "eval_ctx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* ======================================================
 * ユーティリティ
 * ====================================================== */

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

/* ======================================================
 * 数学関数 (移植元: ExponentialFuncs.cs, TrigonometricFuncs.cs)
 * ====================================================== */

UNARY_DOUBLE_FN(bi_sqrt,  sqrt)
UNARY_DOUBLE_FN(bi_exp,   exp)
UNARY_DOUBLE_FN(bi_log,   log)
UNARY_DOUBLE_FN(bi_log2,  log2)
UNARY_DOUBLE_FN(bi_log10, log10)
UNARY_DOUBLE_FN(bi_sin,   sin)
UNARY_DOUBLE_FN(bi_cos,   cos)
UNARY_DOUBLE_FN(bi_tan,   tan)
UNARY_DOUBLE_FN(bi_asin,  asin)
UNARY_DOUBLE_FN(bi_acos,  acos)
UNARY_DOUBLE_FN(bi_atan,  atan)
UNARY_DOUBLE_FN(bi_sinh,  sinh)
UNARY_DOUBLE_FN(bi_cosh,  cosh)
UNARY_DOUBLE_FN(bi_tanh,  tanh)

BINARY_DOUBLE_FN(bi_pow,   pow)
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

/* ======================================================
 * 丸め関数 (移植元: RoundingFuncs.cs)
 * ====================================================== */

UNARY_DOUBLE_FN(bi_floor, floor)
UNARY_DOUBLE_FN(bi_ceil,  ceil)
UNARY_DOUBLE_FN(bi_trunc, trunc)

static val_t *bi_round(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return d_to_v(round(v_to_d(a[0])), a[0]);
}

/* ======================================================
 * 絶対値・符号 (移植元: Absolute_SignFuncs.cs)
 * ====================================================== */

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

/* ======================================================
 * 最大・最小 (移植元: Min_MaxFuncs.cs)
 * ====================================================== */

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

/* ======================================================
 * GCD / LCM (移植元: Gcd_LcmFuncs.cs)
 * ====================================================== */

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

/* ======================================================
 * アサーション (移植元: AssertionFuncs.cs)
 * ====================================================== */

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

/* ======================================================
 * 組み込み関数テーブル
 * ====================================================== */

typedef struct {
    const char   *name;
    int           n_params;  /* -1 = variadic */
    val_t       *(*fn)(val_t **args, int n, void *ctx);
} builtin_entry_t;

static const builtin_entry_t BUILTIN_TABLE[] = {
    /* 指数・対数 */
    { "pow",    2, bi_pow    },
    { "sqrt",   1, bi_sqrt   },
    { "exp",    1, bi_exp    },
    { "log",    1, bi_log    },
    { "log2",   1, bi_log2   },
    { "log10",  1, bi_log10  },
    { "clog2",  1, bi_clog2  },
    { "clog10", 1, bi_clog10 },
    /* 三角関数 */
    { "sin",    1, bi_sin    },
    { "cos",    1, bi_cos    },
    { "tan",    1, bi_tan    },
    { "asin",   1, bi_asin   },
    { "acos",   1, bi_acos   },
    { "atan",   1, bi_atan   },
    { "atan2",  2, bi_atan2  },
    { "sinh",   1, bi_sinh   },
    { "cosh",   1, bi_cosh   },
    { "tanh",   1, bi_tanh   },
    /* 丸め */
    { "floor",  1, bi_floor  },
    { "ceil",   1, bi_ceil   },
    { "trunc",  1, bi_trunc  },
    { "round",  1, bi_round  },
    /* 絶対値・符号 */
    { "abs",    1, bi_abs    },
    { "sign",   1, bi_sign   },
    /* 最大・最小 */
    { "max",   -1, bi_max    },
    { "min",   -1, bi_min    },
    /* GCD / LCM */
    { "gcd",    2, bi_gcd    },
    { "lcm",    2, bi_lcm    },
    /* アサーション */
    { "assert", -1, bi_assert },
    { "all",     1, bi_all   },
    { "any",     1, bi_any   },
    { NULL, 0, NULL }
};

/* ======================================================
 * 公開 API
 * ====================================================== */

static func_def_t *make_builtin(const builtin_entry_t *e) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    strncpy(fd->name, e->name, sizeof(fd->name) - 1);
    fd->n_params    = e->n_params;
    fd->vec_arg_idx = -1;
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
