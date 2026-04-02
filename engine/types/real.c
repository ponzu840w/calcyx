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
