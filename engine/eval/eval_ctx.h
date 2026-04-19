/* 移植元: Calctus/Model/Evaluations/EvalContext.cs,
 *          Calctus/Model/Evaluations/EvalSettings.cs,
 *          Calctus/Model/Evaluations/Var.cs */

#ifndef EVAL_CTX_H
#define EVAL_CTX_H

#include "../parser/expr.h"
#include <stdbool.h>

/* ======================================================
 * 設定 (EvalSettings)
 * ====================================================== */

typedef struct {
    bool fraction_enabled;   /* 分数モードを有効化 (デフォルト: true) */
    bool accuracy_priority;  /* 精度優先 (デフォルト: true) */
    int  max_array_length;   /* 配列の最大要素数 (デフォルト: 256) */
    int  max_string_length;  /* 文字列の最大長 (デフォルト: 256) */
    int  max_call_depth;     /* 関数呼び出しの最大再帰深度 (デフォルト: 64) */
} eval_settings_t;

/* ======================================================
 * 変数 (Var)
 * ====================================================== */

#define EVAL_VAR_MAX 256

typedef struct {
    char   name[TOK_TEXT_MAX];
    val_t *value;     /* owned */
    bool   readonly;
} eval_var_t;

/* ======================================================
 * 評価コンテキスト (EvalContext)
 * ====================================================== */

#define EVAL_DEPTH_MAX 100

typedef struct eval_ctx_s eval_ctx_t;
struct eval_ctx_s {
    eval_ctx_t    *parent;    /* NULL = ルートコンテキスト */
    eval_settings_t settings;
    eval_var_t     vars[EVAL_VAR_MAX];
    int            n_vars;
    int            depth;
    bool           has_error;
    char           error_msg[256];
    int            error_pos;
};

/* ルートコンテキストを初期化 (定数・組み込み関数を登録) */
void eval_ctx_init (eval_ctx_t *ctx);
/* 全変数を解放 */
void eval_ctx_free (eval_ctx_t *ctx);
/* 子コンテキストを初期化 (親からの変数は継承しない; 参照は parent 経由) */
void eval_ctx_init_child(eval_ctx_t *child, eval_ctx_t *parent);

/* 変数参照。allow_create=true のとき見つからなければ現スコープに作成 */
eval_var_t *eval_ctx_ref_var  (eval_ctx_t *ctx, const char *name,
                                bool allow_create);
/* 変数への値代入。val の所有権を移転 */
void        eval_ctx_set_var  (eval_ctx_t *ctx, const char *name, val_t *val);

/* エラー設定マクロ */
#define EVAL_ERROR(ctx, pos, fmt, ...) \
    do { if (!(ctx)->has_error) { \
        (ctx)->has_error = true; \
        (ctx)->error_pos = (pos); \
        snprintf((ctx)->error_msg, sizeof((ctx)->error_msg), fmt, ##__VA_ARGS__); \
    } } while(0)

#endif /* EVAL_CTX_H */
