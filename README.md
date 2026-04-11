# calcyx

[Calctus](https://github.com/shapoco/calctus) (C# / .NET) の C + FLTK 移植版。

ほぼ全てClaudeCodeによる作業。

## 概要

式を複数行並べて逐次評価できるスクラッチパッド型の計算機。前の行の結果を後の行で参照でき、行を編集するとその場でリアルタイム再評価される。

## ビルド

### 依存

- CMake 3.15+
- FLTK 1.3+
- mpdecimal（Mac: `brew install mpdecimal`）

### ビルド手順

```sh
cmake -S . -B build
cmake --build build
./build/ui/calcyx
```

テストのみ実行する場合:

```sh
./build/engine/test_types
./build/engine/test_eval
```

## アーキテクチャ

```
engine/          C99 計算エンジン
  types/         値型 (real, frac, quad, ufixed113, val)
  parser/        字句解析・構文解析
  eval/          評価器・組み込み関数
ui/              FLTK GUI (Mac / Linux / Windows)
samples/         サンプルファイル (Examples.txt, Test_*.txt)
```

エンジンは C99 のみで実装されており、GUI・Android など複数のフロントエンドから共有する設計。

## 移植元

https://github.com/shapoco/calctus

このソフトウェアは [Calctus](https://github.com/shapoco/calctus) (Copyright (c) 2022 shapoco, MIT License) をもとに開発されています。

---

## 未実装機能メモ

オリジナル Calctus との差分。実装優先度の参考用。

### 計算関数

| カテゴリ | 未実装の主な関数 |
|---|---|
| 配列操作 | `range`, `map`, `filter`, `sort`, `sum`, `ave`, `invSum`, `harMean`, `geoMean`, `indexOf`, `contains`, `unique`, `except`, `intersect`, `union`, `extend`, `aggregate` 等 |
| ビット・バイト操作 | `pack/unpack`, `swap2/4/8`, `swapNib`, `reverseBits`, `reverseBytes`, `rotateL/R`, `count1` 等 |
| 文字列操作 | `trim`, `trimStart`, `trimEnd`, `replace`, `toLower`, `toUpper`, `startsWith`, `endsWith`, `split`, `join` |
| 色変換 | `rgb`, `hsv2rgb`, `rgb2hsv`, `hsl2rgb`, `rgb2hsl`, `yuv2rgb`, `rgb2yuv`, `rgbTo565`, `rgbFrom565`, `pack565`, `unpack565` |
| エンコーディング | `utf8Enc/Dec`, `urlEnc/Dec`, `base64Enc/Dec`, `base64EncBytes/DecBytes` |
| E系列 | `esFloor`, `esCeil`, `esRound`, `esRatio` |
| 素数 | `prime`, `isPrime`, `primeFact` |
| グレイコード | `toGray`, `fromGray` |
| ECC / パリティ | `xorReduce`, `oddParity`, `eccWidth`, `eccEnc`, `eccDec` |
| キャスト | `rat` (有理数近似), `array` (文字列→配列), `str` (配列→文字列) |
| 方程式解法 | `solve` (ニュートン法) |
| 日時 | `now` 等の日時関数 |

### UI 機能

| 機能 | 備考 |
|---|---|
| グラフ表示 | 関数プロット・軸設定ウィンドウ |
| Undo / Redo | 現在未実装 |
| 入力補完ポップアップ | 関数名・変数名の候補表示 |
| フォーマットホットキー | F8=Auto, F9=Dec, F10=Hex, F11=Bin, F12=SI 等 |
| 設定ダイアログ | フォント・DPI・ホットキー設定 |
| ウィンドウ状態の永続化 | 位置・サイズの保存 |
