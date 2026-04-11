# calcyx

[Calctus](https://github.com/shapoco/calctus) (C# / .NET) の C + FLTK 移植版。

ほぼ全てClaudeCodeによる作業。

## 概要

式を複数行並べて逐次評価できるスクラッチパッド型の計算機。前の行の結果を後の行で参照でき、行を編集するとその場でリアルタイム再評価される。

## インストール（macOS）

### 前提条件

- macOS 12 以降
- [Homebrew](https://brew.sh/)
- Xcode Command Line Tools（`xcode-select --install`）

### 依存ライブラリのインストール

```sh
brew install cmake fltk mpdecimal
```

### ビルドとインストール

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx
cmake -S . -B build
cmake --build build
cp -r build/ui/calcyx.app /Applications/
```

Launchpad や Spotlight から起動できます。

## インストール（Windows）

### 前提条件

- Windows 10 以降
- [MSYS2](https://www.msys2.org/) (MinGW-w64 環境)

### 依存ライブラリのインストール

MSYS2 MinGW 64-bit シェルで：

```sh
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-fltk mingw-w64-x86_64-mpdecimal
```

### ビルドとインストール

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

`build/ui/calcyx.exe` を任意の場所にコピーして実行できます。

## インストール（Linux）

### 依存ライブラリのインストール

```sh
sudo apt install cmake libfltk1.3-dev libmpdec-dev
```

### ビルドとインストール

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx
cmake -S . -B build
cmake --build build
sudo cp build/ui/calcyx /usr/local/bin/
```

## ビルドについてより詳しく（開発者向け）

### ビルド手順

```sh
cmake -S . -B build
cmake --build build
```

| プラットフォーム | 起動方法 |
|---|---|
| macOS | `open build/ui/calcyx.app` |
| Windows | `build/ui/calcyx.exe` |
| Linux | `./build/ui/calcyx` |

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

| カテゴリ | 未実装の関数 |
|---|---|
| 絶対値・符号 | `mag` |
| 配列操作 | `len`, `range`, `rangeInclusive`, `reverseArray`, `map`, `filter`, `count`, `sort`, `extend`, `aggregate`, `concat`, `unique`, `except`, `intersect`, `union`, `indexOf`, `lastIndexOf`, `contains`、`all`/`any` の述語関数付き2引数版 |
| 文字列操作 | `trim`, `trimStart`, `trimEnd`, `replace`, `toLower`, `toUpper`, `startsWith`, `endsWith`, `split`, `join` |
| 統計 | `sum`, `ave`, `invSum`, `harMean`, `geoMean` |
| 素数 | `prime`, `isPrime`, `primeFact` |
| キャスト | `real`, `rat`, `array`, `str` |
| ビット・バイト操作 | `pack`, `unpack`, `swapNib`, `swap2`, `swap4`, `swap8`, `reverseBits`, `reverseBytes`, `rotateL`, `rotateR`, `count1` |
| グレイコード | `toGray`, `fromGray` |
| 色変換 | `rgb`, `hsv2rgb`, `rgb2hsv`, `hsl2rgb`, `rgb2hsl`, `yuv2rgb`, `rgb2yuv`, `rgbTo565`, `rgbFrom565`, `pack565`, `unpack565` |
| 方程式解法 | `solve` (ニュートン法) |
| ECC / パリティ | `xorReduce`, `oddParity`, `eccWidth`, `eccEnc`, `eccDec` |
| E系列 | `esFloor`, `esCeil`, `esRound`, `esRatio` |
| エンコーディング | `utf8Enc`, `utf8Dec`, `urlEnc`, `urlDec`, `base64Enc`, `base64Dec`, `base64EncBytes`, `base64DecBytes` |

### UI 機能

| 機能 | 備考 |
|---|---|
| グラフ表示 | 関数プロット・軸設定ウィンドウ |
| 入力補完ポップアップ | 関数名・変数名の候補表示 |
| フォーマットホットキー | F8=Auto, F9=Dec, F10=Hex, F11=Bin, F12=SI 等 |
| 設定ダイアログ | フォント・DPI・ホットキー設定 |
| ウィンドウ状態の永続化 | 位置・サイズの保存 |
