# calcyx

[Calctus](https://github.com/shapoco/calctus) (C# / .NET) の C + FLTK 移植版。

ほぼ全てClaudeCodeによる作業。

## 概要

式を複数行並べて逐次評価できるスクラッチパッド型の計算機。前の行の結果を後の行で参照でき、行を編集するとその場でリアルタイム再評価される。

## インストール

バイナリ配布版は [Releases](../../releases) から入手可能。

| プラットフォーム | GUI | CLI |
|---|---|---|
| macOS | `calcyx.app` を `/Applications` へ | `bin/calcyx` を PATH の通った場所にコピー |
| Linux | `sudo dpkg -i calcyx_*.deb`（ランチャーに自動配置） | 同左（`/usr/bin/calcyx` に自動配置） |
| Windows | `calcyx-gui.exe` を任意の場所に配置 | `calcyx.exe` を任意の場所に配置（PATH 設定は任意） |

## ソースからビルド

### 依存パッケージ

FLTK・mpdecimal は初回ビルド時に自動取得・ビルドされる。
ターゲットに固有に必要なパッケージは事前の手動インストールが必要。

| プラットフォーム | コマンド |
|---|---|
| macOS | `brew install cmake` |
| Linux | `sudo apt install cmake libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev` |
| Windows (WSL) | `sudo apt install cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |

### ビルドコマンド

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx

cmake --preset unix      # 初回のみ (macOS / Linux)
cmake --build --preset unix

cmake --preset win       # 初回のみ (Windows, WSL 上)
cmake --build --preset win
```

### インストール

ビルドディレクトリは `cmake --preset unix` なら `build/`、`unix-debug` なら `build-debug/`。

```sh
# GUI
sudo cmake --install build --component gui --prefix /Applications  # macOS
sudo cmake --install build --component gui --prefix /usr/local     # Linux

# CLI
cmake --install build --component cli --prefix ~/.local            # macOS / Linux
```

Windows は `cmake --install` 非対応。ZIP を展開して任意の場所に配置してください。

### パッケージ生成

```sh
cpack --preset unix   # macOS → calcyx-mac-<version>.zip
                      # Linux → calcyx_<version>_amd64.deb
cpack --preset win    # Windows → calcyx-win-<version>.zip
```

バージョンは git tag から自動取得（形式: `v1.2.3`）。
タグ上にない場合はファイル名に `-dev` が付きます。

## テスト

```sh
ctest --preset unix           # 全テスト (macOS / Linux)
ctest --preset unix-headless  # GUI テストを除く (macOS / Linux)
ctest --preset win-headless   # GUI テストを除く (Windows, WSL 上)
```

## アーキテクチャ

```
engine/   C99 計算エンジン（types / parser / eval）
ui/       FLTK GUI（macOS / Linux / Windows）
cli/      CLI フロントエンド
```

エンジンは C99 のみで実装されており、複数のフロントエンドから共有する設計。

### 実行ファイル

| パス | 内容 |
|---|---|
| `build/ui/calcyx.app` | GUI アプリ本体 (macOS) |
| `build/ui/calcyx-gui` | GUI アプリ本体 (Linux) |
| `build/cli/calcyx` | CLI (macOS / Linux) |
| `build-win/ui/calcyx-gui.exe` | GUI アプリ本体 (Windows) |
| `build-win/cli/calcyx.exe` | CLI (Windows) |

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
