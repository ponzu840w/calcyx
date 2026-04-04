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
/* パラメータを子コンテキストに直接作成するヘルパー (親変数を上書きしない) */
static void bind_param(eval_ctx_t *child, const char *pname, val_t *val) {
    if (!pname || !pname[0] || child->n_vars >= EVAL_VAR_MAX) { val_free(val); return; }
    eval_var_t *nv = &child->vars[child->n_vars++];
    memset(nv, 0, sizeof(*nv));
    strncpy(nv->name, pname, TOK_TEXT_MAX - 1);
    nv->value    = val;
    nv->readonly = false;
}

static val_t *call_fd_1(func_def_t *fd, val_t *arg, eval_ctx_t *ctx) {
    val_t *args[1] = { arg };
    if (fd->builtin) return fd->builtin(args, 1, ctx);
    if (!fd->body || !ctx) return NULL;
    if (ctx->depth >= EVAL_DEPTH_MAX) return NULL;
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    if (fd->n_params >= 1 && fd->param_names && fd->param_names[0])
        bind_param(&child, fd->param_names[0], val_dup(arg));
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
        bind_param(&child, fd->param_names[0], val_dup(a0));
    if (fd->n_params >= 2 && fd->param_names && fd->param_names[1])
        bind_param(&child, fd->param_names[1], val_dup(a1));
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

/* mpdecimal 演算で精度を保ちながら range を生成 */
static val_t *make_range_real(val_t *va, val_t *vb, val_t *vc, bool inclusive) {
    real_t start, stop, step, cur, tmp;
    real_init(&start); real_init(&stop); real_init(&step);
    real_init(&cur);   real_init(&tmp);

    val_as_real(&start, va);
    val_as_real(&stop,  vb);
    if (vc) {
        val_as_real(&step, vc);
    } else {
        /* step = sign(stop - start) */
        real_sub(&tmp, &stop, &start);
        int s = real_sign(&tmp);
        real_from_i64(&step, s >= 0 ? 1 : -1);
    }

    bool going_up = (real_sign(&step) >= 0);

    /* 大まかな上限 = |stop - start| / |step| + 2 */
    int max_n = 1000000;
    {
        real_sub(&tmp, &stop, &start);
        double num = fabs(real_to_double(&tmp));
        double den = fabs(real_to_double(&step));
        if (den > 0) {
            double est = num / den + 2;
            if (est > 0 && est < (double)max_n) max_n = (int)est + 2;
        }
    }

    /* 要素数をカウント (real_add は out==a 不可なので tmp 経由) */
    int n = 0;
    real_copy(&cur, &start);
    for (int i = 0; i < max_n; i++) {
        int cmp = real_cmp(&cur, &stop);
        if (going_up) {
            if (inclusive ? (cmp > 0) : (cmp >= 0)) break;
        } else {
            if (inclusive ? (cmp < 0) : (cmp <= 0)) break;
        }
        n++;
        real_add(&tmp, &cur, &step);
        real_copy(&cur, &tmp);
    }

    val_t **items = (val_t **)malloc((size_t)(n > 0 ? n : 1) * sizeof(val_t *));
    if (!items) return NULL;

    real_copy(&cur, &start);
    for (int i = 0; i < n; i++) {
        items[i] = val_new_real(&cur, va->fmt);
        real_add(&tmp, &cur, &step);
        real_copy(&cur, &tmp);
    }
    val_t *out = val_new_array(items, n, va->fmt);
    for (int i = 0; i < n; i++) val_free(items[i]);
    free(items);
    return out;
}

static val_t *bi_range2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], NULL, false);
}
static val_t *bi_range3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], a[2], false);
}
static val_t *bi_rangeIncl2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], NULL, true);
}
static val_t *bi_rangeIncl3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], a[2], true);
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

/* unique(array, comparator): comparator(a,b)==true なら重複とみなす */
static val_t *bi_unique2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    func_def_t *fd = get_fd(a[1]);
    if (!fd) return val_dup(a[0]);
    int len = a[0]->arr_len;
    val_t **tmp = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!tmp) return NULL;
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        bool dup = false;
        for (int j = 0; j < cnt; j++) {
            val_t *eq = call_fd_2(fd, tmp[j], a[0]->arr_items[i], (eval_ctx_t *)ctx);
            dup = eq && val_as_bool(eq);
            val_free(eq);
            if (dup) break;
        }
        if (!dup) {
            tmp[cnt++] = val_dup(a[0]->arr_items[i]);
        }
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
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

/* primeFact(n): n の素因数分解 → 素因数の配列 (移植元: PrimeNumberFuncs.cs) */
static val_t *bi_primeFact(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int64_t num = val_as_long(a[0]);
    if (num < 2) return val_new_array(NULL, 0, FMT_INT);

    /* 最大素因数は sqrt(num) 以下、個数は最大 log2(num) ≈ 63 */
    val_t *buf[128];
    int cnt = 0;

    for (int64_t d = 2; d * d <= num; d++) {
        while (num % d == 0) {
            buf[cnt++] = val_new_i64(d, FMT_INT);
            num /= d;
        }
    }
    if (num > 1) buf[cnt++] = val_new_i64(num, FMT_INT);

    val_t *out = val_new_array(buf, cnt, FMT_INT);
    for (int i = 0; i < cnt; i++) val_free(buf[i]);
    return out;
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
        if (nextx < xmin - tol || nextx > xmax + tol) return false;
        if (nextx < xmin) nextx = xmin;
        if (nextx > xmax) nextx = xmax;
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
    /* 境界点を候補として追加 (境界が根になる場合の対策) */
    {
        double sol;
        if (n_raw < 1024 && newton(fd, xmin, xmin, xmax, h, tol, &sol, ctx))
            raw_sols[n_raw++] = sol;
        if (n_raw < 1024 && newton(fd, xmax, xmin, xmax, h, tol, &sol, ctx))
            raw_sols[n_raw++] = sol;
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

/* 対数サンプリングで初期候補を収集し、候補範囲から h/tol を決定する */
static val_t *bi_solve1(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = get_fd(a[0]);
    if (!fd) return NULL;
    eval_ctx_t *ectx = (eval_ctx_t *)ctx;

    /* 対数スケールで x を生成 (移植元: generateInitCandidates(center=0)) */
    const int SCALE_FINE  = 4;
    const int SCALE_RANGE = 18 * SCALE_FINE; /* 72 */
    /* 負側: 2*SCALE_RANGE 個, 中心: 1, 正側: 2*SCALE_RANGE 個 = 4*SCALE_RANGE+1 = 289 */
    double cands_x[300];
    double cands_y[300];
    int nc = 0;

    /* 負側 */
    for (int i = -SCALE_RANGE; i < SCALE_RANGE; i++) {
        double x = -(pow(10.0, (double)i / SCALE_FINE));
        cands_x[nc] = x;
        cands_y[nc] = eval_func_at(fd, x, ectx);
        nc++;
    }
    /* 中心 */
    cands_x[nc] = 0.0;
    cands_y[nc] = eval_func_at(fd, 0.0, ectx);
    nc++;
    /* 正側 */
    for (int i = -SCALE_RANGE; i < SCALE_RANGE; i++) {
        double x = pow(10.0, (double)i / SCALE_FINE);
        cands_x[nc] = x;
        cands_y[nc] = eval_func_at(fd, x, ectx);
        nc++;
    }

    /* x でソート (バブルソートで十分: nc ≦ 289) */
    for (int i = 0; i < nc - 1; i++)
        for (int j = i + 1; j < nc; j++)
            if (cands_x[j] < cands_x[i]) {
                double tx = cands_x[i]; cands_x[i] = cands_x[j]; cands_x[j] = tx;
                double ty = cands_y[i]; cands_y[i] = cands_y[j]; cands_y[j] = ty;
            }

    /* 符号変化点を収集してフィルタ */
    double filtered[1024];
    int nf = 0;
    for (int i = 0; i < nc && nf < 1024 - 4; i++) {
        bool add = false;
        if (cands_y[i] == 0.0) add = true;
        else if (i + 1 < nc && cands_y[i] * cands_y[i+1] < 0) add = true;
        if (add) {
            if (i > 0)     filtered[nf++] = cands_x[i-1];
            filtered[nf++] = cands_x[i];
            if (i+1 < nc)  filtered[nf++] = cands_x[i+1];
        }
    }

    /* h/tol をフィルタ済み候補範囲から決定 */
    double h, tol;
    if (nf <= 0) {
        h = 1e-6; tol = 1e-11;
    } else {
        double mn = filtered[0], mx = filtered[0];
        for (int i = 1; i < nf; i++) {
            if (filtered[i] < mn) mn = filtered[i];
            if (filtered[i] > mx) mx = filtered[i];
        }
        h   = fmax(1e-18, (mx - mn) / 1e6);
        tol = fmax(1e-23, h / 1e5);
    }

    /* ニュートン法を各候補に適用 */
    double raw_sols[1024];
    int n_raw = 0;
    for (int i = 0; i < nf && n_raw < 1024; i++) {
        double sol;
        if (newton(fd, filtered[i], -1e300, 1e300, h, tol, &sol, ectx))
            raw_sols[n_raw++] = sol;
    }

    /* ソートして近接する解を統合 */
    for (int i = 0; i < n_raw - 1; i++)
        for (int j = i+1; j < n_raw; j++)
            if (raw_sols[j] < raw_sols[i]) { double t=raw_sols[i]; raw_sols[i]=raw_sols[j]; raw_sols[j]=t; }
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
 * GrayCode (移植元: GrayCodeFuncs.cs)
 * ====================================================== */

static val_t *bi_toGray(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    return val_new_i64((int64_t)(x ^ (x >> 1)), a[0]->fmt);
}

static val_t *bi_fromGray(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    uint64_t mask = x >> 1;
    while (mask) { x ^= mask; mask >>= 1; }
    return val_new_i64((int64_t)x, a[0]->fmt);
}

/* ======================================================
 * BitByteOps (移植元: BitByteOpsFuncs.cs)
 * ====================================================== */

static val_t *bi_count1(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    int cnt = 0;
    while (x) { cnt += x & 1; x >>= 1; }
    return val_new_i64(cnt, FMT_INT);
}

/* pack(width, v0, v1, ...): フィールドを LSB から詰める */
static val_t *bi_pack(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n < 1) return val_new_i64(0, FMT_INT);
    /* 第1引数がスカラーの場合: pack(width, v0, v1, ...) */
    if (a[0]->type != VAL_ARRAY) {
        int w = val_as_int(a[0]);
        uint64_t result = 0;
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        for (int i = 1; i < n; i++) {
            result |= ((uint64_t)val_as_long(a[i]) & mask) << ((i-1)*w);
        }
        return val_new_i64((int64_t)result, FMT_HEX);
    }
    /* 第1引数が配列の場合: pack([w0,w1,...], v0, v1, ...) */
    int nw = a[0]->arr_len;
    uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < nw && i+1 < n; i++) {
        int w = val_as_int(a[0]->arr_items[i]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        result |= ((uint64_t)val_as_long(a[i+1]) & mask) << shift;
        shift += w;
    }
    return val_new_i64((int64_t)result, FMT_HEX);
}

/* reverseBits(width, v): v の下位 width ビットを反転 */
static val_t *bi_reverseBits(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int w = val_as_int(a[0]);
    uint64_t v = (uint64_t)val_as_long(a[1]);
    uint64_t out = 0;
    for (int i = 0; i < w; i++)
        out |= ((v >> i) & 1) << (w - 1 - i);
    /* 上位ビットは入力の上位ビットを保持 */
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    out = (v & ~mask) | (out & mask);
    return val_new_i64((int64_t)out, a[1]->fmt);
}

/* reverseBytes(nbytes, v): v の下位 nbytes バイトを反転 */
static val_t *bi_reverseBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int nb = val_as_int(a[0]);
    uint64_t v = (uint64_t)val_as_long(a[1]);
    uint8_t bytes[8];
    for (int i = 0; i < nb; i++) bytes[i] = (uint8_t)(v >> (i * 8));
    uint64_t out = v;
    for (int i = 0; i < nb; i++) {
        out &= ~(0xFFULL << (i * 8));
        out |= (uint64_t)bytes[nb - 1 - i] << (i * 8);
    }
    return val_new_i64((int64_t)out, a[1]->fmt);
}

/* rotateL(width, v) / rotateL(width, n, v): left rotate */
static val_t *bi_rotateL(val_t **a, int n, void *ctx) {
    (void)ctx;
    int w, amt;
    uint64_t v;
    if (n == 2) { w = val_as_int(a[0]); amt = 1; v = (uint64_t)val_as_long(a[1]); }
    else        { w = val_as_int(a[0]); amt = val_as_int(a[1]); v = (uint64_t)val_as_long(a[2]); }
    if (w <= 0 || w > 64) return val_new_i64((int64_t)v, a[n-1]->fmt);
    amt = ((amt % w) + w) % w;
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    uint64_t bits = v & mask;
    bits = ((bits << amt) | (bits >> (w - amt))) & mask;
    uint64_t out = (v & ~mask) | bits;
    return val_new_i64((int64_t)out, a[n-1]->fmt);
}

static val_t *bi_rotateR(val_t **a, int n, void *ctx) {
    (void)ctx;
    int w, amt;
    uint64_t v;
    if (n == 2) { w = val_as_int(a[0]); amt = 1; v = (uint64_t)val_as_long(a[1]); }
    else        { w = val_as_int(a[0]); amt = val_as_int(a[1]); v = (uint64_t)val_as_long(a[2]); }
    if (w <= 0 || w > 64) return val_new_i64((int64_t)v, a[n-1]->fmt);
    amt = ((amt % w) + w) % w;
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    uint64_t bits = v & mask;
    bits = ((bits >> amt) | (bits << (w - amt))) & mask;
    uint64_t out = (v & ~mask) | bits;
    return val_new_i64((int64_t)out, a[n-1]->fmt);
}

/* swapN: swap adjacent N-byte groups */
static uint64_t swap_bytes_n(uint64_t v, int n) {
    uint64_t out = 0;
    for (int i = 0; i < 8; i += 2*n) {
        for (int j = 0; j < n; j++) {
            int from = i + j;
            int to   = i + 2*n - 1 - j;
            if (from < 8) out |= ((v >> (from*8)) & 0xFFULL) << (to*8);
            if (to   < 8) out |= ((v >> (to*8))   & 0xFFULL) << (from*8);
        }
    }
    return out;
}
static val_t *bi_swap2(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),1), a[0]->fmt); }
static val_t *bi_swap4(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),2), a[0]->fmt); }
static val_t *bi_swap8(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),4), a[0]->fmt); }

/* swapNib: swap adjacent nibbles */
static val_t *bi_swapNib(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t v = (uint64_t)val_as_long(a[0]);
    uint64_t out = ((v & 0xF0F0F0F0F0F0F0F0ULL) >> 4) | ((v & 0x0F0F0F0F0F0F0F0FULL) << 4);
    return val_new_i64((int64_t)out, a[0]->fmt);
}

/* unpack: フィールドを解凍 */
static val_t *bi_unpack(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n < 2) return val_new_null();
    /* unpack(width, count, v): v を width ビットずつ count 個 */
    if (a[0]->type != VAL_ARRAY) {
        int w     = val_as_int(a[0]);
        int cnt   = val_as_int(a[1]);
        uint64_t v = (uint64_t)val_as_long(a[2]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        val_t *tmp[256];
        if (cnt > 256) cnt = 256;
        for (int i = 0; i < cnt; i++) {
            tmp[i] = val_new_i64((int64_t)((v >> (i*w)) & mask), FMT_INT);
        }
        val_t *out = val_new_array(tmp, cnt, FMT_INT);
        for (int i = 0; i < cnt; i++) val_free(tmp[i]);
        return out;
    }
    /* unpack([w0,w1,...], v): 可変長フィールド */
    int nw = a[0]->arr_len;
    uint64_t v = (uint64_t)val_as_long(a[1]);
    val_t *tmp[256];
    if (nw > 256) nw = 256;
    int shift = 0;
    for (int i = 0; i < nw; i++) {
        int w = val_as_int(a[0]->arr_items[i]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        tmp[i] = val_new_i64((int64_t)((v >> shift) & mask), FMT_HEX);
        shift += w;
    }
    val_t *out = val_new_array(tmp, nw, FMT_HEX);
    for (int i = 0; i < nw; i++) val_free(tmp[i]);
    return out;
}

/* ======================================================
 * 色変換 (移植元: ColorFuncs.cs, ColorSpace.cs)
 * ====================================================== */

static int cs_clamp255(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (int)(v + 0.5);
}
static int cs_sat_pack(double r, double g, double b) {
    return (cs_clamp255(r) << 16) | (cs_clamp255(g) << 8) | cs_clamp255(b);
}
static void cs_unpack(int64_t rgb, int *r, int *g, int *b) {
    *r = (int)((rgb >> 16) & 0xff);
    *g = (int)((rgb >> 8)  & 0xff);
    *b = (int)(rgb & 0xff);
}

static double cs_rgb_to_hue(double r, double g, double b, double mn, double mx) {
    if (mn == mx) return 0;
    if (mn == b)  return 60 * (g - r) / (mx - mn) + 60;
    if (mn == r)  return 60 * (b - g) / (mx - mn) + 180;
    return          60 * (r - b) / (mx - mn) + 300;
}

/* HSV <-> RGB */
static int cs_hsv2rgb(double h, double s, double v) {
    h = fmod(h, 360.0);
    s = fmax(0, fmin(100, s)) / 100.0;
    v = fmax(0, fmin(100, v)) / 100.0;
    double f = fmod(h / 60.0, 1.0);
    double x = v * 255, y = v * (1 - s) * 255;
    double z = v * (1 - s * f) * 255, w = v * (1 - s * (1 - f)) * 255;
    double r, g, b;
    if      (s == 0)   { r=x; g=x; b=x; }
    else if (h <  60)  { r=x; g=w; b=y; }
    else if (h < 120)  { r=z; g=x; b=y; }
    else if (h < 180)  { r=y; g=x; b=w; }
    else if (h < 240)  { r=y; g=z; b=x; }
    else if (h < 300)  { r=w; g=y; b=x; }
    else               { r=x; g=y; b=z; }
    return cs_sat_pack(r, g, b);
}
static void cs_rgb2hsv(int ri, int gi, int bi, double *h, double *s, double *v) {
    double r=ri, g=gi, b=bi;
    double mn=fmin(r,fmin(g,b)), mx=fmax(r,fmax(g,b));
    *h = cs_rgb_to_hue(r, g, b, mn, mx);
    *s = (mx == 0) ? 0 : 100*(mx-mn)/mx;
    *v = mx * 100.0 / 255.0;
}

/* HSL <-> RGB */
static int cs_hsl2rgb(double h, double s, double l) {
    h = fmod(h, 360.0);
    s = fmax(0, fmin(100, s)) / 100.0;
    l = fmax(0, fmin(100, l)) / 100.0;
    double f = fmod(h / 60.0, 1.0);
    double chroma = s * (1 - fabs(2*l - 1));
    double mx = 255 * (l + chroma / 2.0);
    double mn = 255 * (l - chroma / 2.0);
    double x = mn + (mx - mn) * f, y = mn + (mx - mn) * (1 - f);
    double r, g, b;
    if      (s == 0)   { r=mx; g=mx; b=mx; }
    else if (h <  60)  { r=mx; g=x;  b=mn; }
    else if (h < 120)  { r=y;  g=mx; b=mn; }
    else if (h < 180)  { r=mn; g=mx; b=x;  }
    else if (h < 240)  { r=mn; g=y;  b=mx; }
    else if (h < 300)  { r=x;  g=mn; b=mx; }
    else               { r=mx; g=mn; b=y;  }
    return cs_sat_pack(r, g, b);
}
static void cs_rgb2hsl(int ri, int gi, int bi, double *h, double *s, double *l) {
    double r=ri, g=gi, b=bi;
    double mn=fmin(r,fmin(g,b)), mx=fmax(r,fmax(g,b));
    *h = cs_rgb_to_hue(r, g, b, mn, mx);
    double p = 255.0 - fabs(mx + mn - 255.0);
    *s = (p == 0) ? 0 : 100*(mx-mn)/p;
    *l = 100*(mx+mn)/(255.0*2.0);
}

/* YUV <-> RGB */
static int cs_rgb2yuv(int ri, int gi, int bi) {
    double r=ri, g=gi, b=bi;
    double y =  0.257*r + 0.504*g + 0.098*b + 16;
    double u = -0.148*r - 0.291*g + 0.439*b + 128;
    double v =  0.439*r - 0.368*g - 0.071*b + 128;
    return cs_sat_pack(y, u, v);
}
static int cs_yuv2rgb(int yi, int ui, int vi) {
    double y=yi-16, u=ui-128, v=vi-128;
    double r = 1.164383*y + 1.596027*v;
    double g = 1.164383*y - 0.391762*u - 0.812968*v;
    double b = 1.164383*y + 2.017232*u;
    return cs_sat_pack(r, g, b);
}

/* RGB565 */
static int cs_rgb888to565(int rgb) {
    int r = (int)fmin(31, (((rgb >> 18) & 0x3f) + 1) >> 1);
    int g = (int)fmin(63, (((rgb >> 9)  & 0x7f) + 1) >> 1);
    int b = (int)fmin(31, (((rgb >> 2)  & 0x3f) + 1) >> 1);
    return ((r & 31) << 11) | ((g & 63) << 5) | (b & 31);
}
static int cs_rgb565to888(int rgb) {
    int r = (rgb >> 11) & 0x1f;
    int g = (rgb >> 5)  & 0x3f;
    int b =  rgb        & 0x1f;
    r = (r << 3) | ((r >> 2) & 7);
    g = (g << 2) | ((g >> 4) & 3);
    b = (b << 3) | ((b >> 2) & 7);
    return cs_sat_pack(r, g, b);
}

static val_t *make_real_arr3(double a, double b, double c) {
    val_t *items[3] = {
        val_new_double(a, FMT_REAL),
        val_new_double(b, FMT_REAL),
        val_new_double(c, FMT_REAL)
    };
    val_t *arr = val_new_array(items, 3, FMT_REAL);
    for (int i = 0; i < 3; i++) val_free(items[i]);
    return arr;
}

static val_t *bi_rgb_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=cs_clamp255(val_as_double(a[0]));
    int g=cs_clamp255(val_as_double(a[1]));
    int b=cs_clamp255(val_as_double(a[2]));
    return val_new_i64((r<<16)|(g<<8)|b, FMT_WEB_COLOR);
}
static val_t *bi_rgb_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_reformat(a[0], FMT_WEB_COLOR);
}
static val_t *bi_hsv2rgb(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_hsv2rgb(val_as_double(a[0]),val_as_double(a[1]),val_as_double(a[2])), FMT_WEB_COLOR);
}
static val_t *bi_rgb2hsv(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    double h,s,v; cs_rgb2hsv(r,g,b,&h,&s,&v);
    return make_real_arr3(h,s,v);
}
static val_t *bi_hsl2rgb(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_hsl2rgb(val_as_double(a[0]),val_as_double(a[1]),val_as_double(a[2])), FMT_WEB_COLOR);
}
static val_t *bi_rgb2hsl(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    double h,s,l; cs_rgb2hsl(r,g,b,&h,&s,&l);
    return make_real_arr3(h,s,l);
}
static val_t *bi_rgb2yuv_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=cs_clamp255(val_as_double(a[0]));
    int g=cs_clamp255(val_as_double(a[1]));
    int b=cs_clamp255(val_as_double(a[2]));
    return val_new_i64(cs_rgb2yuv(r,g,b), FMT_HEX);
}
static val_t *bi_rgb2yuv_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    return val_new_i64(cs_rgb2yuv(r,g,b), FMT_HEX);
}
static val_t *bi_yuv2rgb_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int y=cs_clamp255(val_as_double(a[0]));
    int u=cs_clamp255(val_as_double(a[1]));
    int v=cs_clamp255(val_as_double(a[2]));
    return val_new_i64(cs_yuv2rgb(y,u,v), FMT_WEB_COLOR);
}
static val_t *bi_yuv2rgb_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    return val_new_i64(cs_yuv2rgb(r,g,b), FMT_WEB_COLOR);
}
static val_t *bi_rgbTo565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_rgb888to565((int)val_as_long(a[0])), FMT_HEX);
}
static val_t *bi_rgbFrom565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_rgb565to888((int)val_as_long(a[0])), FMT_WEB_COLOR);
}
static val_t *bi_pack565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=(int)val_as_long(a[0]),g=(int)val_as_long(a[1]),b=(int)val_as_long(a[2]);
    r=r<0?0:(r>31?31:r); g=g<0?0:(g>63?63:g); b=b<0?0:(b>31?31:b);
    return val_new_i64((r<<11)|(g<<5)|b, FMT_HEX);
}
static val_t *bi_unpack565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int rgb=(int)val_as_long(a[0]);
    val_t *items[3] = {
        val_new_i64((rgb>>11)&0x1f, FMT_INT),
        val_new_i64((rgb>>5)&0x3f,  FMT_INT),
        val_new_i64(rgb&0x1f,       FMT_INT)
    };
    val_t *arr = val_new_array(items, 3, FMT_INT);
    for (int i=0;i<3;i++) val_free(items[i]);
    return arr;
}

/* ======================================================
 * パリティ / ECC (移植元: Parity_EccFuncs.cs, LMath.cs)
 * ====================================================== */

static int lm_xor_reduce(int64_t val) {
    val ^= val >> 32; val ^= val >> 16; val ^= val >> 8;
    val ^= val >> 4;  val ^= val >> 2;  val ^= val >> 1;
    return (int)(val & 1);
}
static int lm_odd_parity(int64_t val) { return lm_xor_reduce(val) ^ 1; }

static int lm_ecc_width(int dw) {
    int ew = 0;
    while ((1 << ew) < dw + 1) ew++;
    if (ew + dw >= (1 << ew)) ew++;
    return ew + 1;
}

static const int64_t ECC_XOR_MASK[] = {
    (int64_t)0xab55555556aaad5bLL,
    (int64_t)0xcd9999999b33366dLL,
    (int64_t)0xf1e1e1e1e3c3c78eLL,
    (int64_t)0x01fe01fe03fc07f0LL,
    (int64_t)0x01fffe0003fff800LL,
    (int64_t)0x01fffffffc000000LL,
    (int64_t)0xfe00000000000000LL,
};
static const int ECC_CORR[] = {
     0,-1,-2, 1,-3, 2, 3, 4,-4, 5, 6, 7, 8, 9,10,11,
    -5,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
    -6,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,
    42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,
    -7,58,59,60,61,62,63,64,
};

static int lm_ecc_encode(int dw, int64_t data) {
    int ew = lm_ecc_width(dw);
    int ecc = 0;
    for (int i = 0; i < ew - 1; i++) {
        ecc |= lm_xor_reduce(data & ECC_XOR_MASK[i]) << i;
    }
    ecc |= (lm_odd_parity(ecc) ^ lm_odd_parity(data)) << (ew - 1);
    return ecc;
}

static int lm_ecc_decode(int dw, int ecc, int64_t data) {
    int parity = lm_odd_parity(ecc) ^ lm_odd_parity(data);
    int ew = lm_ecc_width(dw);
    int syndrome = ecc ^ lm_ecc_encode(dw, data);
    syndrome &= (1 << (ew - 1)) - 1;
    int err_pos = ECC_CORR[syndrome];
    if (parity == 0) {
        return (err_pos == 0) ? 0 : -1;
    } else {
        if (err_pos == 0) return dw + ew;
        if (err_pos < 0)  return dw - err_pos;
        return err_pos;
    }
}

static val_t *bi_xorReduce(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_xor_reduce(val_as_long(a[0])), FMT_INT);
}
static val_t *bi_oddParity(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_odd_parity(val_as_long(a[0])), FMT_INT);
}
static val_t *bi_eccWidth(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_width((int)val_as_long(a[0])), FMT_INT);
}
static val_t *bi_eccEnc(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_encode((int)val_as_long(a[0]), val_as_long(a[1])), FMT_HEX);
}
static val_t *bi_eccDec(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_decode((int)val_as_long(a[0]), (int)val_as_long(a[1]), val_as_long(a[2])), FMT_INT);
}

/* ======================================================
 * エンコーディング (移植元: EncodingFuncs.cs)
 * ====================================================== */

/* utf8Enc(str) → byte array */
static val_t *bi_utf8Enc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int len = (int)strlen(s);
    val_t **items = (val_t **)malloc((size_t)(len + 1) * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        items[cnt++] = val_new_i64((uint8_t)s[i], FMT_HEX);
    }
    val_t *arr = val_new_array(items, cnt, FMT_HEX);
    for (int i = 0; i < cnt; i++) val_free(items[i]);
    free(items);
    return arr;
}

/* utf8Dec(bytes[]) → string */
static val_t *bi_utf8Dec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_str("");
    int len = a[0]->arr_len;
    char *buf = (char *)malloc((size_t)(len + 1));
    for (int i = 0; i < len; i++) {
        buf[i] = (char)(uint8_t)(val_as_long(a[0]->arr_items[i]) & 0xFF);
    }
    buf[len] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* urlEnc(str) → percent-encoded string */
static val_t *bi_urlEnc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    char *buf = (char *)malloc((size_t)(slen * 3 + 1));
    int pos = 0;
    for (int i = 0; i < slen; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c=='-' || c=='_' || c=='.' || c=='~') {
            buf[pos++] = (char)c;
        } else {
            pos += snprintf(buf + pos, 4, "%%%02x", c);
        }
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* urlDec(str) → decoded string */
static val_t *bi_urlDec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    char *buf = (char *)malloc((size_t)(slen + 1));
    int pos = 0;
    for (int i = 0; i < slen; ) {
        if (s[i] == '%' && i + 2 < slen && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            char hex[3] = {s[i+1], s[i+2], '\0'};
            buf[pos++] = (char)(int)strtol(hex, NULL, 16);
            i += 3;
        } else if (s[i] == '+') {
            buf[pos++] = ' ';
            i++;
        } else {
            buf[pos++] = s[i++];
        }
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* Base64 テーブル */
static const char B64_ENC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static val_t *base64_enc_bytes_impl(const uint8_t *data, int dlen) {
    int outlen = ((dlen + 2) / 3) * 4;
    char *buf = (char *)malloc((size_t)(outlen + 1));
    int pos = 0;
    for (int i = 0; i < dlen; i += 3) {
        int b0 = data[i];
        int b1 = (i+1 < dlen) ? data[i+1] : 0;
        int b2 = (i+2 < dlen) ? data[i+2] : 0;
        buf[pos++] = B64_ENC[(b0 >> 2) & 0x3F];
        buf[pos++] = B64_ENC[((b0 & 3) << 4) | ((b1 >> 4) & 0xF)];
        buf[pos++] = (i+1 < dlen) ? B64_ENC[((b1 & 0xF) << 2) | ((b2 >> 6) & 3)] : '=';
        buf[pos++] = (i+2 < dlen) ? B64_ENC[b2 & 0x3F] : '=';
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64Enc(str) → string */
static val_t *bi_base64Enc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    return base64_enc_bytes_impl((const uint8_t *)s, (int)strlen(s));
}

/* base64EncBytes(bytes[]) → string */
static val_t *bi_base64EncBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_str("");
    int len = a[0]->arr_len;
    uint8_t *data = (uint8_t *)malloc((size_t)(len + 1));
    for (int i = 0; i < len; i++) data[i] = (uint8_t)(val_as_long(a[0]->arr_items[i]) & 0xFF);
    val_t *v = base64_enc_bytes_impl(data, len);
    free(data);
    return v;
}

/* base64Dec(str) → string */
static val_t *bi_base64Dec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    int outlen = (slen / 4) * 3 + 4;
    uint8_t *buf = (uint8_t *)malloc((size_t)outlen);
    int pos = 0;
    for (int i = 0; i + 3 < slen; i += 4) {
        int c0 = b64_decode_char(s[i]);
        int c1 = b64_decode_char(s[i+1]);
        int c2 = (s[i+2] != '=') ? b64_decode_char(s[i+2]) : 0;
        int c3 = (s[i+3] != '=') ? b64_decode_char(s[i+3]) : 0;
        if (c0 < 0 || c1 < 0) break;
        buf[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        if (s[i+2] != '=') buf[pos++] = (uint8_t)(((c1 & 0xF) << 4) | (c2 >> 2));
        if (s[i+3] != '=') buf[pos++] = (uint8_t)(((c2 & 3) << 6) | c3);
    }
    buf[pos] = '\0';
    val_t *v = val_new_str((char *)buf);
    free(buf);
    return v;
}

/* base64DecBytes(str) → bytes[] */
static val_t *bi_base64DecBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    int outlen = (slen / 4) * 3 + 4;
    uint8_t *buf = (uint8_t *)malloc((size_t)outlen);
    int pos = 0;
    for (int i = 0; i + 3 < slen; i += 4) {
        int c0 = b64_decode_char(s[i]);
        int c1 = b64_decode_char(s[i+1]);
        int c2 = (s[i+2] != '=') ? b64_decode_char(s[i+2]) : 0;
        int c3 = (s[i+3] != '=') ? b64_decode_char(s[i+3]) : 0;
        if (c0 < 0 || c1 < 0) break;
        buf[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        if (s[i+2] != '=') buf[pos++] = (uint8_t)(((c1 & 0xF) << 4) | (c2 >> 2));
        if (s[i+3] != '=') buf[pos++] = (uint8_t)(((c2 & 3) << 6) | c3);
    }
    val_t **items = (val_t **)malloc((size_t)pos * sizeof(val_t *));
    for (int i = 0; i < pos; i++) items[i] = val_new_i64(buf[i], FMT_HEX);
    val_t *arr = val_new_array(items, pos, FMT_HEX);
    for (int i = 0; i < pos; i++) val_free(items[i]);
    free(items); free(buf);
    return arr;
}

/* ======================================================
 * E系列 (移植元: ESeriesFuncs.cs, ESeries.cs, PreferredNumbers.cs)
 * ====================================================== */

/* E系列の double 値を mpdecimal に変換する (%.3g で丸めて精度誤差を除去) */
static val_t *val_from_es(double d, val_fmt_t fmt) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3g", d);
    real_t r; real_from_str(&r, buf);
    return val_new_real(&r, fmt);
}

static const double E3[]  = {1.0, 2.2, 4.7};
static const double E6[]  = {1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
static const double E12[] = {1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
static const double E24[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
                              3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
/* 移植元: Calctus/Model/Standards/Eseries.cs - ESeries.E48/E96/E192 */
static const double E48[] = {
    1.00, 1.05, 1.10, 1.15, 1.21, 1.27, 1.33, 1.40, 1.47, 1.54, 1.62, 1.69,
    1.78, 1.87, 1.96, 2.05, 2.15, 2.26, 2.37, 2.49, 2.61, 2.74, 2.87, 3.01,
    3.16, 3.32, 3.48, 3.65, 3.83, 4.02, 4.22, 4.42, 4.64, 4.87, 5.11, 5.36,
    5.62, 5.90, 6.19, 6.49, 6.81, 7.15, 7.50, 7.87, 8.25, 8.66, 9.09, 9.53};
static const double E96[] = {
    1.00, 1.02, 1.05, 1.07, 1.10, 1.13, 1.15, 1.18, 1.21, 1.24, 1.27, 1.30,
    1.33, 1.37, 1.40, 1.43, 1.47, 1.50, 1.54, 1.58, 1.62, 1.65, 1.69, 1.74,
    1.78, 1.82, 1.87, 1.91, 1.96, 2.00, 2.05, 2.10, 2.15, 2.21, 2.26, 2.32,
    2.37, 2.43, 2.49, 2.55, 2.61, 2.67, 2.74, 2.80, 2.87, 2.94, 3.01, 3.09,
    3.16, 3.24, 3.32, 3.40, 3.48, 3.57, 3.65, 3.74, 3.83, 3.92, 4.02, 4.12,
    4.22, 4.32, 4.42, 4.53, 4.64, 4.75, 4.87, 4.99, 5.11, 5.23, 5.36, 5.49,
    5.62, 5.76, 5.90, 6.04, 6.19, 6.34, 6.49, 6.65, 6.81, 6.98, 7.15, 7.32,
    7.50, 7.68, 7.87, 8.06, 8.25, 8.45, 8.66, 8.87, 9.09, 9.31, 9.53, 9.76};
static const double E192[] = {
    1.00, 1.01, 1.02, 1.04, 1.05, 1.06, 1.07, 1.09, 1.10, 1.11, 1.13, 1.14,
    1.15, 1.17, 1.18, 1.20, 1.21, 1.23, 1.24, 1.26, 1.27, 1.29, 1.30, 1.32,
    1.33, 1.35, 1.37, 1.38, 1.40, 1.42, 1.43, 1.45, 1.47, 1.49, 1.50, 1.52,
    1.54, 1.56, 1.58, 1.60, 1.62, 1.64, 1.65, 1.67, 1.69, 1.72, 1.74, 1.76,
    1.78, 1.80, 1.82, 1.84, 1.87, 1.89, 1.91, 1.93, 1.96, 1.98, 2.00, 2.03,
    2.05, 2.08, 2.10, 2.13, 2.15, 2.18, 2.21, 2.23, 2.26, 2.29, 2.32, 2.34,
    2.37, 2.40, 2.43, 2.46, 2.49, 2.52, 2.55, 2.58, 2.61, 2.64, 2.67, 2.71,
    2.74, 2.77, 2.80, 2.84, 2.87, 2.91, 2.94, 2.98, 3.01, 3.05, 3.09, 3.12,
    3.16, 3.20, 3.24, 3.28, 3.32, 3.36, 3.40, 3.44, 3.48, 3.52, 3.57, 3.61,
    3.65, 3.70, 3.74, 3.79, 3.83, 3.88, 3.92, 3.97, 4.02, 4.07, 4.12, 4.17,
    4.22, 4.27, 4.32, 4.37, 4.42, 4.48, 4.53, 4.59, 4.64, 4.70, 4.75, 4.81,
    4.87, 4.93, 4.99, 5.05, 5.11, 5.17, 5.23, 5.30, 5.36, 5.42, 5.49, 5.56,
    5.62, 5.69, 5.76, 5.83, 5.90, 5.97, 6.04, 6.12, 6.19, 6.26, 6.34, 6.42,
    6.49, 6.57, 6.65, 6.73, 6.81, 6.90, 6.98, 7.06, 7.15, 7.23, 7.32, 7.41,
    7.50, 7.59, 7.68, 7.77, 7.87, 7.96, 8.06, 8.16, 8.25, 8.35, 8.45, 8.56,
    8.66, 8.76, 8.87, 8.98, 9.09, 9.20, 9.31, 9.42, 9.53, 9.65, 9.76, 9.88};

static int es_get_series(int n, const double **out) {
    switch (n) {
        case   3: *out = E3;   return   3;
        case   6: *out = E6;   return   6;
        case  12: *out = E12;  return  12;
        case  24: *out = E24;  return  24;
        case  48: *out = E48;  return  48;
        case  96: *out = E96;  return  96;
        case 192: *out = E192; return 192;
        default:  *out = E24;  return  24;
    }
}

/* series[0..len-1] から key 以下の最大要素のインデックスを返す */
static int es_bsearch(const double *series, int len, double key) {
    int i0 = 0, i1 = len - 1;
    while (i0 < i1) {
        int im = (i0 + i1) / 2 + 1;
        if (key < series[im]) i1 = im - 1;
        else                  i0 = im;
    }
    return i0;
}

/* Shift10: value * 10^exp */
static double es_shift10(double value, int exp) {
    double p = 1.0;
    if (exp >= 0) { for (int i=0; i<exp; i++) p *= 10.0; return value * p; }
    else          { for (int i=0; i<-exp; i++) p *= 10.0; return value / p; }
}

/* FindNearests: series から value に最も近い(floor, ceil)を返す */
static void es_find_nearests(const double *series, int len,
                              double value, double *fl, double *ce) {
    double pexp = floor(log10(value));
    int exp = (int)pexp;
    double key = es_shift10(value, -exp);
    int i = es_bsearch(series, len, key);
    *fl = es_shift10(series[i], exp);
    if (fabs(*fl - value) < 1e-10 * fabs(value)) {
        *ce = *fl;
    } else if (i < len - 1) {
        *ce = es_shift10(series[i + 1], exp);
    } else {
        *ce = es_shift10(series[0], exp + 1);
    }
}

/* FindSplitPair: 分圧比 value に最も近いペアを返す */
static void es_find_split_pair(const double *series, int len,
                                double value, double *out_lo, double *out_hi) {
    if (value >= 1.0) { *out_lo = 1.0; *out_hi = 0.0; return; }
    if (value <= 0.0) { *out_lo = 0.0; *out_hi = 1.0; return; }
    double min_diff = 1e300;
    *out_lo = 0.0; *out_hi = 1.0;
    for (int i = 0; i < len; i++) {
        double lo = series[i];
        double hi_target = lo / value - lo;
        double hi_fl, hi_ce;
        es_find_nearests(series, len, hi_target, &hi_fl, &hi_ce);
        double d_fl = fabs(value - lo / (hi_fl + lo));
        double d_ce = fabs(value - lo / (hi_ce + lo));
        if (d_fl < min_diff) { min_diff = d_fl; *out_lo = lo; *out_hi = hi_fl; }
        if (d_ce < min_diff) { min_diff = d_ce; *out_lo = lo; *out_hi = hi_ce; }
    }
    /* 両方が 1 以上になるように桁合わせ */
    double mn = (*out_lo < *out_hi) ? *out_lo : *out_hi;
    if (mn > 0.0) {
        int exp = (int)floor(log10(mn));
        if (exp < 0) {
            *out_lo = es_shift10(*out_lo, -exp);
            *out_hi = es_shift10(*out_hi, -exp);
        }
    }
}

static val_t *bi_esFloor(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    return val_from_es(fl, a[1]->fmt);
}
static val_t *bi_esCeil(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    return val_from_es(ce, a[1]->fmt);
}
static val_t *bi_esRound(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    double result = (fabs(v - fl) <= fabs(ce - v)) ? fl : ce;
    return val_from_es(result, a[1]->fmt);
}
static val_t *bi_esRatio(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double lo, hi; es_find_split_pair(series, len, v, &lo, &hi);
    val_t *items[2] = {
        val_from_es(lo, a[1]->fmt),
        val_from_es(hi, a[1]->fmt)
    };
    val_t *arr = val_new_array(items, 2, a[1]->fmt);
    val_free(items[0]); val_free(items[1]);
    return arr;
}

/* ======================================================
 * Cast functions (移植元: CastFuncs.cs)
 * ====================================================== */

/* rat(x) / rat(x, max_deno): 実数 → 分数 */
static val_t *bi_rat1(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_FRAC) return val_dup(a[0]);
    frac_t f;
    val_as_frac(&f, a[0]);
    return val_new_frac(&f);
}

static val_t *bi_rat2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int64_t max_deno = val_as_long(a[1]);
    real_t r;
    val_as_real(&r, a[0]);
    /* 連分数で近似 (移植元: RMath.FindFrac) */
    double x = real_to_double(&r);
    int sign = (x < 0) ? -1 : 1;
    x = fabs(x);
    /* xis: 連分数係数リスト */
    double xis[256]; int xis_len = 0;
    int64_t p1 = 1, q1 = 1; /* best so far */
    while (xis_len < 200) {
        double xi = floor(x);
        if (xis_len < 256) xis[xis_len++] = xi;
        /* 後ろから計算して収束分数を求める */
        double n = xi, d = 1.0;
        for (int i = xis_len - 2; i >= 0; i--) {
            double tmp = n;
            n = n * xis[i] + d;
            d = tmp;
            /* 約分 */
            double g = (double)((int64_t)fabs(n) | (int64_t)fabs(d));
            /* シンプルな整数 GCD */
            int64_t a2 = (int64_t)fabs(n), b2 = (int64_t)fabs(d);
            while (b2) { int64_t t = b2; b2 = a2 % b2; a2 = t; } (void)g;
            if (a2 > 1) { n /= a2; d /= a2; }
        }
        if ((int64_t)n > max_deno || (int64_t)d > max_deno) break;
        p1 = (int64_t)n; q1 = (int64_t)d;
        double rem = x - xi;
        if (fabs(rem) < 1e-20) break;
        x = 1.0 / rem;
    }
    p1 *= sign;
    frac_t f;
    real_t rp, rq;
    real_from_i64(&rp, p1);
    real_from_i64(&rq, q1 ? q1 : 1);
    frac_from_n_d(&f, &rp, &rq);
    return val_new_frac(&f);
}

/* real(x): 分数 → 実数 */
static val_t *bi_real_fn(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_REAL) return val_dup(a[0]);
    real_t r;
    val_as_real(&r, a[0]);
    return val_new_real(&r, FMT_REAL);
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
    int           vec_arg_idx; /* >=0: この引数インデックスで配列ブロードキャスト; -1: なし */
} bi_entry_t;

static const bi_entry_t EXTRA_TABLE[] = {
    /* 配列 */
    { "mag",          -1, bi_mag,           -1 },
    { "len",           1, bi_len,           -1 },
    { "range",         2, bi_range2,        -1 },
    { "range",         3, bi_range3,        -1 },
    { "rangeInclusive",2, bi_rangeIncl2,    -1 },
    { "rangeInclusive",3, bi_rangeIncl3,    -1 },
    { "concat",        2, bi_concat,        -1 },
    { "reverseArray",  1, bi_reverseArray,  -1 },
    { "map",           2, bi_map,           -1 },
    { "filter",        2, bi_filter,        -1 },
    { "count",         2, bi_count_fn,      -1 },
    { "sort",          1, bi_sort1,         -1 },
    { "sort",          2, bi_sort2,         -1 },
    { "aggregate",     2, bi_aggregate,     -1 },
    { "extend",        3, bi_extend,        -1 },
    { "indexOf",       2, bi_indexOf_arr,   -1 },
    { "lastIndexOf",   2, bi_lastIndexOf_arr, -1 },
    { "contains",      2, bi_contains_arr,  -1 },
    { "except",        2, bi_except,        -1 },
    { "intersect",     2, bi_intersect,     -1 },
    { "union",         2, bi_union_arr,     -1 },
    { "unique",        1, bi_unique1,       -1 },
    { "unique",        2, bi_unique2,       -1 },
    { "all",           2, bi_all2,          -1 },
    { "any",           2, bi_any2,          -1 },
    /* 統計 */
    { "sum",          -1, bi_sum,           -1 },
    { "ave",          -1, bi_ave,           -1 },
    { "geoMean",      -1, bi_geoMean,       -1 },
    { "harMean",      -1, bi_harMean,       -1 },
    { "invSum",       -1, bi_invSum,        -1 },
    /* 素数: isPrime/prime は第0引数でブロードキャスト */
    { "isPrime",       1, bi_isPrime,        0 },
    { "prime",         1, bi_prime,          0 },
    { "primeFact",     1, bi_primeFact,     -1 },
    /* solve */
    { "solve",         1, bi_solve1,        -1 },
    { "solve",         2, bi_solve2,        -1 },
    { "solve",         3, bi_solve3,        -1 },
    /* 文字列 */
    { "str",           1, bi_str,           -1 },
    { "array",         1, bi_array_str,     -1 },
    { "trim",          1, bi_trim,           0 },
    { "trimStart",     1, bi_trimStart,      0 },
    { "trimEnd",       1, bi_trimEnd,        0 },
    { "replace",       3, bi_replace,       -1 },
    { "toLower",       1, bi_toLower,        0 },
    { "toUpper",       1, bi_toUpper,        0 },
    { "startsWith",    2, bi_startsWith,    -1 },
    { "endsWith",      2, bi_endsWith,      -1 },
    { "split",         2, bi_split,         -1 },
    { "join",          2, bi_join,          -1 },
    /* GrayCode */
    { "toGray",        1, bi_toGray,         0 },
    { "fromGray",      1, bi_fromGray,       0 },
    /* BitByteOps */
    { "count1",        1, bi_count1,         0 },
    { "pack",         -1, bi_pack,          -1 },
    { "reverseBits",   2, bi_reverseBits,   -1 },
    { "reverseBytes",  2, bi_reverseBytes,  -1 },
    { "rotateL",       2, bi_rotateL,       -1 },
    { "rotateL",       3, bi_rotateL,       -1 },
    { "rotateR",       2, bi_rotateR,       -1 },
    { "rotateR",       3, bi_rotateR,       -1 },
    { "swap2",         1, bi_swap2,          0 },
    { "swap4",         1, bi_swap4,          0 },
    { "swap8",         1, bi_swap8,          0 },
    { "swapNib",       1, bi_swapNib,        0 },
    { "unpack",        3, bi_unpack,        -1 },
    { "unpack",        2, bi_unpack,        -1 },
    /* Color */
    { "rgb",           3, bi_rgb_3,          -1 },
    { "rgb",           1, bi_rgb_1,          -1 },
    { "hsv2rgb",       3, bi_hsv2rgb,        -1 },
    { "rgb2hsv",       1, bi_rgb2hsv,        -1 },
    { "hsl2rgb",       3, bi_hsl2rgb,        -1 },
    { "rgb2hsl",       1, bi_rgb2hsl,        -1 },
    { "rgb2yuv",       3, bi_rgb2yuv_3,      -1 },
    { "rgb2yuv",       1, bi_rgb2yuv_1,      -1 },
    { "yuv2rgb",       3, bi_yuv2rgb_3,      -1 },
    { "yuv2rgb",       1, bi_yuv2rgb_1,      -1 },
    { "rgbTo565",      1, bi_rgbTo565,       -1 },
    { "rgbFrom565",    1, bi_rgbFrom565,     -1 },
    { "pack565",       3, bi_pack565,        -1 },
    { "unpack565",     1, bi_unpack565,      -1 },
    /* Parity / ECC */
    { "xorReduce",     1, bi_xorReduce,      -1 },
    { "oddParity",     1, bi_oddParity,      -1 },
    { "eccWidth",      1, bi_eccWidth,       -1 },
    { "eccEnc",        2, bi_eccEnc,         -1 },
    { "eccDec",        3, bi_eccDec,         -1 },
    /* Encoding */
    { "utf8Enc",       1, bi_utf8Enc,        -1 },
    { "utf8Dec",       1, bi_utf8Dec,        -1 },
    { "urlEnc",        1, bi_urlEnc,         -1 },
    { "urlDec",        1, bi_urlDec,         -1 },
    { "base64Enc",     1, bi_base64Enc,      -1 },
    { "base64Dec",     1, bi_base64Dec,      -1 },
    { "base64EncBytes",1, bi_base64EncBytes, -1 },
    { "base64DecBytes",1, bi_base64DecBytes, -1 },
    /* E系列 */
    { "esFloor",       2, bi_esFloor,        1 },
    { "esCeil",        2, bi_esCeil,         1 },
    { "esRound",       2, bi_esRound,        1 },
    { "esRatio",       2, bi_esRatio,        1 },
    /* Cast */
    { "rat",           1, bi_rat1,          -1 },
    { "rat",           2, bi_rat2,          -1 },
    { "real",          1, bi_real_fn,       -1 },
    { NULL, 0, NULL, -1 }
};

static func_def_t *make_extra(const bi_entry_t *e) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    strncpy(fd->name, e->name, sizeof(fd->name) - 1);
    fd->n_params    = e->n_params;
    fd->vec_arg_idx = e->vec_arg_idx;
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
