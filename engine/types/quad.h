#ifndef QUAD_H
#define QUAD_H

#include "ufixed113.h"
#include "real.h"
#include <stdint.h>
#include <stdbool.h>

/* 128ビット浮動小数点数 (IEEE 754 quad 相当)
 * neg: 符号
 * exp: 指数（バイアス = 0x3fff）
 * coe: 仮数（ufixed113, Q1.112）
 *
 * 移植元: Calctus/Model/Types/quad.cs */

#define QUAD_EXP_BIAS 0x3fff

typedef struct {
    bool        neg;
    uint16_t    exp;
    ufixed113_t coe;
} cx_quad_t;

extern const cx_quad_t QUAD_POS_ZERO;
extern const cx_quad_t QUAD_NEG_ZERO;

bool    quad_is_zero      (cx_quad_t q);
bool    quad_is_normalized(cx_quad_t q);
cx_quad_t  quad_normalize    (bool neg, int exp, ufixed113_t coe);

/* 変換 */
cx_quad_t   quad_from_real(const real_t *d);
void     quad_to_real  (real_t *out, cx_quad_t q);
uint64_t quad_to_ulong (cx_quad_t a);
cx_quad_t   quad_truncate (cx_quad_t q);

/* 算術 */
cx_quad_t  quad_neg(cx_quad_t a);
cx_quad_t  quad_add(cx_quad_t a, cx_quad_t b);
cx_quad_t  quad_sub(cx_quad_t a, cx_quad_t b);
cx_quad_t  quad_mul(cx_quad_t a, cx_quad_t b);
cx_quad_t  quad_div(cx_quad_t a, cx_quad_t b);
cx_quad_t  quad_inc(cx_quad_t a);
cx_quad_t  quad_dec(cx_quad_t a);

/* 比較 */
bool    quad_eq(cx_quad_t a, cx_quad_t b);
bool    quad_ne(cx_quad_t a, cx_quad_t b);
bool    quad_gt(cx_quad_t a, cx_quad_t b);
bool    quad_lt(cx_quad_t a, cx_quad_t b);
bool    quad_ge(cx_quad_t a, cx_quad_t b);
bool    quad_le(cx_quad_t a, cx_quad_t b);

/* 数学関数 */
cx_quad_t  quad_log2(cx_quad_t a);

#endif /* QUAD_H */
