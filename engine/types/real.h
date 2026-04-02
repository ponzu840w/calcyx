#ifndef REAL_H
#define REAL_H

#include <mpdecimal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* .NET decimal 相当の 10進浮動小数点数 (精度 29桁)
 *
 * 注意: real_t は mpd_t.data が embedded 配列を指す自己参照構造体のため、
 * 値渡しコピーは禁止。必ず real_t* を使うこと。
 * 関数はすべて output-pointer スタイル: 第1引数が出力先 real_t* */

#define REAL_PREC        29
#define REAL_STATIC_ALLOC 4

typedef struct {
    mpd_t      mpd;
    mpd_uint_t data[REAL_STATIC_ALLOC];
} real_t;

extern mpd_context_t real_ctx;

/* プログラム起動時に一度だけ呼ぶ */
void real_ctx_init(void);

/* 初期化（使用前に必ず呼ぶ） */
void real_init(real_t *r);

/* 変換 (out に結果を書く) */
void real_from_i64(real_t *out, int64_t val);
void real_from_str(real_t *out, const char *str);
void real_copy    (real_t *out, const real_t *src);

/* 文字列・数値への変換 */
int     real_to_str   (const real_t *r, char *buf, size_t buflen);
double  real_to_double(const real_t *r);
int64_t real_to_i64   (const real_t *r);

/* 比較 */
int  real_cmp(const real_t *a, const real_t *b);
bool real_eq (const real_t *a, const real_t *b);
bool real_is_zero(const real_t *a);

/* 算術 (out に結果を書く) */
void real_neg   (real_t *out, const real_t *a);
void real_add   (real_t *out, const real_t *a, const real_t *b);
void real_sub   (real_t *out, const real_t *a, const real_t *b);
void real_mul   (real_t *out, const real_t *a, const real_t *b);
void real_div   (real_t *out, const real_t *a, const real_t *b);
void real_rem   (real_t *out, const real_t *a, const real_t *b);
void real_abs   (real_t *out, const real_t *a);
void real_floor (real_t *out, const real_t *a);
void real_divint(real_t *out, const real_t *a, const real_t *b);

#endif /* REAL_H */
