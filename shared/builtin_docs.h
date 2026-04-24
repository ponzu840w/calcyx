/* 組み込み関数の説明文テーブル (UI 非依存)。
 * GUI の補完ポップアップと TUI から共有される。
 * 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイルの Description 文字列
 * ローカライズが必要になったときは gettext の _() でラップする */

#ifndef CALCYX_SHARED_BUILTIN_DOCS_H
#define CALCYX_SHARED_BUILTIN_DOCS_H

#ifdef __cplusplus
extern "C" {
#endif

/* 関数名から説明文を返す。未登録なら NULL。 */
const char *builtin_doc(const char *name);

#ifdef __cplusplus
}
#endif

#endif
