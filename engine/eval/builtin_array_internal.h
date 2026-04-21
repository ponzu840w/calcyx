/* builtin_array_internal.h
 * builtin_array.c をカテゴリ別 (ops/stats/string/bits/color/encode) に
 * 分割する際の内部ヘッダ。共通ヘルパー (bia_*) と各 bi_* の extern
 * 宣言をまとめる。本ヘッダは engine 内部専用で、外部公開しない。 */

#ifndef CALCYX_BUILTIN_ARRAY_INTERNAL_H
#define CALCYX_BUILTIN_ARRAY_INTERNAL_H

#include "builtin.h"
#include "eval_ctx.h"
#include "eval.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

/* ---- 共通ヘルパー (定義は builtin_array.c) ---- */
void         bia_str_copy(char *dst, const char *src, size_t size);
void         bia_bind_param(eval_ctx_t *child, const char *pname, val_t *val);
val_t       *bia_call_fd_1(func_def_t *fd, val_t *arg, eval_ctx_t *ctx);
val_t       *bia_call_fd_2(func_def_t *fd, val_t *a0, val_t *a1, eval_ctx_t *ctx);
func_def_t  *bia_get_fd(val_t *v);

/* ---- 各カテゴリの bi_* 関数 (EXTRA_TABLE から参照される) ---- */

/* ops */
val_t *bi_mag(val_t **a, int n, void *ctx);
val_t *bi_len(val_t **a, int n, void *ctx);
val_t *bi_range2(val_t **a, int n, void *ctx);
val_t *bi_range3(val_t **a, int n, void *ctx);
val_t *bi_rangeIncl2(val_t **a, int n, void *ctx);
val_t *bi_rangeIncl3(val_t **a, int n, void *ctx);
val_t *bi_concat(val_t **a, int n, void *ctx);
val_t *bi_reverseArray(val_t **a, int n, void *ctx);
val_t *bi_map(val_t **a, int n, void *ctx);
val_t *bi_filter(val_t **a, int n, void *ctx);
val_t *bi_count_fn(val_t **a, int n, void *ctx);
val_t *bi_sort1(val_t **a, int n, void *ctx);
val_t *bi_sort2(val_t **a, int n, void *ctx);
val_t *bi_aggregate(val_t **a, int n, void *ctx);
val_t *bi_extend(val_t **a, int n, void *ctx);
val_t *bi_indexOf_arr(val_t **a, int n, void *ctx);
val_t *bi_lastIndexOf_arr(val_t **a, int n, void *ctx);
val_t *bi_contains_arr(val_t **a, int n, void *ctx);
val_t *bi_except(val_t **a, int n, void *ctx);
val_t *bi_intersect(val_t **a, int n, void *ctx);
val_t *bi_union_arr(val_t **a, int n, void *ctx);
val_t *bi_unique1(val_t **a, int n, void *ctx);
val_t *bi_unique2(val_t **a, int n, void *ctx);
val_t *bi_all2(val_t **a, int n, void *ctx);
val_t *bi_any2(val_t **a, int n, void *ctx);

/* stats / primes / solve */
val_t *bi_sum(val_t **a, int n, void *ctx);
val_t *bi_ave(val_t **a, int n, void *ctx);
val_t *bi_geoMean(val_t **a, int n, void *ctx);
val_t *bi_harMean(val_t **a, int n, void *ctx);
val_t *bi_invSum(val_t **a, int n, void *ctx);
val_t *bi_isPrime(val_t **a, int n, void *ctx);
val_t *bi_prime(val_t **a, int n, void *ctx);
val_t *bi_primeFact(val_t **a, int n, void *ctx);
val_t *bi_solve1(val_t **a, int n, void *ctx);
val_t *bi_solve2(val_t **a, int n, void *ctx);
val_t *bi_solve3(val_t **a, int n, void *ctx);

/* string + GrayCode */
val_t *bi_str(val_t **a, int n, void *ctx);
val_t *bi_array_str(val_t **a, int n, void *ctx);
val_t *bi_trim(val_t **a, int n, void *ctx);
val_t *bi_trimStart(val_t **a, int n, void *ctx);
val_t *bi_trimEnd(val_t **a, int n, void *ctx);
val_t *bi_toLower(val_t **a, int n, void *ctx);
val_t *bi_toUpper(val_t **a, int n, void *ctx);
val_t *bi_replace(val_t **a, int n, void *ctx);
val_t *bi_startsWith(val_t **a, int n, void *ctx);
val_t *bi_endsWith(val_t **a, int n, void *ctx);
val_t *bi_split(val_t **a, int n, void *ctx);
val_t *bi_join(val_t **a, int n, void *ctx);
val_t *bi_toGray(val_t **a, int n, void *ctx);
val_t *bi_fromGray(val_t **a, int n, void *ctx);

/* BitByteOps */
val_t *bi_count1(val_t **a, int n, void *ctx);
val_t *bi_pack(val_t **a, int n, void *ctx);
val_t *bi_reverseBits(val_t **a, int n, void *ctx);
val_t *bi_reverseBytes(val_t **a, int n, void *ctx);
val_t *bi_rotateL(val_t **a, int n, void *ctx);
val_t *bi_rotateR(val_t **a, int n, void *ctx);
val_t *bi_swap2(val_t **a, int n, void *ctx);
val_t *bi_swap4(val_t **a, int n, void *ctx);
val_t *bi_swap8(val_t **a, int n, void *ctx);
val_t *bi_swapNib(val_t **a, int n, void *ctx);
val_t *bi_unpack(val_t **a, int n, void *ctx);

/* Color + Parity/ECC */
val_t *bi_rgb_3(val_t **a, int n, void *ctx);
val_t *bi_rgb_1(val_t **a, int n, void *ctx);
val_t *bi_hsv2rgb(val_t **a, int n, void *ctx);
val_t *bi_rgb2hsv(val_t **a, int n, void *ctx);
val_t *bi_hsl2rgb(val_t **a, int n, void *ctx);
val_t *bi_rgb2hsl(val_t **a, int n, void *ctx);
val_t *bi_rgb2yuv_3(val_t **a, int n, void *ctx);
val_t *bi_rgb2yuv_1(val_t **a, int n, void *ctx);
val_t *bi_yuv2rgb_3(val_t **a, int n, void *ctx);
val_t *bi_yuv2rgb_1(val_t **a, int n, void *ctx);
val_t *bi_rgbTo565(val_t **a, int n, void *ctx);
val_t *bi_rgbFrom565(val_t **a, int n, void *ctx);
val_t *bi_pack565(val_t **a, int n, void *ctx);
val_t *bi_unpack565(val_t **a, int n, void *ctx);
val_t *bi_xorReduce(val_t **a, int n, void *ctx);
val_t *bi_oddParity(val_t **a, int n, void *ctx);
val_t *bi_eccWidth(val_t **a, int n, void *ctx);
val_t *bi_eccEnc(val_t **a, int n, void *ctx);
val_t *bi_eccDec(val_t **a, int n, void *ctx);

/* Encoding + E series + Cast */
val_t *bi_utf8Enc(val_t **a, int n, void *ctx);
val_t *bi_utf8Dec(val_t **a, int n, void *ctx);
val_t *bi_urlEnc(val_t **a, int n, void *ctx);
val_t *bi_urlDec(val_t **a, int n, void *ctx);
val_t *bi_base64Enc(val_t **a, int n, void *ctx);
val_t *bi_base64EncBytes(val_t **a, int n, void *ctx);
val_t *bi_base64Dec(val_t **a, int n, void *ctx);
val_t *bi_base64DecBytes(val_t **a, int n, void *ctx);
val_t *bi_esFloor(val_t **a, int n, void *ctx);
val_t *bi_esCeil(val_t **a, int n, void *ctx);
val_t *bi_esRound(val_t **a, int n, void *ctx);
val_t *bi_esRatio(val_t **a, int n, void *ctx);
val_t *bi_rat1(val_t **a, int n, void *ctx);
val_t *bi_rat2(val_t **a, int n, void *ctx);
val_t *bi_real_fn(val_t **a, int n, void *ctx);

#endif /* CALCYX_BUILTIN_ARRAY_INTERNAL_H */
