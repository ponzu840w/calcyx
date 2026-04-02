#ifndef UFIXED113_H
#define UFIXED113_H

#include <stdint.h>
#include <stdbool.h>

/* Q1.112 固定小数点数
 * 5ワード x 28ビット = 140ビット内部表現（うち有効 113ビット）
 * w[0]=LSB (bits 0-27), w[1] (bits 28-55), w[2] (bits 56-83),
 * w[3] (bits 84-111), w[4]=MSB (bit 112, 1ビットのみ使用) */

#define UFIXED113_N_WORDS  5
#define UFIXED113_WORD_BITS 28
#define UFIXED113_MASK     ((uint32_t)((1u << UFIXED113_WORD_BITS) - 1))
#define UFIXED113_NUM_BITS 113

typedef struct {
    uint32_t w[UFIXED113_N_WORDS];
} ufixed113_t;

typedef enum {
    UFIXED113_OK = 0,
    UFIXED113_OVERFLOW,
    UFIXED113_DIVIDE_BY_ZERO,
    UFIXED113_INVALID_ARGUMENT,
} ufixed113_err_t;

extern const ufixed113_t UFIXED113_ZERO;
extern const ufixed113_t UFIXED113_ONE;

/* 比較 */
bool          ufixed113_eq (ufixed113_t a, ufixed113_t b);
int           ufixed113_cmp(ufixed113_t a, ufixed113_t b); /* -1 / 0 / 1 */

/* シフト */
ufixed113_t   ufixed113_ssl(ufixed113_t a, uint32_t carry); /* 1ビット左シフト */
ufixed113_t   ufixed113_ssr(ufixed113_t a, uint32_t carry); /* 1ビット右シフト */
ufixed113_t   ufixed113_lsl(ufixed113_t a, uint32_t n);     /* 論理左シフト */
ufixed113_t   ufixed113_lsr(ufixed113_t a, uint32_t n);     /* 論理右シフト */
ufixed113_t   ufixed113_asr(ufixed113_t a, uint32_t n);     /* 算術右シフト */

/* ビット操作 */
ufixed113_t   ufixed113_not          (ufixed113_t a);
ufixed113_t   ufixed113_truncate_right(ufixed113_t a, uint32_t n);
ufixed113_t   ufixed113_align        (ufixed113_t a, int *shift_out); /* MSB=1 に正規化 */

/* 算術（carry/borrow を返す版） */
ufixed113_t   ufixed113_add(ufixed113_t a, ufixed113_t b, uint32_t *carry_out);
ufixed113_t   ufixed113_sub(ufixed113_t a, ufixed113_t b, uint32_t *borrow_out);
ufixed113_t   ufixed113_mul(ufixed113_t a, ufixed113_t b, uint32_t *carry_out);
ufixed113_err_t ufixed113_div_rem(ufixed113_t a, ufixed113_t b,
                                  ufixed113_t *q_out, ufixed113_t *r_out);

/* 符号反転（2の補数） */
ufixed113_t   ufixed113_neg(ufixed113_t a);

/* 変換 */
ufixed113_t   ufixed113_from_double(double d);     /* 範囲 [0, 2)、範囲外は ZERO */
uint64_t      ufixed113_lower64bits(ufixed113_t a); /* bits 0-63 を取得 */

#endif /* UFIXED113_H */
