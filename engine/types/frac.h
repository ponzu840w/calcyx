/* 移植元: Calctus/Model/Types/frac.cs
 *         Calctus/Model/Mathematics/RMath.cs (Gcd, Reduce, FindFrac) */

#ifndef FRAC_H
#define FRAC_H

#include "real.h"
#include <stdbool.h>

/* 有理数型
 * real_t を持つため値渡し不可。必ずポインタを使うこと。
 * Deno は常に正、符号は Nume に持たせる。 */

typedef struct {
    real_t nume;
    real_t deno;
} frac_t;

/* 初期化後コピー (real_t の自己参照ポインタを修正) */
void frac_copy(frac_t *out, const frac_t *src);

/* 変換 */
void frac_from_real  (frac_t *out, const real_t *val);
void frac_from_n_d   (frac_t *out, const real_t *n, const real_t *d);
void frac_from_i64   (frac_t *out, int64_t val);
void frac_to_real    (real_t *out, const frac_t *f);

/* 連分数展開によって val に最も近い分数を求める
 * 分子・分母ともに FindFracMaxDeno 以下に収める */
#define FRAC_FIND_MAX 1000000000000LL  /* 10^12 */
void frac_find_frac(real_t *n_out, real_t *d_out, const real_t *val);

/* 通分 (frac_add/sub の内部処理)
 * out_an, out_bn, out_deno に結果を書く。失敗時は false を返す。 */
bool frac_reduce(real_t *out_an, real_t *out_bn, real_t *out_deno,
                 const frac_t *a, const frac_t *b);

/* 算術 */
void frac_neg(frac_t *out, const frac_t *a);
void frac_add(frac_t *out, const frac_t *a, const frac_t *b);
void frac_sub(frac_t *out, const frac_t *a, const frac_t *b);
void frac_mul(frac_t *out, const frac_t *a, const frac_t *b);
void frac_div(frac_t *out, const frac_t *a, const frac_t *b);

/* 比較 */
bool frac_eq(const frac_t *a, const frac_t *b);
bool frac_ne(const frac_t *a, const frac_t *b);
bool frac_lt(const frac_t *a, const frac_t *b);
bool frac_gt(const frac_t *a, const frac_t *b);
bool frac_le(const frac_t *a, const frac_t *b);
bool frac_ge(const frac_t *a, const frac_t *b);

#endif /* FRAC_H */
