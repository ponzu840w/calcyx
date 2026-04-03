/* 移植元: Calctus/Model/Evaluations/EvalContext.cs */

#include "eval_ctx.h"
#include "builtin.h"
#include "../types/real.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================================================
 * 定数登録ヘルパー
 * ====================================================== */

static void add_const_real_str(eval_ctx_t *ctx, const char *name,
                                const char *str, val_fmt_t fmt) {
    real_t r;
    real_from_str(&r, str);
    val_t *v = val_new_real(&r, fmt);
    eval_var_t *var = eval_ctx_ref_var(ctx, name, true);
    if (var) {
        var->value    = v;
        var->readonly = true;
    } else {
        val_free(v);
    }
}

static void add_const_i64(eval_ctx_t *ctx, const char *name,
                           int64_t n, val_fmt_t fmt) {
    val_t *v = val_new_i64(n, fmt);
    eval_var_t *var = eval_ctx_ref_var(ctx, name, true);
    if (var) {
        var->value    = v;
        var->readonly = true;
    } else {
        val_free(v);
    }
}

/* ======================================================
 * eval_ctx_init
 * ====================================================== */

void eval_ctx_init(eval_ctx_t *ctx) {
    real_ctx_init();  /* mpdecimal コンテキストを初期化 */
    memset(ctx, 0, sizeof(*ctx));
    ctx->settings.fraction_enabled  = true;
    ctx->settings.accuracy_priority = true;

    /* 定数 (移植元: EvalContext() コンストラクタ) */
    /* PI = 3.1415926535897932384626433833 */
    add_const_real_str(ctx, "PI", "3.1415926535897932384626433833", FMT_REAL);
    /* E  = 2.7182818284590452353602874714 */
    add_const_real_str(ctx, "E",  "2.7182818284590452353602874714", FMT_REAL);

    add_const_i64(ctx, "INT_MIN",   (int64_t)(-2147483648LL),  FMT_HEX);
    add_const_i64(ctx, "INT_MAX",   (int64_t)(2147483647LL),   FMT_HEX);
    add_const_i64(ctx, "UINT_MIN",  (int64_t)(0LL),            FMT_HEX);
    add_const_i64(ctx, "UINT_MAX",  (int64_t)(4294967295LL),   FMT_HEX);
    add_const_i64(ctx, "LONG_MIN",  (int64_t)(-9223372036854775807LL - 1), FMT_HEX);
    add_const_i64(ctx, "LONG_MAX",  (int64_t)(9223372036854775807LL),      FMT_HEX);
    add_const_i64(ctx, "ULONG_MIN", (int64_t)(0LL),            FMT_HEX);
    /* ULONG_MAX は int64_t では表現できないため実数で登録 */
    add_const_real_str(ctx, "ULONG_MAX", "18446744073709551615", FMT_HEX);

    /* DECIMAL_MIN / MAX は mpdecimal の最大値に相当する概算値 */
    add_const_real_str(ctx, "DECIMAL_MIN",
        "-79228162514264337593543950335", FMT_REAL);
    add_const_real_str(ctx, "DECIMAL_MAX",
        "79228162514264337593543950335",  FMT_REAL);

    /* 組み込み関数を登録 */
    builtin_register_all(ctx);
    builtin_register_extra(ctx);
}

void eval_ctx_free(eval_ctx_t *ctx) {
    for (int i = 0; i < ctx->n_vars; i++) {
        val_free(ctx->vars[i].value);
        ctx->vars[i].value = NULL;
    }
    ctx->n_vars = 0;
}

void eval_ctx_init_child(eval_ctx_t *child, eval_ctx_t *parent) {
    memset(child, 0, sizeof(*child));
    child->parent   = parent;
    child->settings = parent->settings;
    child->depth    = parent->depth + 1;
}

/* ======================================================
 * 変数操作
 * ====================================================== */

eval_var_t *eval_ctx_ref_var(eval_ctx_t *ctx, const char *name,
                              bool allow_create) {
    /* 現スコープを検索 */
    for (int i = 0; i < ctx->n_vars; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0)
            return &ctx->vars[i];
    }
    /* 親スコープを検索 (readonly でない場合も読み取り可) */
    if (ctx->parent) {
        eval_var_t *v = eval_ctx_ref_var(ctx->parent, name, false);
        if (v) return v;
    }
    /* 新規作成 */
    if (allow_create) {
        if (ctx->n_vars >= EVAL_VAR_MAX) return NULL;
        eval_var_t *v = &ctx->vars[ctx->n_vars++];
        memset(v, 0, sizeof(*v));
        strncpy(v->name, name, TOK_TEXT_MAX - 1);
        v->value    = val_new_null();
        v->readonly = false;
        return v;
    }
    return NULL;
}

void eval_ctx_set_var(eval_ctx_t *ctx, const char *name, val_t *val) {
    eval_var_t *v = eval_ctx_ref_var(ctx, name, false);
    if (v) {
        if (v->readonly) {
            /* 上書き禁止: 現スコープに新しい変数として作成 */
            if (ctx->n_vars < EVAL_VAR_MAX) {
                eval_var_t *nv = &ctx->vars[ctx->n_vars++];
                memset(nv, 0, sizeof(*nv));
                strncpy(nv->name, name, TOK_TEXT_MAX - 1);
                nv->value    = val;
                nv->readonly = false;
                return;
            }
            val_free(val);
            return;
        }
        val_free(v->value);
        v->value = val;
    } else {
        /* 現スコープに作成 */
        if (ctx->n_vars >= EVAL_VAR_MAX) { val_free(val); return; }
        eval_var_t *nv = &ctx->vars[ctx->n_vars++];
        memset(nv, 0, sizeof(*nv));
        strncpy(nv->name, name, TOK_TEXT_MAX - 1);
        nv->value    = val;
        nv->readonly = false;
    }
}
