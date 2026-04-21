/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

/* ======================================================
 * BitByteOps (移植元: BitByteOpsFuncs.cs)
 * ====================================================== */

val_t *bi_count1(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    int cnt = 0;
    while (x) { cnt += x & 1; x >>= 1; }
    return val_new_i64(cnt, FMT_INT);
}

/* pack(width, v0, v1, ...): フィールドを LSB から詰める */
val_t *bi_pack(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n < 1) return val_new_i64(0, FMT_INT);
    /* 第1引数がスカラーの場合: pack(width, v0, v1, ...) */
    if (a[0]->type != VAL_ARRAY) {
        int w = val_as_int(a[0]);
        uint64_t result = 0;
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        for (int i = 1; i < n; i++) {
            result |= ((uint64_t)val_as_long(a[i]) & mask) << ((i-1)*w);
        }
        return val_new_i64((int64_t)result, FMT_HEX);
    }
    /* 第1引数が配列の場合: pack([w0,w1,...], v0, v1, ...) */
    int nw = a[0]->arr_len;
    uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < nw && i+1 < n; i++) {
        int w = val_as_int(a[0]->arr_items[i]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        result |= ((uint64_t)val_as_long(a[i+1]) & mask) << shift;
        shift += w;
    }
    return val_new_i64((int64_t)result, FMT_HEX);
}

/* reverseBits(width, v): v の下位 width ビットを反転 */
val_t *bi_reverseBits(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int w = val_as_int(a[0]);
    uint64_t v = (uint64_t)val_as_long(a[1]);
    uint64_t out = 0;
    for (int i = 0; i < w; i++)
        out |= ((v >> i) & 1) << (w - 1 - i);
    /* 上位ビットは入力の上位ビットを保持 */
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    out = (v & ~mask) | (out & mask);
    return val_new_i64((int64_t)out, a[1]->fmt);
}

/* reverseBytes(nbytes, v): v の下位 nbytes バイトを反転 */
val_t *bi_reverseBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int nb = val_as_int(a[0]);
    uint64_t v = (uint64_t)val_as_long(a[1]);
    uint8_t bytes[8];
    for (int i = 0; i < nb; i++) bytes[i] = (uint8_t)(v >> (i * 8));
    uint64_t out = v;
    for (int i = 0; i < nb; i++) {
        out &= ~(0xFFULL << (i * 8));
        out |= (uint64_t)bytes[nb - 1 - i] << (i * 8);
    }
    return val_new_i64((int64_t)out, a[1]->fmt);
}

/* rotateL(width, v) / rotateL(width, n, v): left rotate */
val_t *bi_rotateL(val_t **a, int n, void *ctx) {
    (void)ctx;
    int w, amt;
    uint64_t v;
    if (n == 2) { w = val_as_int(a[0]); amt = 1; v = (uint64_t)val_as_long(a[1]); }
    else        { w = val_as_int(a[0]); amt = val_as_int(a[1]); v = (uint64_t)val_as_long(a[2]); }
    if (w <= 0 || w > 64) return val_new_i64((int64_t)v, a[n-1]->fmt);
    amt = ((amt % w) + w) % w;
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    uint64_t bits = v & mask;
    bits = ((bits << amt) | (bits >> (w - amt))) & mask;
    uint64_t out = (v & ~mask) | bits;
    return val_new_i64((int64_t)out, a[n-1]->fmt);
}

val_t *bi_rotateR(val_t **a, int n, void *ctx) {
    (void)ctx;
    int w, amt;
    uint64_t v;
    if (n == 2) { w = val_as_int(a[0]); amt = 1; v = (uint64_t)val_as_long(a[1]); }
    else        { w = val_as_int(a[0]); amt = val_as_int(a[1]); v = (uint64_t)val_as_long(a[2]); }
    if (w <= 0 || w > 64) return val_new_i64((int64_t)v, a[n-1]->fmt);
    amt = ((amt % w) + w) % w;
    uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
    uint64_t bits = v & mask;
    bits = ((bits >> amt) | (bits << (w - amt))) & mask;
    uint64_t out = (v & ~mask) | bits;
    return val_new_i64((int64_t)out, a[n-1]->fmt);
}

/* swapN: swap adjacent N-byte groups */
static uint64_t swap_bytes_n(uint64_t v, int n) {
    uint64_t out = 0;
    for (int i = 0; i < 8; i += 2*n) {
        for (int j = 0; j < n; j++) {
            int from = i + j;
            int to   = i + 2*n - 1 - j;
            if (from < 8) out |= ((v >> (from*8)) & 0xFFULL) << (to*8);
            if (to   < 8) out |= ((v >> (to*8))   & 0xFFULL) << (from*8);
        }
    }
    return out;
}
val_t *bi_swap2(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),1), a[0]->fmt); }
val_t *bi_swap4(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),2), a[0]->fmt); }
val_t *bi_swap8(val_t **a, int n, void *ctx) { (void)ctx;(void)n; return val_new_i64((int64_t)swap_bytes_n((uint64_t)val_as_long(a[0]),4), a[0]->fmt); }

/* swapNib: swap adjacent nibbles */
val_t *bi_swapNib(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t v = (uint64_t)val_as_long(a[0]);
    uint64_t out = ((v & 0xF0F0F0F0F0F0F0F0ULL) >> 4) | ((v & 0x0F0F0F0F0F0F0F0FULL) << 4);
    return val_new_i64((int64_t)out, a[0]->fmt);
}

/* unpack: フィールドを解凍 */
val_t *bi_unpack(val_t **a, int n, void *ctx) {
    (void)ctx;
    if (n < 2) return val_new_null();
    /* unpack(width, count, v): v を width ビットずつ count 個 */
    if (a[0]->type != VAL_ARRAY) {
        int w     = val_as_int(a[0]);
        int cnt   = val_as_int(a[1]);
        uint64_t v = (uint64_t)val_as_long(a[2]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        val_t *tmp[256];
        if (cnt > 256) cnt = 256;
        for (int i = 0; i < cnt; i++) {
            tmp[i] = val_new_i64((int64_t)((v >> (i*w)) & mask), FMT_INT);
        }
        val_t *out = val_new_array(tmp, cnt, FMT_INT);
        for (int i = 0; i < cnt; i++) val_free(tmp[i]);
        return out;
    }
    /* unpack([w0,w1,...], v): 可変長フィールド */
    int nw = a[0]->arr_len;
    uint64_t v = (uint64_t)val_as_long(a[1]);
    val_t *tmp[256];
    if (nw > 256) nw = 256;
    int shift = 0;
    for (int i = 0; i < nw; i++) {
        int w = val_as_int(a[0]->arr_items[i]);
        uint64_t mask = (w < 64) ? ((1ULL << w) - 1) : ~0ULL;
        tmp[i] = val_new_i64((int64_t)((v >> shift) & mask), FMT_HEX);
        shift += w;
    }
    val_t *out = val_new_array(tmp, nw, FMT_HEX);
    for (int i = 0; i < nw; i++) val_free(tmp[i]);
    return out;
}

