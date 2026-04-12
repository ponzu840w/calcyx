# calcyx

[Calctus](https://github.com/shapoco/calctus) (C# / .NET) の C + FLTK 移植版。

ほぼ全てClaudeCodeによる作業。

## 概要

式を複数行並べて逐次評価できるスクラッチパッド型の計算機。前の行の結果を後の行で参照でき、行を編集するとその場でリアルタイム再評価される。

## ビルド&インストール

### 依存パッケージ

FLTK・mpdecimal は初回ビルド時に自動取得・ビルドされます。  
X11 まわりなど OS に密着した依存のみ手動インストールが必要です。

| プラットフォーム | コマンド |
|---|---|
| macOS | `brew install cmake` |
| Linux | `sudo apt install cmake libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev` |
| Windows (WSL) | `sudo apt install cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |

### ビルド

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx
cmake --preset unix    # macOS / Linux
cmake --preset windows    # Windows (WSL)
cmake --build --preset <preset>
```

### インストール

| プラットフォーム | 方法 |
|---|---|
| macOS | `cp -r build/ui/calcyx.app /Applications/` |
| Linux | `sudo cp build/ui/calcyx /usr/local/bin/` |
| Windows | `build-win/ui/calcyx.exe` をコピーして実行 |

## アーキテクチャ

```
engine/   C99 計算エンジン（types / parser / eval）
ui/       FLTK GUI（macOS / Linux / Windows )
```

エンジンは C99 のみで実装されており、複数のフロントエンドから共有する設計。

## テスト

```sh
cmake --build --preset unix
./build/engine/test_types
./build/engine/test_eval
```

## 移植元

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
| 設定ダイアログ | フォント・DPI・ホットキー設定 |
| ウィンドウ状態の永続化 | 位置・サイズの保存 |
