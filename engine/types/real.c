#include "real.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

mpd_context_t real_ctx;

void real_ctx_init(void) {
    mpd_defaultcontext(&real_ctx);
    real_ctx.prec  = REAL_PREC;
    real_ctx.round = MPD_ROUND_HALF_EVEN;
    real_ctx.emax  = MPD_MAX_EMAX;
    real_ctx.emin  = MPD_MIN_EMIN;
    real_ctx.clamp = 1;
}

void real_init(real_t *r) {
    memset(r->data, 0, sizeof(r->data));
    r->mpd.flags  = MPD_STATIC | MPD_STATIC_DATA;
    r->mpd.exp    = 0;
    r->mpd.digits = 0;
    r->mpd.len    = 0;
    r->mpd.alloc  = REAL_STATIC_ALLOC;
    r->mpd.data   = r->data;
}

void real_copy(real_t *out, const real_t *src) {
    real_init(out);
    uint32_t status = 0;
    mpd_qcopy(&out->mpd, (mpd_t *)&src->mpd, &status);
    out->mpd.data = out->data;  /* ポインタを自分の data に向け直す */
}

/* --- 変換 --- */

void real_from_i64(real_t *out, int64_t val) {
    real_init(out);
    mpd_set_i64(&out->mpd, val, &real_ctx);
}

void real_from_str(real_t *out, const char *str) {
    real_init(out);
    mpd_set_string(&out->mpd, str, &real_ctx);
}

int real_to_str(const real_t *r, char *buf, size_t buflen) {
    char *s = mpd_to_sci((mpd_t *)&r->mpd, 0);
    if (!s) return -1;
    snprintf(buf, buflen, "%s", s);
    mpd_free(s);
    return 0;
}

double real_to_double(const real_t *r) {
    char *s = mpd_to_sci((mpd_t *)&r->mpd, 0);
    if (!s) return 0.0;
    double v = strtod(s, NULL);
    mpd_free(s);
    return v;
}

int64_t real_to_i64(const real_t *r) {
    return mpd_get_i64((mpd_t *)&r->mpd, &real_ctx);
}

/* --- 比較 --- */

int real_cmp(const real_t *a, const real_t *b) {
    return mpd_cmp((mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

bool real_eq(const real_t *a, const real_t *b) {
    return mpd_cmp((mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx) == 0;
}

bool real_is_zero(const real_t *a) {
    return mpd_iszero((mpd_t *)&a->mpd);
}

/* --- 判定 --- */

bool real_is_integer(const real_t *a) {
    real_t fl;
    real_floor(&fl, a);
    return real_eq(a, &fl);
}

int real_sign(const real_t *a) {
    real_t zero;
    real_from_i64(&zero, 0);
    return real_cmp(a, &zero);
}

/* ユークリッド互除法 (a, b は整数) */
void real_gcd(real_t *out, const real_t *a, const real_t *b) {
    real_t aa, bb, rem;
    real_abs(&aa, a);
    real_abs(&bb, b);
    while (!real_is_zero(&bb)) {
        real_rem(&rem, &aa, &bb);
        real_copy(&aa, &bb);
        real_copy(&bb, &rem);
    }
    real_copy(out, &aa);
}

/* --- 算術 --- */

void real_neg(real_t *out, const real_t *a) {
    real_init(out);
    mpd_minus(&out->mpd, (mpd_t *)&a->mpd, &real_ctx);
}

void real_add(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_add(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

void real_sub(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_sub(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

void real_mul(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_mul(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

void real_div(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_div(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

void real_rem(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_rem(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

void real_abs(real_t *out, const real_t *a) {
    real_init(out);
    mpd_abs(&out->mpd, (mpd_t *)&a->mpd, &real_ctx);
}

void real_floor(real_t *out, const real_t *a) {
    real_init(out);
    mpd_floor(&out->mpd, (mpd_t *)&a->mpd, &real_ctx);
}

void real_divint(real_t *out, const real_t *a, const real_t *b) {
    real_init(out);
    mpd_divint(&out->mpd, (mpd_t *)&a->mpd, (mpd_t *)&b->mpd, &real_ctx);
}

/* 整数べき乗 (正確な反復乗算, 移植元: RMath.PowN) */
void real_pown(real_t *out, const real_t *base, int64_t n) {
    real_t ret, x, tmp;
    real_init(&ret); real_init(&x); real_init(&tmp);
    real_from_i64(&ret, 1);
    real_copy(&x, base);
    bool neg = (n < 0);
    if (neg) n = -n;
    while (n > 0) {
        if (n & 1) {
            real_mul(&tmp, &ret, &x);
            real_copy(&ret, &tmp);
        }
        n >>= 1;
        if (n > 0) {
            real_mul(&tmp, &x, &x);
            real_copy(&x, &tmp);
        }
    }
    if (neg) {
        real_from_i64(&tmp, 1);
        real_div(out, &tmp, &ret);
    } else {
        real_copy(out, &ret);
    }
}

/* E の定数値 (遅延初期化) */
static real_t s_E_const;
static bool   s_E_init = false;
static void ensure_E_const(void) {
    if (!s_E_init) {
        real_from_str(&s_E_const, "2.7182818284590452353602874714");
        s_E_init = true;
    }
}

/* exp(x): RMath.Exp と同じ方式 — s=round(x), t=x-s, E^s * exp_series(t)
 * これにより exp(10) == E^10 (整数べき乗が一致) */
void real_exp(real_t *out, const real_t *a) {
    ensure_E_const();

    /* s = round(a) → 整数 */
    real_t s_real, t_real, exp_t, tmp;
    real_init(&s_real); real_init(&t_real);
    real_init(&exp_t); real_init(&tmp);

    /* s = floor(a + 0.5) */
    real_t half;
    real_init(&half);
    real_from_str(&half, "0.5");
    real_add(&tmp, a, &half);
    real_floor(&s_real, &tmp);

    /* t = a - s */
    real_sub(&t_real, a, &s_real);

    /* exp_series(t) = mpd_exp(t) (|t| ≤ 0.5 なので精度十分) */
    real_init(&exp_t);
    mpd_exp(&exp_t.mpd, &t_real.mpd, &real_ctx);
    exp_t.mpd.data = exp_t.data;  /* data ポインタ修正 */

    /* E^s (整数べき乗) */
    int64_t s_int = real_to_i64(&s_real);
    real_t E_s;
    real_init(&E_s);
    real_pown(&E_s, &s_E_const, s_int);

    /* out = E^s * exp_series(t) */
    real_mul(out, &E_s, &exp_t);
}

void real_ln(real_t *out, const real_t *a) {
    real_init(out);
    mpd_ln(&out->mpd, (mpd_t *)&a->mpd, &real_ctx);
}

/* pow(base, exp): 整数指数なら real_pown を使用 (正確) */
void real_pow(real_t *out, const real_t *base, const real_t *exp) {
    if (real_is_integer(exp)) {
        int64_t n = real_to_i64(exp);
        real_pown(out, base, n);
    } else {
        real_init(out);
        mpd_pow(&out->mpd, (mpd_t *)&base->mpd, (mpd_t *)&exp->mpd, &real_ctx);
    }
}
