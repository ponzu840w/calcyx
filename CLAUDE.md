# calcyx 開発方針

## 移植元
https://github.com/shapoco/calctus (C# / .NET)
ローカルクローン: `tmp/calctus-linux/`

## テストの方針

**テストコードは移植元リポジトリのものを使用する。**

- 各テスト関数の冒頭に、参照元ファイルとメソッド名をコメントで明記すること
- 書式例:
  ```c
  /* 移植元: Calctus/Model/Types/ufixed113.cs - ufixed113.Test() */
  ```
- 独自に作成したテストは追加しない
- 移植元にテストがない場合はテストを書かない

## アーキテクチャ

- エンジン: C (C99)、`engine/` 以下
- GUI: FLTK (Mac / Linux / Windows)、`ui/` 以下（未実装）
- Android: JNI + Kotlin（未実装）
- エンジンを共有し、GUI はプラットフォームごとに個別実装

## ビルド

```sh
cmake -S . -B build
cmake --build build
./build/engine/test_types
```

Mac では `brew install mpdecimal` が必要。
