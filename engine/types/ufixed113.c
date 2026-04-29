#include "ufixed113.h"
#include <math.h>

#define MSB_IDX (UFIXED113_N_WORDS - 1)

const ufixed113_t UFIXED113_ZERO = {{ 0, 0, 0, 0, 0 }};
const ufixed113_t UFIXED113_ONE  = {{ 0, 0, 0, 0, 1 }};
static const ufixed113_t ONE_LSB = {{ 1, 0, 0, 0, 0 }};

/* --- 比較 --- */

bool ufixed113_eq(ufixed113_t a, ufixed113_t b) {
    for (int i = 0; i < UFIXED113_N_WORDS; i++) {
        if (a.w[i] != b.w[i]) return false;
    }
    return true;
}

int ufixed113_cmp(ufixed113_t a, ufixed113_t b) {
    for (int i = MSB_IDX; i >= 0; i--) {
        if (a.w[i] > b.w[i]) return  1;
        if (a.w[i] < b.w[i]) return -1;
    }
    return 0;
}

/* --- シフト --- */

ufixed113_t ufixed113_ssl(ufixed113_t a, uint32_t carry) {
    uint32_t next = carry;
    for (int i = 0; i < UFIXED113_N_WORDS; i++) {
        if (i == MSB_IDX) {
            a.w[i] = next & 1;
        } else {
            uint32_t out = (a.w[i] >> (UFIXED113_WORD_BITS - 1)) & 1;
            a.w[i] = ((a.w[i] << 1) & UFIXED113_MASK) | next;
            next = out;
        }
    }
    return a;
}

ufixed113_t ufixed113_ssr(ufixed113_t a, uint32_t carry) {
    uint32_t prev = carry;
    for (int i = MSB_IDX; i >= 0; i--) {
        uint32_t lsb = a.w[i] & 1;
        if (i == MSB_IDX) {
            a.w[i] = prev;
        } else {
            a.w[i] = (a.w[i] >> 1) | (prev << (UFIXED113_WORD_BITS - 1));
        }
        prev = lsb;
    }
    return a;
}

/* word-level シフト (1bit ずつ ssl/ssr を呼ぶ実装より高速).
 * n を word 数 + bit 数に分け、word 単位の bulk move + bit 単位の合成で計算する. */

ufixed113_t ufixed113_lsl(ufixed113_t a, uint32_t n) {
    if (n == 0) return a;
    if (n >= UFIXED113_NUM_BITS) return UFIXED113_ZERO;

    uint32_t ws = n / UFIXED113_WORD_BITS;
    uint32_t bs = n % UFIXED113_WORD_BITS;

    ufixed113_t r = UFIXED113_ZERO;

    if (bs == 0) {
        for (int i = (int)ws; i < UFIXED113_N_WORDS; i++)
            r.w[i] = a.w[i - (int)ws];
    } else {
        uint32_t bsc = UFIXED113_WORD_BITS - bs;
        for (int i = (int)ws; i < UFIXED113_N_WORDS; i++) {
            int hi_idx = i - (int)ws;
            int lo_idx = hi_idx - 1;
            uint32_t hi = a.w[hi_idx];
            uint32_t lo = (lo_idx >= 0) ? a.w[lo_idx] : 0;
            r.w[i] = ((hi << bs) | (lo >> bsc)) & UFIXED113_MASK;
        }
    }
    r.w[MSB_IDX] &= 1;
    return r;
}

ufixed113_t ufixed113_lsr(ufixed113_t a, uint32_t n) {
    if (n == 0) return a;
    if (n >= UFIXED113_NUM_BITS) return UFIXED113_ZERO;

    uint32_t ws = n / UFIXED113_WORD_BITS;
    uint32_t bs = n % UFIXED113_WORD_BITS;

    ufixed113_t r = UFIXED113_ZERO;

    if (bs == 0) {
        for (int i = 0; i + (int)ws < UFIXED113_N_WORDS; i++)
            r.w[i] = a.w[i + (int)ws];
    } else {
        uint32_t bsc = UFIXED113_WORD_BITS - bs;
        for (int i = 0; i + (int)ws < UFIXED113_N_WORDS; i++) {
            int lo_idx = i + (int)ws;
            int hi_idx = lo_idx + 1;
            uint32_t lo = a.w[lo_idx];
            uint32_t hi = (hi_idx < UFIXED113_N_WORDS) ? a.w[hi_idx] : 0;
            r.w[i] = ((lo >> bs) | (hi << bsc)) & UFIXED113_MASK;
        }
    }
    r.w[MSB_IDX] &= 1;
    return r;
}

ufixed113_t ufixed113_asr(ufixed113_t a, uint32_t n) {
    if (n == 0) return a;
    uint32_t sign = a.w[MSB_IDX] & 1;
    if (n >= UFIXED113_NUM_BITS)
        return sign ? ufixed113_not(UFIXED113_ZERO) : UFIXED113_ZERO;

    uint32_t ws = n / UFIXED113_WORD_BITS;
    uint32_t bs = n % UFIXED113_WORD_BITS;
    uint32_t fill = sign ? UFIXED113_MASK : 0;

    /* MSB ワードは bit 0 のみ有効. 上位 27bit を sign 拡張して一様に扱う. */
    a.w[MSB_IDX] = fill;

    ufixed113_t r;

    if (bs == 0) {
        for (int i = 0; i < UFIXED113_N_WORDS; i++) {
            int src = i + (int)ws;
            r.w[i] = (src < UFIXED113_N_WORDS) ? a.w[src] : fill;
        }
    } else {
        uint32_t bsc = UFIXED113_WORD_BITS - bs;
        for (int i = 0; i < UFIXED113_N_WORDS; i++) {
            int lo_idx = i + (int)ws;
            int hi_idx = lo_idx + 1;
            uint32_t lo = (lo_idx < UFIXED113_N_WORDS) ? a.w[lo_idx] : fill;
            uint32_t hi = (hi_idx < UFIXED113_N_WORDS) ? a.w[hi_idx] : fill;
            r.w[i] = ((lo >> bs) | (hi << bsc)) & UFIXED113_MASK;
        }
    }
    r.w[MSB_IDX] = sign;
    return r;
}

/* --- ビット操作 --- */

ufixed113_t ufixed113_not(ufixed113_t a) {
    for (int i = 0; i < UFIXED113_N_WORDS; i++)
        a.w[i] ^= UFIXED113_MASK;
    a.w[MSB_IDX] &= 1;
    return a;
}

ufixed113_t ufixed113_truncate_right(ufixed113_t a, uint32_t n) {
    if (n == 0) return a;
    if (n >= UFIXED113_NUM_BITS) return UFIXED113_ZERO;

    int idx = 0;
    while (n >= UFIXED113_WORD_BITS && idx < MSB_IDX) {
        a.w[idx++] = 0;
        n -= UFIXED113_WORD_BITS;
    }
    if (n > 0 && idx < UFIXED113_N_WORDS) {
        if (idx == MSB_IDX) {
            a.w[idx] = 0;
        } else {
            uint32_t mask = (1u << n) - 1;
            a.w[idx] &= ~mask;
        }
    }
    return a;
}

ufixed113_t ufixed113_align(ufixed113_t a, int *shift_out) {
    *shift_out = 0;
    if (ufixed113_eq(a, UFIXED113_ZERO)) return a;
    while (a.w[MSB_IDX] == 0 && *shift_out < (int)UFIXED113_NUM_BITS) {
        a = ufixed113_ssl(a, 0);
        (*shift_out)++;
        if (ufixed113_eq(a, UFIXED113_ZERO)) break;
    }
    return a;
}

/* --- 算術 --- */

ufixed113_t ufixed113_add(ufixed113_t a, ufixed113_t b, uint32_t *carry_out) {
    uint32_t carry = 0;
    for (int i = 0; i < UFIXED113_N_WORDS; i++) {
        uint64_t s = (uint64_t)a.w[i] + b.w[i] + carry;
        if (i == MSB_IDX) {
            a.w[i] = (uint32_t)s;
        } else {
            a.w[i] = (uint32_t)(s & UFIXED113_MASK);
            carry  = (uint32_t)(s >> UFIXED113_WORD_BITS);
        }
    }
    *carry_out = (a.w[MSB_IDX] >> 1) & 1;
    a.w[MSB_IDX] &= 1;
    return a;
}

ufixed113_t ufixed113_neg(ufixed113_t a) {
    uint32_t carry;
    return ufixed113_add(ufixed113_not(a), ONE_LSB, &carry);
}

ufixed113_t ufixed113_sub(ufixed113_t a, ufixed113_t b, uint32_t *borrow_out) {
    *borrow_out = (ufixed113_cmp(a, b) < 0) ? 1 : 0;
    uint32_t carry;
    return ufixed113_add(a, ufixed113_neg(b), &carry);
}

ufixed113_t ufixed113_mul(ufixed113_t a, ufixed113_t b, uint32_t *carry_out) {
    uint64_t acc[2 * UFIXED113_N_WORDS] = {0};

    for (int i = 0; i < UFIXED113_N_WORDS; i++) {
        uint32_t vi = (i == MSB_IDX) ? (b.w[i] & 1) : b.w[i];
        if (vi == 0) continue;
        for (int j = 0; j < UFIXED113_N_WORDS; j++) {
            uint32_t vj = (j == MSB_IDX) ? (a.w[j] & 1) : a.w[j];
            if (vj == 0) continue;
            acc[i + j] += (uint64_t)vi * vj;
        }
    }

    uint64_t c = 0;
    for (int k = 0; k < 2 * UFIXED113_N_WORDS; k++) {
        acc[k] += c;
        c = acc[k] >> UFIXED113_WORD_BITS;
        acc[k] &= UFIXED113_MASK;
    }

    ufixed113_t r;
    for (int i = 0; i < UFIXED113_N_WORDS; i++)
        r.w[i] = (uint32_t)acc[UFIXED113_N_WORDS - 1 + i];

    *carry_out = (r.w[MSB_IDX] >> 1) & 1;
    r.w[MSB_IDX] &= 1;
    return r;
}

uint64_t ufixed113_lower64bits(ufixed113_t a) {
    return (uint64_t)a.w[0]
         | ((uint64_t)a.w[1] << 28)
         | ((uint64_t)a.w[2] << 56);
}

ufixed113_t ufixed113_from_double(double d) {
    if (d < 0.0 || d >= 2.0) return UFIXED113_ZERO;
    ufixed113_t r = UFIXED113_ZERO;
    for (int i = 0; i < UFIXED113_NUM_BITS; i++) {
        uint32_t bit = (uint32_t)floor(d);
        r = ufixed113_ssl(r, bit);
        d -= bit;
        d *= 2.0;
    }
    return r;
}

ufixed113_err_t ufixed113_div_rem(ufixed113_t a, ufixed113_t b,
                                   ufixed113_t *q_out, ufixed113_t *r_out) {
    if (ufixed113_eq(b, UFIXED113_ZERO)) return UFIXED113_DIVIDE_BY_ZERO;
    if (ufixed113_eq(a, UFIXED113_ZERO)) {
        *q_out = *r_out = UFIXED113_ZERO;
        return UFIXED113_OK;
    }
    if (ufixed113_cmp(a, b) < 0) {
        *q_out = UFIXED113_ZERO;
        *r_out = a;
        return UFIXED113_OK;
    }

    int da, db;
    ufixed113_t a_al = ufixed113_align(a, &da);
    ufixed113_t b_al = ufixed113_align(b, &db);

    ufixed113_t q   = UFIXED113_ZERO;
    ufixed113_t cur = a_al;

    for (int i = 0; i < UFIXED113_NUM_BITS; i++) {
        uint32_t bit = (ufixed113_cmp(cur, b_al) >= 0) ? 1 : 0;
        if (bit) {
            uint32_t borrow;
            cur = ufixed113_sub(cur, b_al, &borrow);
        }
        q   = ufixed113_ssl(q,   bit);
        cur = ufixed113_ssl(cur, 0);
    }

    int scale = da - db;
    q = (scale >= 0) ? ufixed113_lsr(q, (uint32_t) scale)
                     : ufixed113_lsl(q, (uint32_t)-scale);

    uint32_t carry;
    ufixed113_t qb = ufixed113_mul(q, b, &carry);
    uint32_t borrow;
    ufixed113_t r = ufixed113_sub(a, qb, &borrow);

    *q_out = q;
    *r_out = r;
    return UFIXED113_OK;
}
