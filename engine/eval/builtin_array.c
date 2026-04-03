/* 移植元: Calctus/Model/Functions/BuiltIns/ArrayFuncs.cs,
 *          Sum_AverageFuncs.cs, PrimeNumberFuncs.cs, SolveFuncs.cs,
 *          StringFuncs.cs, Absolute_SignFuncs.cs (mag)
 *          Calctus/Model/Mathematics/NewtonsMethod.cs, RMath.cs */

#include "builtin.h"
#include "eval_ctx.h"
#include "eval.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

/* ======================================================
 * ヘルパー: 関数値を引数付きで呼び出す
 * ====================================================== */

/* ctx が NULL の場合は子コンテキストを作れないのでダミーを使う */
static val_t *call_fd_1(func_def_t *fd, val_t *arg, eval_ctx_t *ctx) {
    val_t *args[1] = { arg };
    /* eval.c の call_func と同等の処理をここでも行う */
    if (fd->builtin) return fd->builtin(args, 1, ctx);
    if (!fd->body || !ctx) return NULL;
    if (ctx->depth >= EVAL_DEPTH_MAX) return NULL;
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    if (fd->n_params >= 1 && fd->param_names && fd->param_names[0])
        eval_ctx_set_var(&child, fd->param_names[0], val_dup(arg));
    val_t *r = expr_eval((expr_t *)fd->body, &child);
    if (child.has_error && !ctx->has_error) {
        ctx->has_error = child.has_error;
        ctx->error_pos = child.error_pos;
        memcpy(ctx->error_msg, child.error_msg, sizeof(ctx->error_msg));
    }
    eval_ctx_free(&child);
    return r;
}

static val_t *call_fd_2(func_def_t *fd, val_t *a0, val_t *a1, eval_ctx_t *ctx) {
    val_t *args[2] = { a0, a1 };
    if (fd->builtin) return fd->builtin(args, 2, ctx);
    if (!fd->body || !ctx) return NULL;
    if (ctx->depth >= EVAL_DEPTH_MAX) return NULL;
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    if (fd->n_params >= 1 && fd->param_names && fd->param_names[0])
        eval_ctx_set_var(&child, fd->param_names[0], val_dup(a0));
    if (fd->n_params >= 2 && fd->param_names && fd->param_names[1])
        eval_ctx_set_var(&child, fd->param_names[1], val_dup(a1));
    val_t *r = expr_eval((expr_t *)fd->body, &child);
    if (child.has_error && !ctx->has_error) {
        ctx->has_error = child.has_error;
        ctx->error_pos = child.error_pos;
        memcpy(ctx->error_msg, child.error_msg, sizeof(ctx->error_msg));
    }
    eval_ctx_free(&child);
    return r;
}

/* a[n] から func_def_t を取り出す */
static func_def_t *get_fd(val_t *v) {
    if (v && v->type == VAL_FUNC) return v->func_v;
    return NULL;
}

/* ======================================================
 * mag(x...) — Euclidean norm (移植元: Absolute_SignFuncs.cs)
 * ====================================================== */

static val_t *bi_mag(val_t **a, int n, void *ctx) {
    (void)ctx;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        double v = val_as_double(a[i]);
        sum += v * v;
    }
    return val_new_double(sqrt(sum), a[0]->fmt);
}

/* ======================================================
 * len(array_or_str) (移植元: ArrayFuncs.cs)
 * ====================================================== */

static val_t *bi_len(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_ARRAY)
        return val_new_i64(a[0]->arr_len, FMT_INT);
    if (a[0]->type == VAL_STR)
        return val_new_i64((int64_t)strlen(a[0]->str_v), FMT_INT);
    return val_new_i64(1, FMT_INT);
}

/* ======================================================
 * range / rangeInclusive (移植元: ArrayFuncs.cs, RMath.Range)
 * ====================================================== */

/* step=0 は自動 */
static val_t *make_range_fn(double start, double stop, double step, bool inclusive) {
    if (step == 0.0) step = (start <= stop) ? 1.0 : -1.0;
    int n = (int)ceil((stop - start) / step);
    if (n < 0) n = 0;
    if (inclusive && start + n * step == stop) n++;
    if (n > 1000000) n = 1000000;
    val_t **items = (val_t **)malloc((size_t)n * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < n; i++)
        items[i] = val_new_double(start + step * i, FMT_REAL);
    val_t *out = val_new_array(items, n, FMT_REAL);
    for (int i = 0; i < n; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_range2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_fn(val_as_double(a[0]), val_as_double(a[1]), 0, false);
}
static val_t *bi_range3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_fn(val_as_double(a[0]), val_as_double(a[1]),
                         val_as_double(a[2]), false);
}
static val_t *bi_rangeIncl2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_fn(val_as_double(a[0]), val_as_double(a[1]), 0, true);
}
static val_t *bi_rangeIncl3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_fn(val_as_double(a[0]), val_as_double(a[1]),
                         val_as_double(a[2]), true);
}

/* ======================================================
 * 配列操作 (移植元: ArrayFuncs.cs)
 * ====================================================== */

static val_t *bi_concat(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int na = a[0]->type == VAL_ARRAY ? a[0]->arr_len : 0;
    int nb = a[1]->type == VAL_ARRAY ? a[1]->arr_len : 0;
    val_t **items = (val_t **)malloc((size_t)(na + nb) * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < na; i++) items[i]    = val_dup(a[0]->arr_items[i]);
    for (int i = 0; i < nb; i++) items[na+i] = val_dup(a[1]->arr_items[i]);
    val_t *out = val_new_array(items, na + nb, a[0]->fmt);
    for (int i = 0; i < na + nb; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_reverseArray(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < len; i++) items[i] = val_dup(a[0]->arr_items[len - 1 - i]);
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_map(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return NULL;
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < len; i++) {
        items[i] = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (!items[i]) items[i] = val_new_null();
    }
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_filter(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return NULL;
    int len = a[0]->arr_len;
    val_t **tmp = (val_t **)malloc((size_t)len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        val_t *r = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (r && val_as_bool(r)) tmp[cnt++] = val_dup(a[0]->arr_items[i]);
        val_free(r);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

static val_t *bi_count_fn(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return NULL;
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (r && val_as_bool(r)) cnt++;
        val_free(r);
    }
    return val_new_i64(cnt, FMT_INT);
}

/* sort: compare two vals numerically */
typedef struct { val_t **arr; eval_ctx_t *ctx; func_def_t *key_fd; } sort_data_t;
static sort_data_t *g_sort_data = NULL;

static int sort_cmp(const void *pa, const void *pb) {
    val_t *a = *(val_t **)pa;
    val_t *b = *(val_t **)pb;
    sort_data_t *sd = g_sort_data;
    double da, db;
    if (sd->key_fd) {
        val_t *ka = call_fd_1(sd->key_fd, a, sd->ctx);
        val_t *kb = call_fd_1(sd->key_fd, b, sd->ctx);
        da = ka ? val_as_double(ka) : 0;
        db = kb ? val_as_double(kb) : 0;
        val_free(ka); val_free(kb);
    } else {
        da = val_as_double(a);
        db = val_as_double(b);
    }
    return (da > db) - (da < db);
}

static val_t *bi_sort1(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    for (int i = 0; i < len; i++) items[i] = val_dup(a[0]->arr_items[i]);
    sort_data_t sd = { items, (eval_ctx_t *)ctx, NULL };
    g_sort_data = &sd;
    qsort(items, (size_t)len, sizeof(val_t *), sort_cmp);
    g_sort_data = NULL;
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_sort2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    func_def_t *fd = get_fd(a[1]);
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    for (int i = 0; i < len; i++) items[i] = val_dup(a[0]->arr_items[i]);
    sort_data_t sd = { items, (eval_ctx_t *)ctx, fd };
    g_sort_data = &sd;
    qsort(items, (size_t)len, sizeof(val_t *), sort_cmp);
    g_sort_data = NULL;
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_aggregate(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY || a[0]->arr_len == 0) return NULL;
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return NULL;
    val_t *acc = val_dup(a[0]->arr_items[0]);
    for (int i = 1; i < a[0]->arr_len; i++) {
        val_t *r = call_fd_2(fd, acc, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        val_free(acc);
        acc = r ? r : val_new_null();
    }
    return acc;
}

static val_t *bi_extend(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return NULL;
    int cnt = val_as_int(a[2]);
    int seed_len = a[0]->arr_len;
    val_t **list = (val_t **)malloc((size_t)(seed_len + cnt) * sizeof(val_t *));
    for (int i = 0; i < seed_len; i++) list[i] = val_dup(a[0]->arr_items[i]);
    int total = seed_len;
    for (int i = 0; i < cnt; i++) {
        /* call fd with current array */
        val_t *arr_so_far = val_new_array(list, total, a[0]->fmt);
        val_t *r = call_fd_1(fd, arr_so_far, (eval_ctx_t *)ctx);
        val_free(arr_so_far);
        list[total++] = r ? r : val_new_null();
    }
    val_t *out = val_new_array(list, total, a[0]->fmt);
    for (int i = 0; i < total; i++) val_free(list[i]);
    free(list);
    return out;
}

static val_t *bi_indexOf_arr(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* indexOf(str, str) */
    if (a[0]->type == VAL_STR) {
        char needle[512] = "";
        if (a[1]->type == VAL_STR) {
            strncpy(needle, a[1]->str_v, sizeof(needle)-1);
        } else {
            val_to_str(a[1], needle, sizeof(needle));
        }
        const char *p = strstr(a[0]->str_v, needle);
        return val_new_i64(p ? (int64_t)(p - a[0]->str_v) : -1, FMT_INT);
    }
    /* indexOf(array, val) */
    if (a[0]->type != VAL_ARRAY) return val_new_i64(-1, FMT_INT);
    /* func arg */
    if (a[1]->type == VAL_FUNC) {
        func_def_t *fd = get_fd(a[1]);
        for (int i = 0; i < a[0]->arr_len; i++) {
            val_t *r = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
            bool ok = r && val_as_bool(r);
            val_free(r);
            if (ok) return val_new_i64(i, FMT_INT);
        }
        return val_new_i64(-1, FMT_INT);
    }
    /* value comparison */
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *eq = val_eq(a[0]->arr_items[i], a[1]);
        bool ok = eq && val_as_bool(eq);
        val_free(eq);
        if (ok) return val_new_i64(i, FMT_INT);
    }
    return val_new_i64(-1, FMT_INT);
}

static val_t *bi_lastIndexOf_arr(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_STR) {
        char needle[512] = "";
        if (a[1]->type == VAL_STR) strncpy(needle, a[1]->str_v, sizeof(needle)-1);
        else val_to_str(a[1], needle, sizeof(needle));
        /* simple last-occurance search */
        const char *s = a[0]->str_v;
        size_t slen = strlen(s), nlen = strlen(needle);
        int64_t last = -1;
        for (size_t i = 0; i + nlen <= slen; i++) {
            if (strncmp(s + i, needle, nlen) == 0) last = (int64_t)i;
        }
        return val_new_i64(last, FMT_INT);
    }
    if (a[0]->type != VAL_ARRAY) return val_new_i64(-1, FMT_INT);
    for (int i = a[0]->arr_len - 1; i >= 0; i--) {
        val_t *eq = val_eq(a[0]->arr_items[i], a[1]);
        bool ok = eq && val_as_bool(eq);
        val_free(eq);
        if (ok) return val_new_i64(i, FMT_INT);
    }
    return val_new_i64(-1, FMT_INT);
}

static val_t *bi_contains_arr(val_t **a, int n, void *ctx) {
    val_t *idx = bi_indexOf_arr(a, n, ctx);
    bool ok = idx && (val_as_long(idx) >= 0);
    val_free(idx);
    return val_new_bool(ok);
}

/* 集合演算ヘルパー: a に b の要素が含まれるか */
static bool arr_contains_val(val_t *arr, val_t *v) {
    for (int i = 0; i < arr->arr_len; i++) {
        val_t *eq = val_eq(arr->arr_items[i], v);
        bool ok = eq && val_as_bool(eq);
        val_free(eq);
        if (ok) return true;
    }
    return false;
}

static val_t *bi_except(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    val_t **tmp = (val_t **)malloc((size_t)a[0]->arr_len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        if (!arr_contains_val(a[1], a[0]->arr_items[i]))
            tmp[cnt++] = val_dup(a[0]->arr_items[i]);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

static val_t *bi_intersect(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_array(NULL, 0, FMT_REAL);
    val_t **tmp = (val_t **)malloc((size_t)a[0]->arr_len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        if (arr_contains_val(a[1], a[0]->arr_items[i]))
            tmp[cnt++] = val_dup(a[0]->arr_items[i]);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

static val_t *bi_union_arr(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int na = a[0]->type == VAL_ARRAY ? a[0]->arr_len : 0;
    int nb = a[1]->type == VAL_ARRAY ? a[1]->arr_len : 0;
    val_t **tmp = (val_t **)malloc((size_t)(na + nb) * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < na; i++) tmp[cnt++] = val_dup(a[0]->arr_items[i]);
    for (int i = 0; i < nb; i++) {
        if (!arr_contains_val(a[0], a[1]->arr_items[i]))
            tmp[cnt++] = val_dup(a[1]->arr_items[i]);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

static val_t *bi_unique1(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    val_t **tmp = (val_t **)malloc((size_t)a[0]->arr_len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        bool dup = false;
        for (int j = 0; j < cnt; j++) {
            val_t *eq = val_eq(tmp[j], a[0]->arr_items[i]);
            dup = eq && val_as_bool(eq);
            val_free(eq);
            if (dup) break;
        }
        if (!dup) tmp[cnt++] = val_dup(a[0]->arr_items[i]);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

/* unique(array, func): func(a)==func(b) なら同一 */
static val_t *bi_unique2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return val_dup(a[0]);
    val_t **tmp   = (val_t **)malloc((size_t)a[0]->arr_len * sizeof(val_t *));
    val_t **keys  = (val_t **)malloc((size_t)a[0]->arr_len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *ki = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        bool dup = false;
        for (int j = 0; j < cnt; j++) {
            val_t *eq = val_eq(keys[j], ki);
            dup = eq && val_as_bool(eq);
            val_free(eq);
            if (dup) break;
        }
        if (!dup) {
            tmp[cnt]  = val_dup(a[0]->arr_items[i]);
            keys[cnt] = ki;
            cnt++;
        } else {
            val_free(ki);
        }
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) { val_free(tmp[i]); val_free(keys[i]); }
    free(tmp); free(keys);
    return out;
}

/* all/any with function */
static val_t *bi_all2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(false);
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return val_new_bool(false);
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        bool ok = r && val_as_bool(r);
        val_free(r);
        if (!ok) return val_new_bool(false);
    }
    return val_new_bool(true);
}

static val_t *bi_any2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(false);
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return val_new_bool(false);
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        bool ok = r && val_as_bool(r);
        val_free(r);
        if (ok) return val_new_bool(true);
    }
    return val_new_bool(false);
}

/* ======================================================
 * 統計関数 (移植元: Sum_AverageFuncs.cs)
 * 引数はスカラー可変長 OR 単一配列
 * ====================================================== */

/* flatten: 引数 a[0..n-1] を double 配列に展開 */
static int flatten_args(val_t **a, int n, double *buf, int bufmax) {
    int cnt = 0;
    for (int i = 0; i < n && cnt < bufmax; i++) {
        if (a[i]->type == VAL_ARRAY) {
            for (int j = 0; j < a[i]->arr_len && cnt < bufmax; j++)
                buf[cnt++] = val_as_double(a[i]->arr_items[j]);
        } else {
            buf[cnt++] = val_as_double(a[i]);
        }
    }
    return cnt;
}

#define STAT_BUF 4096

static val_t *bi_sum(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    double s = 0; for (int i = 0; i < cnt; i++) s += buf[i];
    return val_new_double(s, a[0]->fmt);
}
static val_t *bi_ave(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double s = 0; for (int i = 0; i < cnt; i++) s += buf[i];
    return val_new_double(s / cnt, a[0]->fmt);
}
static val_t *bi_geoMean(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double p = 1; for (int i = 0; i < cnt; i++) p *= buf[i];
    return val_new_double(pow(p, 1.0/cnt), a[0]->fmt);
}
static val_t *bi_harMean(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double s = 0; for (int i = 0; i < cnt; i++) s += 1.0/buf[i];
    return val_new_double(cnt / s, a[0]->fmt);
}
static val_t *bi_invSum(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double s = 0; for (int i = 0; i < cnt; i++) s += 1.0/buf[i];
    return val_new_double(1.0 / s, a[0]->fmt);
}

/* ======================================================
 * 素数 (移植元: PrimeNumberFuncs.cs, RMath.cs)
 * ====================================================== */

static bool is_prime_i64(int64_t n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int64_t i = 3; i * i <= n; i += 2)
        if (n % i == 0) return false;
    return true;
}

static val_t *bi_isPrime(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return val_new_bool(is_prime_i64(val_as_long(a[0])));
}

static val_t *bi_prime(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int idx = val_as_int(a[0]);
    if (idx < 0) return val_new_i64(2, FMT_INT);
    int64_t v = 2;
    for (int i = 0; i < idx; i++) {
        v++;
        while (!is_prime_i64(v)) v++;
    }
    return val_new_i64(v, FMT_INT);
}

/* ======================================================
 * solve — Newton法 (移植元: NewtonsMethod.cs)
 * ====================================================== */

static double eval_func_at(func_def_t *fd, double x, eval_ctx_t *ctx) {
    val_t *xv = val_new_double(x, FMT_REAL);
    val_t *r  = call_fd_1(fd, xv, ctx);
    val_free(xv);
    double y = r ? val_as_double(r) : 0;
    val_free(r);
    return y;
}

/* 1回のニュートン法 */
static bool newton(func_def_t *fd, double init, double xmin, double xmax,
                   double h, double tol, double *result, eval_ctx_t *ctx) {
    double x = init;
    for (int i = 0; i < 50; i++) {
        double s = (eval_func_at(fd, x + h, ctx) - eval_func_at(fd, x - h, ctx)) / (2 * h);
        if (s == 0) return false;
        double y     = eval_func_at(fd, x, ctx);
        double nextx = x - y / s;
        if (nextx < xmin || nextx > xmax) return false;
        if (fabs(nextx - x) < tol) { *result = nextx; return true; }
        x = nextx;
    }
    return false;
}

/* 近接した解をまとめる */
static int reduce_sols(double *sols, int n, double tol) {
    if (n == 0) return 0;
    double tmp[1024];
    int cnt = 0;
    double acc = sols[0];
    int  acc_n = 1;
    for (int i = 1; i < n; i++) {
        if (sols[i] - sols[i-1] < tol * 10) {
            acc += sols[i]; acc_n++;
        } else {
            tmp[cnt++] = acc / acc_n;
            acc = sols[i]; acc_n = 1;
        }
    }
    tmp[cnt++] = acc / acc_n;
    memcpy(sols, tmp, (size_t)cnt * sizeof(double));
    return cnt;
}

static val_t *solve_impl(func_def_t *fd, double xmin, double xmax,
                          eval_ctx_t *ctx) {
    double h   = (xmax - xmin) > 0 ? (xmax - xmin) / 1e6 : 1e-6;
    double tol = h / 1e5;
    if (h < 1e-18) h = 1e-18;
    if (tol < 1e-23) tol = 1e-23;

    /* 初期値生成: [xmin, xmax] を 200 等分してサンプリング */
    int N = 200;
    double *sx = (double *)malloc((size_t)(N + 1) * sizeof(double));
    double *sy = (double *)malloc((size_t)(N + 1) * sizeof(double));
    for (int i = 0; i <= N; i++) {
        sx[i] = xmin + (xmax - xmin) * i / N;
        sy[i] = eval_func_at(fd, sx[i], ctx);
    }

    double raw_sols[1024];
    int n_raw = 0;

    for (int i = 0; i <= N && n_raw < 1024; i++) {
        bool candidate = false;
        if (sy[i] == 0) {
            candidate = true;
        } else if (i < N && (sy[i] > 0) != (sy[i+1] > 0)) {
            candidate = true;
        } else if (i > 0 && i < N) {
            bool pos_peak = sy[i] > 0 && sy[i] <= fmin(sy[i-1],sy[i+1]) && sy[i]*1.1 <= fmax(sy[i-1],sy[i+1]);
            bool neg_peak = sy[i] < 0 && sy[i] >= fmax(sy[i-1],sy[i+1]) && sy[i]*1.1 >= fmin(sy[i-1],sy[i+1]);
            if (pos_peak || neg_peak) candidate = true;
        }
        if (candidate && n_raw < 1024) {
            double sol;
            if (newton(fd, sx[i], xmin, xmax, h, tol, &sol, ctx))
                raw_sols[n_raw++] = sol;
        }
    }
    free(sx); free(sy);

    /* ソート */
    for (int i = 0; i < n_raw - 1; i++)
        for (int j = i+1; j < n_raw; j++)
            if (raw_sols[j] < raw_sols[i]) {
                double t = raw_sols[i]; raw_sols[i] = raw_sols[j]; raw_sols[j] = t;
            }
    int n_sols = reduce_sols(raw_sols, n_raw, tol);

    if (n_sols == 0) return val_new_null();
    if (n_sols == 1) return val_new_double(raw_sols[0], FMT_REAL);
    val_t **items = (val_t **)malloc((size_t)n_sols * sizeof(val_t *));
    for (int i = 0; i < n_sols; i++) items[i] = val_new_double(raw_sols[i], FMT_REAL);
    val_t *out = val_new_array(items, n_sols, FMT_REAL);
    for (int i = 0; i < n_sols; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_solve1(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = get_fd(a[0]);
    if (!fd) return NULL;
    return solve_impl(fd, -1e15, 1e15, (eval_ctx_t *)ctx);
}

static val_t *bi_solve2(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = get_fd(a[0]);
    if (!fd) return NULL;
    /* 配列の場合は初期値のリスト */
    if (a[1]->type == VAL_ARRAY) {
        double sols[256];
        int cnt = 0;
        double h = 1e-6, tol = 1e-11;
        for (int i = 0; i < a[1]->arr_len && cnt < 256; i++) {
            double init = val_as_double(a[1]->arr_items[i]);
            double sol;
            if (newton(fd, init, -1e15, 1e15, h, tol, &sol, (eval_ctx_t *)ctx))
                sols[cnt++] = sol;
        }
        for (int i = 0; i < cnt - 1; i++)
            for (int j = i+1; j < cnt; j++)
                if (sols[j] < sols[i]) { double t = sols[i]; sols[i] = sols[j]; sols[j] = t; }
        cnt = reduce_sols(sols, cnt, tol);
        if (cnt == 0) return val_new_null();
        if (cnt == 1) return val_new_double(sols[0], FMT_REAL);
        val_t **items = (val_t **)malloc((size_t)cnt * sizeof(val_t *));
        for (int i = 0; i < cnt; i++) items[i] = val_new_double(sols[i], FMT_REAL);
        val_t *out = val_new_array(items, cnt, FMT_REAL);
        for (int i = 0; i < cnt; i++) val_free(items[i]);
        free(items);
        return out;
    }
    /* スカラー初期値 */
    double init = val_as_double(a[1]);
    double sol;
    double h = 1e-6, tol = 1e-11;
    if (newton(fd, init, -1e15, 1e15, h, tol, &sol, (eval_ctx_t *)ctx))
        return val_new_double(sol, FMT_REAL);
    return val_new_null();
}

static val_t *bi_solve3(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = get_fd(a[0]);
    if (!fd) return NULL;
    double xmin = val_as_double(a[1]);
    double xmax = val_as_double(a[2]);
    return solve_impl(fd, xmin, xmax, (eval_ctx_t *)ctx);
}

/* ======================================================
 * 文字列関数 (移植元: StringFuncs.cs)
 * ====================================================== */

static val_t *bi_str(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    char buf[1024];
    if (a[0]->type == VAL_ARRAY) {
        /* 配列→文字列: 各要素を char コードとして連結 */
        size_t pos = 0;
        for (int i = 0; i < a[0]->arr_len && pos < sizeof(buf) - 4; i++) {
            int64_t c = val_as_long(a[0]->arr_items[i]);
            /* UTF-8 エンコード (最大4バイト) */
            if (c < 0x80) {
                buf[pos++] = (char)c;
            } else if (c < 0x800) {
                buf[pos++] = (char)(0xC0 | (c >> 6));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            } else if (c < 0x10000) {
                buf[pos++] = (char)(0xE0 | (c >> 12));
                buf[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            } else {
                buf[pos++] = (char)(0xF0 | (c >> 18));
                buf[pos++] = (char)(0x80 | ((c >> 12) & 0x3F));
                buf[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            }
        }
        buf[pos] = '\0';
        return val_new_str(buf);
    }
    val_to_str(a[0], buf, sizeof(buf));
    return val_new_str(buf);
}

/* array(str): 文字列 → char コードの配列 */
static val_t *bi_array_str(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const unsigned char *s = (const unsigned char *)a[0]->str_v;
    /* UTF-8 デコード */
    val_t *tmp[4096];
    int cnt = 0;
    for (size_t i = 0; s[i] && cnt < 4096; ) {
        int64_t cp;
        if (s[i] < 0x80) {
            cp = s[i++];
        } else if ((s[i] & 0xE0) == 0xC0) {
            cp = (s[i] & 0x1F); i++;
            if (s[i]) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        } else if ((s[i] & 0xF0) == 0xE0) {
            cp = (s[i] & 0x0F); i++;
            for (int k = 0; k < 2 && s[i]; k++) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        } else {
            cp = (s[i] & 0x07); i++;
            for (int k = 0; k < 3 && s[i]; k++) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        }
        tmp[cnt++] = val_new_i64(cp, FMT_CHAR);
    }
    val_t *out = val_new_array(tmp, cnt, FMT_CHAR);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    return out;
}

static val_t *bi_trim(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s = a[0]->str_v;
    while (*s == ' ' || *s == '\t') s++;
    char buf[1024]; strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    return val_new_str(buf);
}
static val_t *bi_trimStart(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s = a[0]->str_v;
    while (*s == ' ' || *s == '\t') s++;
    return val_new_str(s);
}
static val_t *bi_trimEnd(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; strncpy(buf, a[0]->str_v, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    return val_new_str(buf);
}

static val_t *bi_toLower(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; strncpy(buf, a[0]->str_v, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (int i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return val_new_str(buf);
}
static val_t *bi_toUpper(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; strncpy(buf, a[0]->str_v, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (int i = 0; buf[i]; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
    return val_new_str(buf);
}

static val_t *bi_replace(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s   = a[0]->str_v;
    char from[512] = "", to[512] = "";
    if (a[1]->type == VAL_STR) strncpy(from, a[1]->str_v, sizeof(from)-1);
    else val_to_str(a[1], from, sizeof(from));
    if (a[2]->type == VAL_STR) strncpy(to, a[2]->str_v, sizeof(to)-1);
    else val_to_str(a[2], to, sizeof(to));
    size_t flen = strlen(from), tlen = strlen(to);
    char buf[4096]; size_t pos = 0;
    while (*s && pos + tlen < sizeof(buf) - 1) {
        if (flen > 0 && strncmp(s, from, flen) == 0) {
            memcpy(buf + pos, to, tlen); pos += tlen; s += flen;
        } else {
            buf[pos++] = *s++;
        }
    }
    buf[pos] = '\0';
    return val_new_str(buf);
}

static val_t *bi_startsWith(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_new_bool(false);
    char prefix[512] = "";
    if (a[1]->type == VAL_STR) strncpy(prefix, a[1]->str_v, sizeof(prefix)-1);
    size_t plen = strlen(prefix);
    return val_new_bool(strncmp(a[0]->str_v, prefix, plen) == 0);
}
static val_t *bi_endsWith(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_new_bool(false);
    char suf[512] = "";
    if (a[1]->type == VAL_STR) strncpy(suf, a[1]->str_v, sizeof(suf)-1);
    size_t slen = strlen(a[0]->str_v), suflen = strlen(suf);
    if (suflen > slen) return val_new_bool(false);
    return val_new_bool(strcmp(a[0]->str_v + slen - suflen, suf) == 0);
}

static val_t *bi_split(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* split(sep, str) */
    char sep[512] = "", src[4096] = "";
    if (a[0]->type == VAL_STR) strncpy(sep, a[0]->str_v, sizeof(sep)-1);
    if (a[1]->type == VAL_STR) strncpy(src, a[1]->str_v, sizeof(src)-1);
    size_t seplen = strlen(sep);
    val_t *tmp[1024];
    int cnt = 0;
    if (seplen == 0) {
        /* 空セパレータ: 1文字ずつ */
        for (size_t i = 0; src[i] && cnt < 1024; i++) {
            char ch[2] = { src[i], '\0' };
            tmp[cnt++] = val_new_str(ch);
        }
    } else {
        char *p = src, *found;
        while ((found = strstr(p, sep)) != NULL && cnt < 1023) {
            size_t len = (size_t)(found - p);
            char buf[1024]; memcpy(buf, p, len); buf[len] = '\0';
            tmp[cnt++] = val_new_str(buf);
            p = found + seplen;
        }
        tmp[cnt++] = val_new_str(p);
    }
    val_t *out = val_new_array(tmp, cnt, FMT_STRING);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    return out;
}

static val_t *bi_join(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* join(sep, array) */
    char sep[512] = "";
    if (a[0]->type == VAL_STR) strncpy(sep, a[0]->str_v, sizeof(sep)-1);
    if (a[1]->type != VAL_ARRAY) return val_new_str("");
    char buf[4096]; size_t pos = 0;
    size_t seplen = strlen(sep);
    for (int i = 0; i < a[1]->arr_len; i++) {
        char elem[1024];
        if (a[1]->arr_items[i]->type == VAL_STR)
            strncpy(elem, a[1]->arr_items[i]->str_v, sizeof(elem)-1);
        else
            val_to_str(a[1]->arr_items[i], elem, sizeof(elem));
        size_t elen = strlen(elem);
        if (i > 0 && pos + seplen < sizeof(buf) - 1) {
            memcpy(buf + pos, sep, seplen); pos += seplen;
        }
        if (pos + elen < sizeof(buf) - 1) { memcpy(buf + pos, elem, elen); pos += elen; }
    }
    buf[pos] = '\0';
    return val_new_str(buf);
}

/* ======================================================
 * 追加の builtin_find / builtin_register_all 対応テーブル
 * builtin.c の BUILTIN_TABLE を拡張するため別のテーブルを用意し、
 * builtin_find_extra / builtin_register_extra として公開する
 * ====================================================== */

typedef struct {
    const char   *name;
    int           n_params;   /* -1 = variadic */
    val_t       *(*fn)(val_t **args, int n, void *ctx);
} bi_entry_t;

static const bi_entry_t EXTRA_TABLE[] = {
    /* 配列 */
    { "mag",          -1, bi_mag          },
    { "len",           1, bi_len          },
    { "range",         2, bi_range2       },
    { "range",         3, bi_range3       },
    { "rangeInclusive",2, bi_rangeIncl2   },
    { "rangeInclusive",3, bi_rangeIncl3   },
    { "concat",        2, bi_concat       },
    { "reverseArray",  1, bi_reverseArray },
    { "map",           2, bi_map          },
    { "filter",        2, bi_filter       },
    { "count",         2, bi_count_fn     },
    { "sort",          1, bi_sort1        },
    { "sort",          2, bi_sort2        },
    { "aggregate",     2, bi_aggregate    },
    { "extend",        3, bi_extend       },
    { "indexOf",       2, bi_indexOf_arr  },
    { "lastIndexOf",   2, bi_lastIndexOf_arr },
    { "contains",      2, bi_contains_arr },
    { "except",        2, bi_except       },
    { "intersect",     2, bi_intersect    },
    { "union",         2, bi_union_arr    },
    { "unique",        1, bi_unique1      },
    { "unique",        2, bi_unique2      },
    { "all",           2, bi_all2         },
    { "any",           2, bi_any2         },
    /* 統計 */
    { "sum",          -1, bi_sum          },
    { "ave",          -1, bi_ave          },
    { "geoMean",      -1, bi_geoMean      },
    { "harMean",      -1, bi_harMean      },
    { "invSum",       -1, bi_invSum       },
    /* 素数 */
    { "isPrime",       1, bi_isPrime      },
    { "prime",         1, bi_prime        },
    /* solve */
    { "solve",         1, bi_solve1       },
    { "solve",         2, bi_solve2       },
    { "solve",         3, bi_solve3       },
    /* 文字列 */
    { "str",           1, bi_str          },
    { "array",         1, bi_array_str    },
    { "trim",          1, bi_trim         },
    { "trimStart",     1, bi_trimStart    },
    { "trimEnd",       1, bi_trimEnd      },
    { "replace",       3, bi_replace      },
    { "toLower",       1, bi_toLower      },
    { "toUpper",       1, bi_toUpper      },
    { "startsWith",    2, bi_startsWith   },
    { "endsWith",      2, bi_endsWith     },
    { "split",         2, bi_split        },
    { "join",          2, bi_join         },
    { NULL, 0, NULL }
};

static func_def_t *make_extra(const bi_entry_t *e) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    strncpy(fd->name, e->name, sizeof(fd->name) - 1);
    fd->n_params    = e->n_params;
    fd->vec_arg_idx = -1;
    fd->variadic    = (e->n_params == -1);
    fd->builtin     = e->fn;
    return fd;
}

func_def_t *builtin_find_extra(const char *name, int n_args) {
    for (int i = 0; EXTRA_TABLE[i].name; i++) {
        const bi_entry_t *e = &EXTRA_TABLE[i];
        if (strcmp(e->name, name) != 0) continue;
        if (e->n_params != -1 && e->n_params != n_args) continue;
        return make_extra(e);
    }
    return NULL;
}

void builtin_register_extra(eval_ctx_t *ctx) {
    for (int i = 0; EXTRA_TABLE[i].name; i++) {
        const bi_entry_t *e = &EXTRA_TABLE[i];
        /* 同名が既に登録済みなら上書きしない (range は 2/3 引数両方登録) */
        eval_var_t *v = eval_ctx_ref_var(ctx, e->name, true);
        if (!v) continue;
        /* 既に非 NULL なら skip しない: overwrite して最新版にする */
        func_def_t *fd = make_extra(e);
        if (!fd) continue;
        val_free(v->value);
        v->value    = val_new_func(fd);
        v->readonly = false;
    }
}
