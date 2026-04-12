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

/* 補完候補列挙用コールバック型 */
typedef void (*builtin_enum_cb)(const char *name, int n_params, void *userdata);

/* 基本組み込み関数テーブルを列挙 (重複あり: 同名複数アリティ) */
void builtin_enum_main (builtin_enum_cb cb, void *userdata);
/* 追加組み込み関数テーブルを列挙 */
void builtin_enum_extra(builtin_enum_cb cb, void *userdata);

#endif /* BUILTIN_H */
