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
} quad_t;

extern const quad_t QUAD_POS_ZERO;
extern const quad_t QUAD_NEG_ZERO;

bool    quad_is_zero      (quad_t q);
bool    quad_is_normalized(quad_t q);
quad_t  quad_normalize    (bool neg, int exp, ufixed113_t coe);

/* 変換 */
quad_t   quad_from_real(const real_t *d);
void     quad_to_real  (real_t *out, quad_t q);
uint64_t quad_to_ulong (quad_t a);
quad_t   quad_truncate (quad_t q);

/* 算術 */
quad_t  quad_neg(quad_t a);
quad_t  quad_add(quad_t a, quad_t b);
quad_t  quad_sub(quad_t a, quad_t b);
quad_t  quad_mul(quad_t a, quad_t b);
quad_t  quad_div(quad_t a, quad_t b);
quad_t  quad_inc(quad_t a);
quad_t  quad_dec(quad_t a);

/* 比較 */
bool    quad_eq(quad_t a, quad_t b);
bool    quad_ne(quad_t a, quad_t b);
bool    quad_gt(quad_t a, quad_t b);
bool    quad_lt(quad_t a, quad_t b);
bool    quad_ge(quad_t a, quad_t b);
bool    quad_le(quad_t a, quad_t b);

#endif /* QUAD_H */
