/* 移植元: Calctus/Model/Expressions/ (各 OnEval メソッド) */

#ifndef EVAL_H
#define EVAL_H

#include "eval_ctx.h"
#include "builtin.h"

/* 式を評価して val_t* を返す。失敗時は NULL を返し ctx にエラーを設定する。
 * 成功時の戻り値は呼び出し元が val_free() で解放すること。 */
val_t *expr_eval(const expr_t *e, eval_ctx_t *ctx);

/* 文字列を解析・評価して結果を返す。
 * errmsg (NULL可) にエラーメッセージを書き込む */
val_t *eval_str(const char *src, eval_ctx_t *ctx,
                char *errmsg, int errmsg_len);

/* ; 以降の行末コメントを除去してバッファを切り詰める。
 * 文字列リテラル / 文字リテラル内の ; は無視する。
 * buf を直接書き換える。 */
void eval_strip_comment(char *buf);

#endif /* EVAL_H */
