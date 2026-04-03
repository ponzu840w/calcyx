/* 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイル */

#ifndef BUILTIN_H
#define BUILTIN_H

#include "eval_ctx.h"

/* 組み込み関数を ctx に登録する */
void builtin_register_all(eval_ctx_t *ctx);

/* 組み込み関数を名前で検索する (context に依存しないグローバルテーブル) */
func_def_t *builtin_find(const char *name, int n_args);

#endif /* BUILTIN_H */
