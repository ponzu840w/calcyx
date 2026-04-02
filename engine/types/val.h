/* 移植元: Calctus/Model/Types/Val.cs (+ RealVal.cs, FracVal.cs, BoolVal.cs,
 *          StrVal.cs, NullVal.cs, ArrayVal.cs) */

#ifndef VAL_H
#define VAL_H

#include "real.h"
#include "frac.h"
#include <stdbool.h>
#include <stdint.h>

/* ======================================================
 * フォーマット種別 (FormatHint / NumberFormatter に相当)
 * ====================================================== */

typedef enum {
    FMT_REAL         = 0,  /* 通常の実数表示 (Weak) */
    FMT_INT          = 1,  /* 10進整数表示   (Weak) */
    FMT_SI_PREFIX    = 2,  /* SI接頭語 1k, 1M  (Strong) */
    FMT_BIN_PREFIX   = 3,  /* 2進接頭語 1Ki    (Strong) */
    FMT_HEX          = 4,  /* 16進 0x...        (AlwaysLeft) */
    FMT_OCT          = 5,  /* 8進  0...          (AlwaysLeft) */
    FMT_BIN          = 6,  /* 2進  0b...         (AlwaysLeft) */
    FMT_CHAR         = 7,  /* 文字 'a'           (AlwaysLeft) */
    FMT_STRING       = 8,  /* 文字列 "..."       (AlwaysLeft) */
    FMT_DATETIME     = 9,  /* 日時              (AlwaysLeft) */
    FMT_WEB_COLOR    = 10, /* 色 #rrggbb        (AlwaysLeft) */
} val_fmt_t;

/* FormatHint.Select(b): 2項演算の結果フォーマットを決定 */
val_fmt_t val_fmt_select(val_fmt_t a, val_fmt_t b);

/* ======================================================
 * 値型
 * ====================================================== */

typedef enum {
    VAL_REAL  = 0,
    VAL_FRAC  = 1,
    VAL_BOOL  = 2,
    VAL_STR   = 3,
    VAL_NULL  = 4,
    VAL_ARRAY = 5,
} val_type_t;

/* val_t は常にヒープ確保。値渡し禁止。
 * 所有権: 生成した関数が所有し、呼び出し側に所有権を移す。
 * 不要になったら val_free() で解放すること。 */
typedef struct val_s val_t;
struct val_s {
    val_type_t type;
    val_fmt_t  fmt;

    /* type に応じていずれか一つを使用 */
    real_t   real_v;      /* VAL_REAL */
    frac_t   frac_v;      /* VAL_FRAC */
    bool     bool_v;      /* VAL_BOOL */
    char    *str_v;       /* VAL_STR:  malloc 済み NUL 終端文字列、所有 */
    val_t  **arr_items;   /* VAL_ARRAY: malloc 済み val_t* 配列、所有 */
    int      arr_len;     /* VAL_ARRAY: 要素数 */
};

/* --- 生成 --- */
val_t *val_new_real   (const real_t *r, val_fmt_t fmt);
val_t *val_new_frac   (const frac_t *f);
val_t *val_new_bool   (bool b);
val_t *val_new_str    (const char *s);
val_t *val_new_null   (void);
val_t *val_new_i64    (int64_t v, val_fmt_t fmt);
val_t *val_new_double (double v, val_fmt_t fmt);
val_t *val_new_array  (val_t **items, int len, val_fmt_t fmt);

/* --- コピー / 解放 --- */
val_t *val_dup  (const val_t *src);
void   val_free (val_t *v);

/* --- フォーマット変換 --- */
val_t *val_reformat(const val_t *v, val_fmt_t fmt);

/* --- 型変換 --- */
/* AsReal: VAL_REAL → そのまま, VAL_FRAC → 実数に, その他 → 失敗 */
void val_as_real  (real_t *out, const val_t *v);
void val_as_frac  (frac_t *out, const val_t *v);
bool val_as_bool  (const val_t *v);
int64_t  val_as_long  (const val_t *v);
int      val_as_int   (const val_t *v);
double   val_as_double(const val_t *v);

/* UpConvert: 精度を右辺に合わせる (REAL→FRAC など) */
val_t *val_upconvert(const val_t *a, const val_t *b);

/* --- 単項演算 --- */
val_t *val_neg     (const val_t *a);
val_t *val_bit_not (const val_t *a);

/* --- 算術演算 --- */
val_t *val_add  (const val_t *a, const val_t *b);
val_t *val_sub  (const val_t *a, const val_t *b);
val_t *val_mul  (const val_t *a, const val_t *b);
val_t *val_div  (const val_t *a, const val_t *b);
val_t *val_idiv (const val_t *a, const val_t *b);  /* 整数除算 */
val_t *val_mod  (const val_t *a, const val_t *b);

/* --- ビット演算 --- */
val_t *val_bit_and (const val_t *a, const val_t *b);
val_t *val_bit_xor (const val_t *a, const val_t *b);
val_t *val_bit_or  (const val_t *a, const val_t *b);

/* --- シフト演算 --- */
val_t *val_lsl (const val_t *a, const val_t *b);  /* 論理左シフト  << */
val_t *val_lsr (const val_t *a, const val_t *b);  /* 論理右シフト  >> (unsigned) */
val_t *val_asl (const val_t *a, const val_t *b);  /* 算術左シフト  <<< */
val_t *val_asr (const val_t *a, const val_t *b);  /* 算術右シフト  >>> */

/* --- 比較演算 (VAL_BOOL を返す) --- */
val_t *val_eq  (const val_t *a, const val_t *b);
val_t *val_ne  (const val_t *a, const val_t *b);
val_t *val_gt  (const val_t *a, const val_t *b);
val_t *val_lt  (const val_t *a, const val_t *b);
val_t *val_ge  (const val_t *a, const val_t *b);
val_t *val_le  (const val_t *a, const val_t *b);

/* --- 論理演算 (VAL_BOOL 同士) --- */
val_t *val_logic_and (const val_t *a, const val_t *b);
val_t *val_logic_or  (const val_t *a, const val_t *b);
val_t *val_logic_not (const val_t *a);

/* --- 文字列変換 (デバッグ・表示用) --- */
/* buf に NUL 終端文字列を書く。足りなければ切り詰める。 */
void val_to_str(const val_t *v, char *buf, size_t buflen);

#endif /* VAL_H */
