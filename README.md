# calcyx

calcyx（カルキクス、カルシクス、カルサイクス、カルキシー）は、[Calctus](https://github.com/shapoco/calctus) (C# / .NET) のマルチプラットフォーム再実装プロジェクト。

計算エンジンをCで再実装し、UIは柔軟に各言語で実装。現在のリリースはPC版（Windows, macOS, Linux）（GUI（FLTK）, CLI）および[Web版](https://ponzu840w.jp/app/calcyx/)（emscripten）。今後はTUI版やスマフォ版を実装予定。

（ほぼ全てClaudeCodeによる作業。）

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

開発機： macOS または Linux。

### 依存パッケージ

FLTK・mpdecimal はビルド時に自動取得される。

**必須**

| 開発機 | コマンド |
|---|---|
| macOS | `brew install cmake` |
| Linux | `sudo apt install cmake` |
| Linux (GUI) | `sudo apt install libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev libgl1-mesa-dev` |

**ターゲット別の追加パッケージ**

| ターゲット | 開発機 | コマンド |
|---|---|---|
| `win` (Windows クロスビルド) | macOS | `brew install mingw-w64` |
| `win` (Windows クロスビルド) | Linux | `sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |
| `web` (WebAssembly) | macOS | `brew install emscripten` |
| `web` (WebAssembly) | Linux | [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) を参照 |

### ビルドコマンド

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx

# macOS / Linux ネイティブ
cmake --preset unix
cmake --build --preset unix

# Windows 向けクロスビルド
cmake --preset win
cmake --build --preset win

# Web (WebAssembly)
cmake --preset web
cmake --build --preset web
```

デバッグビルドは `unix-debug` / `win-debug` プリセットを使用。

### インストール

ビルドディレクトリは `unix` なら `build/`、`unix-debug` なら `build-debug/`、`win` なら `build-win/`。

```sh
# [GUI]
# macOS
sudo cmake --install build --component gui --prefix /Applications
# Linux
sudo cmake --install build --component gui --prefix /usr/local

# [CLI]
# macOS / Linux
cmake --install build --component cli --prefix ~/.local
```

Windows は `cmake --install` 非対応。ZIP を展開して任意の場所に配置すること。

### パッケージ生成

```sh
# macOS, Linux
cpack --preset unix   # -> calcyx-mac-<version>.zip
                      # or calcyx-linux-<version>.deb
# Windows
cpack --preset win    # -> calcyx-win-<version>.zip
# Web (静的ホスティング用ファイル一式)
cpack --preset web    # -> calcyx-web-<version>.zip
```

バージョンは git tag から自動取得（形式: `v1.2.3`）。
タグ上にない場合はファイル名に `-dev` が付く。

### Web 版の開発サーバー

```sh
cmake --build --preset web
cd build-web/web && python3 -m http.server 8080
# → http://localhost:8080
```

## テスト

ビルドに使った preset と同名で実行する。

```sh
ctest --preset unix          # 全テスト (macOS / Linux)
ctest --preset unix-debug    # AddressSanitizer ビルドで全テスト
ctest --preset win           # Windows クロスビルド全テスト (WSL native / wine)
ctest --preset web           # WebAssembly を node 経由で実行

# GUIテストをスキップしたい場合は `*-headless`
ctest --preset unix-headless
ctest --preset win-headless
```

## アーキテクチャ

```
engine/   C99 計算エンジン（types / parser / eval）
gui/       FLTK GUI（macOS / Linux / Windows）
cli/      CLI フロントエンド
web/      Web フロントエンド（Vanilla JS + WebAssembly）
```

エンジンは C99 のみで実装されており、複数のフロントエンドから共有する設計。

### 実行ファイル

| パス | 内容 |
|---|---|
| `build/gui/calcyx.app` | GUI アプリ本体 (macOS) |
| `build/gui/calcyx-gui` | GUI アプリ本体 (Linux) |
| `build/cli/calcyx` | CLI (macOS / Linux) |
| `build-win/gui/calcyx-gui.exe` | GUI アプリ本体 (Windows) |
| `build-win/cli/calcyx.exe` | CLI (Windows) |

## Calctus からの変更点

calcyx は Calctus の移植版ですが、以下の点で文法・動作が異なります。

### v0.3.0

- **文字列スライスが両端インクルーシブ** Calctus では `s[2:4]` が末尾エクスクルーシブ（2文字）でしたが、calcyx では配列・ビットフィールドと統一して両端インクルーシブ（3文字）です。
- **`;` による行コメント** `;` 以降は無視されます。

## 移植元

このソフトウェアは [Calctus](https://github.com/shapoco/calctus) (Copyright (c) 2022 shapoco, MIT License) をもとに開発されています。

## Third-party licenses

このソフトウェアは以下のオープンソースライブラリを使用しています。

### FLTK 1.4.4

Copyright (c) 1998-2024 Bill Spitzak and others.

[FLTK](https://www.fltk.org/) is licensed under the GNU Lesser General Public License version 2 with the following exception:

> Statically linking applications to the FLTK library does not constitute a modified or derivative work and does not require the author to provide source code for the application, use the shared FLTK libraries, or link their applications against a user-installed version of FLTK.

Full license text: https://www.fltk.org/COPYING.php

### mpdecimal 4.0.0

Copyright (c) 2008-2024 Stefan Krah.

[mpdecimal](https://www.bytereef.org/mpdecimal/) is licensed under the BSD 2-Clause License.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
