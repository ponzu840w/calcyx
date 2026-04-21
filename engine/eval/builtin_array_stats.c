/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

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

val_t *bi_sum(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    double s = 0; for (int i = 0; i < cnt; i++) s += buf[i];
    return val_new_double(s, a[0]->fmt);
}
val_t *bi_ave(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double s = 0; for (int i = 0; i < cnt; i++) s += buf[i];
    return val_new_double(s / cnt, a[0]->fmt);
}
val_t *bi_geoMean(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double p = 1; for (int i = 0; i < cnt; i++) p *= buf[i];
    return val_new_double(pow(p, 1.0/cnt), a[0]->fmt);
}
val_t *bi_harMean(val_t **a, int n, void *ctx) {
    (void)ctx;
    double buf[STAT_BUF];
    int cnt = flatten_args(a, n, buf, STAT_BUF);
    if (cnt == 0) return val_new_double(0, FMT_REAL);
    double s = 0; for (int i = 0; i < cnt; i++) s += 1.0/buf[i];
    return val_new_double(cnt / s, a[0]->fmt);
}
val_t *bi_invSum(val_t **a, int n, void *ctx) {
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

val_t *bi_isPrime(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    return val_new_bool(is_prime_i64(val_as_long(a[0])));
}

val_t *bi_prime(val_t **a, int n, void *ctx) {
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
val_t *bi_primeFact(val_t **a, int n, void *ctx) {
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
    val_t *r  = bia_call_fd_1(fd, xv, ctx);
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
val_t *bi_solve1(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = bia_get_fd(a[0]);
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

val_t *bi_solve2(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = bia_get_fd(a[0]);
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

val_t *bi_solve3(val_t **a, int n, void *ctx) {
    (void)n;
    func_def_t *fd = bia_get_fd(a[0]);
    if (!fd) return NULL;
    double xmin = val_as_double(a[1]);
    double xmax = val_as_double(a[2]);
    return solve_impl(fd, xmin, xmax, (eval_ctx_t *)ctx);
}

