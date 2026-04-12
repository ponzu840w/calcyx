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

開発機： macOS または Linux。

FLTK・mpdecimal はビルド時に自動取得される。

**常に必要**

| 開発機 | コマンド |
|---|---|
| macOS | `brew install cmake` |
| Linux | `sudo apt install cmake` |
| Linux (GUI) | `sudo apt install libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev` |

**ターゲット別の追加パッケージ**

| ターゲット | 開発機 | コマンド |
|---|---|---|
| `win` (Windows クロスビルド) | macOS | `brew install mingw-w64` |
| `win` (Windows クロスビルド) | Linux | `sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |
| `wasm` (WebAssembly) | macOS | `brew install emscripten` |
| `wasm` (WebAssembly) | Linux | [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) を参照 |

### ビルドコマンド

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx

cmake --preset unix      # macOS / Linux ネイティブ
cmake --build --preset unix

cmake --preset win       # Windows 向けクロスビルド
cmake --build --preset win

cmake --preset wasm      # WebAssembly
cmake --build --preset wasm
```

デバッグビルドは `unix-debug` / `win-debug` プリセットを使用。

### インストール

ビルドディレクトリは `unix` なら `build/`、`unix-debug` なら `build-debug/`、`win` なら `build-win/`。

```sh
# GUI
sudo cmake --install build --component gui --prefix /Applications  # macOS
sudo cmake --install build --component gui --prefix /usr/local     # Linux

# CLI
cmake --install build --component cli --prefix ~/.local            # macOS / Linux
```

Windows は `cmake --install` 非対応。ZIP を展開して任意の場所に配置すること。

### パッケージ生成

```sh
cpack --preset unix   # macOS → calcyx-mac-<version>.zip
                      # Linux → calcyx_<version>_amd64.deb
cpack --preset win    # Windows → calcyx-win-<version>.zip
```

バージョンは git tag から自動取得（形式: `v1.2.3`）。
タグ上にない場合はファイル名に `-dev` が付く。

### Web 版の開発サーバー

```sh
cmake --build --preset wasm
cd build-wasm/web && python3 -m http.server 8080
# → http://localhost:8080
```

WASM は `file://` では動作しない（CORS 制限）。HTTP サーバー経由でアクセスすること。

## テスト

```sh
ctest --preset unix           # 全テスト (macOS / Linux)
ctest --preset unix-headless  # GUI テストを除く (macOS / Linux)
ctest --preset win-headless   # GUI テストを除く (Windows クロスビルド)
```

## アーキテクチャ

```
engine/   C99 計算エンジン（types / parser / eval）
ui/       FLTK GUI（macOS / Linux / Windows）
cli/      CLI フロントエンド
web/      Web フロントエンド（Vanilla JS + WebAssembly）
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
