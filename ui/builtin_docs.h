// 組み込み関数の説明文テーブル (UI レイヤーのみ)
// 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイルの Description 文字列
// ローカライズが必要になったときは gettext の _() でラップする

#pragma once

// 関数名から説明文を返す。未登録なら nullptr。
const char *builtin_doc(const char *name);
