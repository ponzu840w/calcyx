/* 移植元: Calctus/Model/Types/quad.cs */

#include "quad.h"
#include <string.h>

const quad_t QUAD_POS_ZERO = { false, 0, {{ 0,0,0,0,0 }} };
const quad_t QUAD_NEG_ZERO = { true,  0, {{ 0,0,0,0,0 }} };

/* --- 基本述語 --- */

bool quad_is_zero(quad_t q) {
    return (q.exp == 0) && ufixed113_eq(q.coe, UFIXED113_ZERO);
}

bool quad_is_normalized(quad_t q) {
    return q.exp != 0 && q.exp != 0x7fff;
}

/* --- 正規化 ---
 * quad.cs: public static quad Normalize(bool neg, int exp, ufixed113 coe) */
quad_t quad_normalize(bool neg, int exp, ufixed113_t coe) {
    if (exp >= 0x7fff)
        return (quad_t){ neg, 0x7ffe, UFIXED113_ONE };

    if (ufixed113_eq(coe, UFIXED113_ZERO))
        return (quad_t){ neg, 0, UFIXED113_ZERO };

    int shift;
    coe = ufixed113_align(coe, &shift);
    exp -= shift;

    if (exp <= 0) {
        coe = ufixed113_lsl(coe, (uint32_t)(-exp));
        exp = 0;
    }

    return (quad_t){ neg, (uint16_t)exp, coe };
}

/* --- 変換 --- */

/* quad.cs: public static explicit operator quad(decimal d) */
quad_t quad_from_real(const real_t *d) {
    real_t zero, one, two, abs_d, di, df, mod, tmp;
    real_from_i64(&zero, 0);
    real_from_i64(&one,  1);
    real_from_i64(&two,  2);

    bool neg = (real_cmp(d, &zero) < 0);
    real_abs(&abs_d, d);
    real_floor(&di, &abs_d);
    real_sub(&df, &abs_d, &di);

    /* 整数部 */
    quad_t qi;
    if (!real_is_zero(&di)) {
        uint16_t ei = QUAD_EXP_BIAS - 1;
        ufixed113_t ci = UFIXED113_ZERO;
        while (!real_is_zero(&di)) {
            real_rem(&mod, &di, &two);
            uint32_t bit = (uint32_t)real_to_i64(&mod);
            ci = ufixed113_ssr(ci, bit);
            ei++;
            real_divint(&tmp, &di, &two); real_copy(&di, &tmp);
        }
        qi = (quad_t){ neg, ei, ci };
    } else {
        qi = neg ? QUAD_NEG_ZERO : QUAD_POS_ZERO;
    }

    /* 小数部 */
    quad_t qf;
    if (!real_is_zero(&df)) {
        uint16_t ef = (uint16_t)(QUAD_EXP_BIAS + UFIXED113_NUM_BITS - 1);
        ufixed113_t cf = UFIXED113_ZERO;
        while (cf.w[UFIXED113_N_WORDS - 1] == 0) {
            real_t df2; real_mul(&df2, &df, &two); real_copy(&df, &df2);
            real_floor(&tmp, &df);
            uint32_t bit = (uint32_t)real_to_i64(&tmp);
            cf = ufixed113_ssl(cf, bit);
            ef--;
            real_t df3; real_sub(&df3, &df, &tmp); real_copy(&df, &df3);
        }
        qf = (quad_t){ neg, ef, cf };
    } else {
        qf = neg ? QUAD_NEG_ZERO : QUAD_POS_ZERO;
    }

    return quad_add(qi, qf);
}

/* quad.cs: public static explicit operator ulong(quad a) */
uint64_t quad_to_ulong(quad_t a) {
    if (quad_is_zero(a) || a.exp == 0) return 0;
    ufixed113_t coe = a.coe;
    int shifts = ((int)QUAD_EXP_BIAS - (int)a.exp) + UFIXED113_NUM_BITS - 1;
    if (shifts > 0)
        coe = ufixed113_lsr(coe, (uint32_t)shifts);
    return ufixed113_lower64bits(coe);
}

/* quad.cs: public quad Truncate() */
quad_t quad_truncate(quad_t q) {
    if (q.exp == 0)
        return (quad_t){ q.neg, 0, UFIXED113_ZERO };
    int trunc_bits = ((int)QUAD_EXP_BIAS - (int)q.exp) + UFIXED113_NUM_BITS - 1;
    ufixed113_t coe = q.coe;
    if (trunc_bits > 0)
        coe = ufixed113_truncate_right(coe, (uint32_t)trunc_bits);
    return quad_normalize(q.neg, q.exp, coe);
}

/* quad.cs: public static explicit operator decimal(quad q) */
void quad_to_real(real_t *out, quad_t q) {
    real_t zero, one, two, sign, di, df, p, fp, sum;
    real_from_i64(&zero, 0);
    real_from_i64(&one,  1);
    real_from_i64(&two,  2);

    if (quad_is_zero(q)) { real_copy(out, &zero); return; }

    real_from_i64(&sign, q.neg ? -1 : 1);
    if (q.neg) q = quad_neg(q);

    quad_t qi = quad_truncate(q);
    quad_t qf = quad_sub(q, qi);

    /* 整数部 */
    quad_t q2 = { false, (uint16_t)(QUAD_EXP_BIAS + 1), UFIXED113_ONE }; /* 2.0 */
    real_copy(&di, &zero);
    real_copy(&p,  &one);
    while (quad_gt(qi, QUAD_POS_ZERO)) {
        if ((quad_to_ulong(qi) & 1) == 1) {
            real_t tmp; real_add(&tmp, &di, &p); real_copy(&di, &tmp);
        }
        qi = quad_truncate(quad_div(qi, q2));
        real_t tmp; real_mul(&tmp, &p, &two); real_copy(&p, &tmp);
    }

    /* 小数部 */
    quad_t q1 = { false, (uint16_t)QUAD_EXP_BIAS, UFIXED113_ONE }; /* 1.0 */
    real_copy(&df, &zero);
    real_copy(&fp, &one);
    while (!quad_is_zero(qf)) {
        qf = quad_mul(qf, q2);
        real_t tmp; real_div(&tmp, &fp, &two); real_copy(&fp, &tmp);
        if (quad_ge(qf, q1)) {
            real_t tmp2; real_add(&tmp2, &df, &fp); real_copy(&df, &tmp2);
            qf = quad_sub(qf, q1);
        }
    }

    real_add(&sum, &di, &df);
    real_mul(out, &sign, &sum);
}

/* --- 算術 --- */

quad_t quad_neg(quad_t a) { return (quad_t){ !a.neg, a.exp, a.coe }; }

/* quad.cs: public static quad operator +(quad a, quad b) */
quad_t quad_add(quad_t a, quad_t b) {
    if (quad_is_zero(a)) return b;
    if (quad_is_zero(b)) return a;

    ufixed113_t a_coe = a.coe;
    ufixed113_t b_coe = b.coe;
    uint16_t a_exp = (a.exp < 1) ? 1 : a.exp;
    uint16_t b_exp = (b.exp < 1) ? 1 : b.exp;
    int q_exp = (a_exp > b_exp) ? a_exp : b_exp;

    if (a_exp > b_exp)
        b_coe = ufixed113_lsr(b_coe, a_exp - b_exp);
    else if (a_exp < b_exp)
        a_coe = ufixed113_lsr(a_coe, b_exp - a_exp);

    bool q_neg;
    ufixed113_t q_coe;

    if (a.neg == b.neg) {
        q_neg = a.neg;
        uint32_t carry;
        q_coe = ufixed113_add(a_coe, b_coe, &carry);
        if (carry == 1) {
            q_coe = ufixed113_ssr(q_coe, 1);
            q_exp++;
        }
    } else {
        if (ufixed113_cmp(a_coe, b_coe) < 0) {
            q_neg = b.neg;
            uint32_t borrow;
            q_coe = ufixed113_sub(b_coe, a_coe, &borrow);
        } else {
            q_neg = a.neg;
            uint32_t borrow;
            q_coe = ufixed113_sub(a_coe, b_coe, &borrow);
        }
    }

    return quad_normalize(q_neg, q_exp, q_coe);
}

quad_t quad_sub(quad_t a, quad_t b) { return quad_add(a, quad_neg(b)); }

quad_t quad_inc(quad_t a) {
    quad_t one = { false, (uint16_t)QUAD_EXP_BIAS, UFIXED113_ONE };
    return quad_add(a, one);
}

quad_t quad_dec(quad_t a) {
    quad_t one = { false, (uint16_t)QUAD_EXP_BIAS, UFIXED113_ONE };
    return quad_sub(a, one);
}

/* quad.cs: public static quad operator *(quad a, quad b) */
quad_t quad_mul(quad_t a, quad_t b) {
    bool neg = a.neg ^ b.neg;
    int  exp = (int)a.exp + (int)b.exp - (int)QUAD_EXP_BIAS;
    uint32_t carry;
    ufixed113_t coe = ufixed113_mul(a.coe, b.coe, &carry);
    if (carry == 1) { coe = ufixed113_ssr(coe, 1); exp++; }
    return quad_normalize(neg, exp, coe);
}

/* quad.cs: public static quad operator /(quad a, quad b) */
quad_t quad_div(quad_t a, quad_t b) {
    bool neg = a.neg ^ b.neg;
    int  exp = (int)a.exp - (int)b.exp + (int)QUAD_EXP_BIAS;
    ufixed113_t q, r;
    ufixed113_div_rem(a.coe, b.coe, &q, &r);
    return quad_normalize(neg, exp, q);
}

/* --- 比較 --- */

bool quad_eq(quad_t a, quad_t b) {
    if (quad_is_zero(a)) return quad_is_zero(b);
    if (quad_is_zero(b)) return quad_is_zero(a);
    return (a.neg == b.neg) && (a.exp == b.exp) && ufixed113_eq(a.coe, b.coe);
}

bool quad_ne(quad_t a, quad_t b) { return !quad_eq(a, b); }

bool quad_gt(quad_t a, quad_t b) {
    if (quad_is_zero(a) && quad_is_zero(b)) return false;
    if ( a.neg && !b.neg) return false;
    if (!a.neg &&  b.neg) return true;
    if (a.exp > b.exp) return !a.neg;
    if (a.exp < b.exp) return  a.neg;
    if (ufixed113_cmp(a.coe, b.coe) > 0) return !a.neg;
    return false;
}

bool quad_lt(quad_t a, quad_t b) { return quad_gt(b, a); }
bool quad_ge(quad_t a, quad_t b) { return quad_gt(a, b) || quad_eq(a, b); }
bool quad_le(quad_t a, quad_t b) { return quad_lt(a, b) || quad_eq(a, b); }
