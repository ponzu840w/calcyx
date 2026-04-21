/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

/* --- mag(x...) — Euclidean norm (移植元: Absolute_SignFuncs.cs) --- */

val_t *bi_mag(val_t **a, int n, void *ctx) {
    (void)ctx;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        double v = val_as_double(a[i]);
        sum += v * v;
    }
    return val_new_double(sqrt(sum), a[0]->fmt);
}

/* --- len(array_or_str) (移植元: ArrayFuncs.cs) --- */

val_t *bi_len(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_ARRAY)
        return val_new_i64(a[0]->arr_len, FMT_INT);
    if (a[0]->type == VAL_STR)
        return val_new_i64((int64_t)strlen(a[0]->str_v), FMT_INT);
    return val_new_i64(1, FMT_INT);
}

/* --- range / rangeInclusive (移植元: ArrayFuncs.cs, RMath.Range) --- */

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

val_t *bi_range2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], NULL, false);
}
val_t *bi_range3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], a[2], false);
}
val_t *bi_rangeIncl2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], NULL, true);
}
val_t *bi_rangeIncl3(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return make_range_real(a[0], a[1], a[2], true);
}

/* --- 配列操作 (移植元: ArrayFuncs.cs) --- */

val_t *bi_concat(val_t **a, int n, void *ctx) {
    (void)n;
    eval_ctx_t *ec = (eval_ctx_t *)ctx;
    int na = a[0]->type == VAL_ARRAY ? a[0]->arr_len : 0;
    int nb = a[1]->type == VAL_ARRAY ? a[1]->arr_len : 0;
    if (na + nb > ec->settings.max_array_length) {
        EVAL_ERROR(ec, 0, "Array length exceeds limit (%d).", ec->settings.max_array_length);
        return NULL;
    }
    val_t **items = (val_t **)malloc((size_t)(na + nb) * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < na; i++) items[i]    = val_dup(a[0]->arr_items[i]);
    for (int i = 0; i < nb; i++) items[na+i] = val_dup(a[1]->arr_items[i]);
    val_t *out = val_new_array(items, na + nb, a[0]->fmt);
    for (int i = 0; i < na + nb; i++) val_free(items[i]);
    free(items);
    return out;
}

val_t *bi_reverseArray(val_t **a, int n, void *ctx) {
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

val_t *bi_map(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return NULL;
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < len; i++) {
        items[i] = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (!items[i]) items[i] = val_new_null();
    }
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

val_t *bi_filter(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return NULL;
    int len = a[0]->arr_len;
    val_t **tmp = (val_t **)malloc((size_t)len * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        val_t *r = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (r && val_as_bool(r)) tmp[cnt++] = val_dup(a[0]->arr_items[i]);
        val_free(r);
    }
    val_t *out = val_new_array(tmp, cnt, a[0]->fmt);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    free(tmp);
    return out;
}

val_t *bi_count_fn(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return NULL;
    int cnt = 0;
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        if (r && val_as_bool(r)) cnt++;
        val_free(r);
    }
    return val_new_i64(cnt, FMT_INT);
}

/* sort: コンテキスト付き比較とインサーションソート
 * qsort のグローバル変数経由コンテキスト渡しを廃止し、
 * 計算機の配列サイズ（通常 ≪1000）では O(n²) で十分。 */
typedef struct { eval_ctx_t *ctx; func_def_t *key_fd; } sort_ctx_t;

static int sort_compare(const val_t *a, const val_t *b, const sort_ctx_t *sc) {
    double da, db;
    if (sc->key_fd) {
        val_t *ka = bia_call_fd_1(sc->key_fd, (val_t *)a, sc->ctx);
        val_t *kb = bia_call_fd_1(sc->key_fd, (val_t *)b, sc->ctx);
        da = ka ? val_as_double(ka) : 0;
        db = kb ? val_as_double(kb) : 0;
        val_free(ka); val_free(kb);
    } else {
        da = val_as_double(a);
        db = val_as_double(b);
    }
    return (da > db) - (da < db);
}

static void sort_items(val_t **items, int len, const sort_ctx_t *sc) {
    for (int i = 1; i < len; i++) {
        val_t *key = items[i];
        int j = i - 1;
        while (j >= 0 && sort_compare(items[j], key, sc) > 0) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
}

val_t *bi_sort1(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < len; i++) items[i] = val_dup(a[0]->arr_items[i]);
    sort_ctx_t sc = { (eval_ctx_t *)ctx, NULL };
    sort_items(items, len, &sc);
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

val_t *bi_sort2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    func_def_t *fd = bia_get_fd(a[1]);
    int len = a[0]->arr_len;
    val_t **items = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!items) return NULL;
    for (int i = 0; i < len; i++) items[i] = val_dup(a[0]->arr_items[i]);
    sort_ctx_t sc = { (eval_ctx_t *)ctx, fd };
    sort_items(items, len, &sc);
    val_t *out = val_new_array(items, len, a[0]->fmt);
    for (int i = 0; i < len; i++) val_free(items[i]);
    free(items);
    return out;
}

val_t *bi_aggregate(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY || a[0]->arr_len == 0) return NULL;
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return NULL;
    val_t *acc = val_dup(a[0]->arr_items[0]);
    for (int i = 1; i < a[0]->arr_len; i++) {
        val_t *r = bia_call_fd_2(fd, acc, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        val_free(acc);
        acc = r ? r : val_new_null();
    }
    return acc;
}

val_t *bi_extend(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return NULL;
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return NULL;
    int cnt = val_as_int(a[2]);
    int seed_len = a[0]->arr_len;
    val_t **list = (val_t **)malloc((size_t)(seed_len + cnt) * sizeof(val_t *));
    for (int i = 0; i < seed_len; i++) list[i] = val_dup(a[0]->arr_items[i]);
    int total = seed_len;
    for (int i = 0; i < cnt; i++) {
        /* call fd with current array */
        val_t *arr_so_far = val_new_array(list, total, a[0]->fmt);
        val_t *r = bia_call_fd_1(fd, arr_so_far, (eval_ctx_t *)ctx);
        val_free(arr_so_far);
        list[total++] = r ? r : val_new_null();
    }
    val_t *out = val_new_array(list, total, a[0]->fmt);
    for (int i = 0; i < total; i++) val_free(list[i]);
    free(list);
    return out;
}

val_t *bi_indexOf_arr(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* indexOf(str, str) */
    if (a[0]->type == VAL_STR) {
        char needle[512] = "";
        if (a[1]->type == VAL_STR) {
            bia_str_copy(needle, a[1]->str_v, sizeof(needle));
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
        func_def_t *fd = bia_get_fd(a[1]);
        for (int i = 0; i < a[0]->arr_len; i++) {
            val_t *r = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
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

val_t *bi_lastIndexOf_arr(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_STR) {
        char needle[512] = "";
        if (a[1]->type == VAL_STR) bia_str_copy(needle, a[1]->str_v, sizeof(needle));
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

val_t *bi_contains_arr(val_t **a, int n, void *ctx) {
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

val_t *bi_except(val_t **a, int n, void *ctx) {
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

val_t *bi_intersect(val_t **a, int n, void *ctx) {
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

val_t *bi_union_arr(val_t **a, int n, void *ctx) {
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

val_t *bi_unique1(val_t **a, int n, void *ctx) {
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
val_t *bi_unique2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_dup(a[0]);
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return val_dup(a[0]);
    int len = a[0]->arr_len;
    val_t **tmp = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!tmp) return NULL;
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        bool dup = false;
        for (int j = 0; j < cnt; j++) {
            val_t *eq = bia_call_fd_2(fd, tmp[j], a[0]->arr_items[i], (eval_ctx_t *)ctx);
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
val_t *bi_all2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(false);
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return val_new_bool(false);
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        bool ok = r && val_as_bool(r);
        val_free(r);
        if (!ok) return val_new_bool(false);
    }
    return val_new_bool(true);
}

val_t *bi_any2(val_t **a, int n, void *ctx) {
    (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_bool(false);
    func_def_t *fd = bia_get_fd(a[1]);
    if (!fd) return val_new_bool(false);
    for (int i = 0; i < a[0]->arr_len; i++) {
        val_t *r = bia_call_fd_1(fd, a[0]->arr_items[i], (eval_ctx_t *)ctx);
        bool ok = r && val_as_bool(r);
        val_free(r);
        if (ok) return val_new_bool(true);
    }
    return val_new_bool(false);
}

