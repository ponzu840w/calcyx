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

/* ======================================================
 * 関数値 (FuncVal / FuncDef 相当)
 * ====================================================== */

/* val.h は parser.h を include しないので body は void* で保持する */
typedef struct func_def_s {
    char    name[256];
    char  **param_names;    /* n_params 個の strdup 文字列 (owned) */
    int     n_params;       /* -1: variadic */
    int     vec_arg_idx;    /* -1: ベクタ化引数なし */
    bool    variadic;
    void   *body;           /* expr_t* (ユーザ定義) / NULL (組み込み) */
    void  (*free_body)(void *body);
    void *(*dup_body) (const void *body);
    /* 組み込み関数 (ユーザ定義は NULL) */
    struct val_s *(*builtin)(struct val_s **args, int n_args, void *ctx);
} func_def_t;

void       func_def_free(func_def_t *f);       /* param_names と body を解放 */
func_def_t *func_def_dup (const func_def_t *f);

/* 型操作レイヤーのハードリミット (eval_settings で上書き可能) */
#define VAL_ARRAY_MAX_LEN   1000000
#define VAL_STRING_MAX_LEN  1000000

typedef enum {
    VAL_REAL  = 0,
    VAL_FRAC  = 1,
    VAL_BOOL  = 2,
    VAL_STR   = 3,
    VAL_NULL  = 4,
    VAL_ARRAY = 5,
    VAL_FUNC  = 6,  /* 関数値 (FuncVal 相当) */
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
    char    *str_v;        /* VAL_STR:  malloc 済み NUL 終端文字列、所有 */
    val_t  **arr_items;    /* VAL_ARRAY: malloc 済み val_t* 配列、所有 */
    int      arr_len;      /* VAL_ARRAY: 要素数 */
    func_def_t *func_v;   /* VAL_FUNC: 関数定義 (owned) */
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
val_t *val_new_func   (func_def_t *fd);  /* fd の所有権を移転 */

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

/* --- 文字列変換 --- */
/* 生の文字列表現。VAL_STR/FMT_CHAR はクォートなし。
 * 文字列連結・内部処理用（移植元の Val.AsString 相当）。 */
void val_to_str(const val_t *v, char *buf, size_t buflen);

/* 表示用の文字列表現。VAL_STR は "..." で、FMT_CHAR は '...' で囲みエスケープする。
 * 配列内の要素も再帰的に表示形式になる（移植元の Formatter.Format 相当）。 */
void val_to_display_str(const val_t *v, char *buf, size_t buflen);

/* --- NumberFormatter (移植元: NumberFormatter.cs - FormatSettings / RealToString) --- */
typedef struct {
    int  decimal_len;       /* DecimalLengthToDisplay */
    bool e_notation;        /* ENotationEnabled */
    int  e_positive_min;    /* ENotationExpPositiveMin */
    int  e_negative_max;    /* ENotationExpNegativeMax */
    bool e_alignment;       /* ENotationAlignment */
} fmt_settings_t;

void real_to_str_with_settings(const real_t *r, const fmt_settings_t *fs,
                                char *buf, size_t buflen);

/* グローバル数値フォーマット設定 (UI から書き換え可能) */
extern fmt_settings_t g_fmt_settings;

#endif /* VAL_H */
