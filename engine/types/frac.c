/* 移植元: Calctus/Model/Types/frac.cs
 *         Calctus/Model/Mathematics/RMath.cs */

#include "frac.h"
#include <string.h>

/* --- コピー --- */

void frac_copy(frac_t *out, const frac_t *src) {
    real_copy(&out->nume, &src->nume);
    real_copy(&out->deno, &src->deno);
}

/* --- 変換 --- */

void frac_from_i64(frac_t *out, int64_t val) {
    real_from_i64(&out->nume, val);
    real_from_i64(&out->deno, 1);
}

void frac_to_real(real_t *out, const frac_t *f) {
    real_div(out, &f->nume, &f->deno);
}

/* RMath.FindFrac: 連分数展開で val に最も近い分数を求める */
void frac_find_frac(real_t *n_out, real_t *d_out, const real_t *val) {
    real_t zero, one, max_r;
    real_from_i64(&zero, 0);
    real_from_i64(&one,  1);
    real_from_i64(&max_r, FRAC_FIND_MAX);

    /* x == 0 */
    if (real_is_zero(val)) {
        real_copy(n_out, &zero);
        real_copy(d_out, &one);
        return;
    }

    /* x が整数 */
    if (real_is_integer(val)) {
        real_copy(n_out, val);
        real_copy(d_out, &one);
        return;
    }

    int sign = real_sign(val);
    real_t x;
    real_abs(&x, val);

    /* 連分数係数列 xis[0..count-1] */
#define XIS_MAX 80
    real_t xis[XIS_MAX];
    int count = 0;

    real_t best_n, best_d;
    real_from_i64(&best_n, 1);
    real_from_i64(&best_d, 1);

    real_t threshold;
    real_from_str(&threshold, "1E-20");

    for (;;) {
        if (count >= XIS_MAX) break;

        real_t xi, frac_part, inv;
        real_floor(&xi, &x);
        real_copy(&xis[count++], &xi);

        /* 現在の連分数係数列から収束分数を再構成 */
        real_t n_cf, d_cf;
        real_copy(&n_cf, &xi);
        real_from_i64(&d_cf, 1);
        for (int i = count - 2; i >= 0; i--) {
            real_t tmp, t1, t2, g, nd, dd;
            real_copy(&tmp, &n_cf);
            real_mul(&t1, &n_cf, &xis[i]);
            real_add(&t2, &t1, &d_cf);
            real_copy(&n_cf, &t2);
            real_copy(&d_cf, &tmp);
            /* 約分 */
            real_gcd(&g, &d_cf, &n_cf);
            real_divint(&nd, &n_cf, &g);
            real_divint(&dd, &d_cf, &g);
            real_copy(&n_cf, &nd);
            real_copy(&d_cf, &dd);
        }

        /* 最大値チェック */
        if (real_cmp(&n_cf, &max_r) > 0 || real_cmp(&d_cf, &max_r) > 0) break;

        real_copy(&best_n, &n_cf);
        real_copy(&best_d, &d_cf);

        /* 収束チェック: |best_n/best_d - x| < 1e-20 */
        {
            real_t approx, diff, abs_diff;
            real_div(&approx, &best_n, &best_d);
            real_sub(&diff, &approx, &x);
            real_abs(&abs_diff, &diff);
            if (real_cmp(&abs_diff, &threshold) < 0) break;
        }

        /* |x - xi| < 1e-20 */
        {
            real_t diff, abs_diff;
            real_sub(&diff, &x, &xi);
            real_abs(&abs_diff, &diff);
            if (real_cmp(&abs_diff, &threshold) < 0) break;
        }

        /* x = 1 / (x - xi) */
        real_sub(&frac_part, &x, &xi);
        real_div(&inv, &one, &frac_part);
        real_copy(&x, &inv);
    }
#undef XIS_MAX

    /* 符号を付ける */
    real_t sign_r;
    real_from_i64(&sign_r, sign);
    real_mul(n_out, &sign_r, &best_n);
    real_copy(d_out, &best_d);
}

/* frac(decimal val): FindFrac で分数に変換 */
void frac_from_real(frac_t *out, const real_t *val) {
    frac_find_frac(&out->nume, &out->deno, val);
}

/* frac(decimal n, decimal d): 整数同士なら GCD で約分、そうでなければ FindFrac */
void frac_from_n_d(frac_t *out, const real_t *n, const real_t *d) {
    if (real_is_integer(n) && real_is_integer(d)) {
        int sign = real_sign(n) * real_sign(d);
        real_t an, ad, g, rn, rd, sign_r;
        real_abs(&an, n);
        real_abs(&ad, d);
        real_gcd(&g, &an, &ad);
        real_divint(&rn, &an, &g);
        real_divint(&rd, &ad, &g);
        real_from_i64(&sign_r, sign);
        real_mul(&out->nume, &sign_r, &rn);
        real_copy(&out->deno, &rd);
    } else {
        real_t val;
        real_div(&val, n, d);
        frac_find_frac(&out->nume, &out->deno, &val);
    }
}

/* --- 通分 (RMath.Reduce) --- */

bool frac_reduce(real_t *out_an, real_t *out_bn, real_t *out_deno,
                 const frac_t *a, const frac_t *b) {
    real_t g, deno, an, bn;
    real_gcd(&g, &a->deno, &b->deno);
    real_t t;
    real_mul(&t, &a->deno, &b->deno);
    real_divint(&deno, &t, &g);
    real_mul(&an, &a->nume, &deno);
    real_divint(out_an, &an, &a->deno);
    real_mul(&bn, &b->nume, &deno);
    real_divint(out_bn, &bn, &b->deno);
    real_copy(out_deno, &deno);
    return true;
}

/* --- 算術 --- */

/* frac(-val.Nume, val.Deno) */
void frac_neg(frac_t *out, const frac_t *a) {
    real_t neg_n;
    real_neg(&neg_n, &a->nume);
    /* 整数同士なので frac_from_n_d の GCD パスを直接インライン化 */
    real_t an, ad, g, rn, rd;
    real_abs(&an, &neg_n);
    real_abs(&ad, &a->deno);
    real_gcd(&g, &an, &ad);
    real_divint(&rn, &an, &g);
    real_divint(&rd, &ad, &g);
    int sign = real_sign(&neg_n);
    real_t sign_r;
    real_from_i64(&sign_r, sign);
    real_mul(&out->nume, &sign_r, &rn);
    real_copy(&out->deno, &rd);
}

void frac_add(frac_t *out, const frac_t *a, const frac_t *b) {
    real_t an, bn, deno, sum;
    frac_reduce(&an, &bn, &deno, a, b);
    real_add(&sum, &an, &bn);
    frac_from_n_d(out, &sum, &deno);
}

void frac_sub(frac_t *out, const frac_t *a, const frac_t *b) {
    real_t an, bn, deno, diff;
    frac_reduce(&an, &bn, &deno, a, b);
    real_sub(&diff, &an, &bn);
    frac_from_n_d(out, &diff, &deno);
}

/* a * b = ((a.Nume/g0) * (b.Nume/g1)) / ((a.Deno/g1) * (b.Deno/g0)) */
void frac_mul(frac_t *out, const frac_t *a, const frac_t *b) {
    real_t g0, g1, an, bn, ad, bd, rn, rd;
    real_gcd(&g0, &a->nume, &b->deno);
    real_gcd(&g1, &b->nume, &a->deno);
    real_divint(&an, &a->nume, &g0);
    real_divint(&bn, &b->nume, &g1);
    real_divint(&ad, &a->deno, &g1);
    real_divint(&bd, &b->deno, &g0);
    real_mul(&rn, &an, &bn);
    real_mul(&rd, &ad, &bd);
    frac_from_n_d(out, &rn, &rd);
}

/* a / b = ((a.Nume/gn) * (b.Deno/gd)) / ((a.Deno/gd) * (b.Nume/gn)) */
void frac_div(frac_t *out, const frac_t *a, const frac_t *b) {
    real_t gn, gd, an, bd, ad, bn, rn, rd;
    real_gcd(&gn, &a->nume, &b->nume);
    real_gcd(&gd, &b->deno, &a->deno);
    real_divint(&an, &a->nume, &gn);
    real_divint(&bd, &b->deno, &gd);
    real_divint(&ad, &a->deno, &gd);
    real_divint(&bn, &b->nume, &gn);
    real_mul(&rn, &an, &bd);
    real_mul(&rd, &ad, &bn);
    frac_from_n_d(out, &rn, &rd);
}

/* --- 比較 --- */

/* Reduce して分子同士を比較 */
bool frac_eq(const frac_t *a, const frac_t *b) {
    real_t an, bn, deno;
    if (!frac_reduce(&an, &bn, &deno, a, b)) return false;
    return real_eq(&an, &bn);
}

bool frac_ne(const frac_t *a, const frac_t *b) { return !frac_eq(a, b); }

/* (a - b).Nume の符号で判定 */
bool frac_lt(const frac_t *a, const frac_t *b) {
    frac_t diff;
    frac_sub(&diff, a, b);
    return real_sign(&diff.nume) < 0;
}

bool frac_gt(const frac_t *a, const frac_t *b) { return frac_lt(b, a); }
bool frac_le(const frac_t *a, const frac_t *b) { return !frac_gt(a, b); }
bool frac_ge(const frac_t *a, const frac_t *b) { return !frac_lt(a, b); }
