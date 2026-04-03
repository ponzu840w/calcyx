/* 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイル */

#ifndef BUILTIN_H
#define BUILTIN_H

#include "eval_ctx.h"

/* 組み込み関数を ctx に登録する */
void builtin_register_all(eval_ctx_t *ctx);

/* 組み込み関数を名前で検索する (context に依存しないグローバルテーブル) */
func_def_t *builtin_find(const char *name, int n_args);

/* 追加組み込み関数 (配列・文字列操作等) を ctx に登録する */
void builtin_register_extra(eval_ctx_t *ctx);

/* 追加組み込み関数を名前で検索する */
func_def_t *builtin_find_extra(const char *name, int n_args);

#endif /* BUILTIN_H */
