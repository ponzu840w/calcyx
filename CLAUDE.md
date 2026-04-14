# calcyx 開発方針

## 移植元
https://github.com/shapoco/calctus (C# / .NET)
ローカルクローン: `tmp/calctus-linux/`

## テストの方針

**エンジンのテストコードは移植元リポジトリのものを使用する。**

- 各テスト関数の冒頭に、参照元ファイルとメソッド名をコメントで明記すること
- 書式例:
  ```c
  /* 移植元: Calctus/Model/Types/ufixed113.cs - ufixed113.Test() */
  ```
- 移植元のテストコードを最大限に尊重し、独自に作成したテストは極力追加しない。

**UI のテストは独自実装。** 移植元に相当するテストがないため例外とする。

- `ui/test_undo.cpp`: SheetView の Undo/Redo 動作確認（FLTK ウィンドウを生成して直接呼び出す）
- SheetView には以下のテスト用インターフェースがある（本番コードからは使わないこと）:
  - `row_count()` / `row_expr(int)` / `focused_row()` — 状態の読み取り
  - `test_type_and_commit(const char *)` — エディタに入力してコミット
  - `test_insert_row(int)` / `test_delete_row(int)` — 行操作

## アーキテクチャ

- エンジン: C (C99)、`engine/` 以下
- GUI: FLTK (Mac / Linux / Windows)、`ui/` 以下
- Android: JNI + Kotlin（未実装）
- エンジンを共有し、GUI はプラットフォームごとに個別実装

## ビルド

```sh
cmake --preset unix      # macOS / Linux ネイティブ
cmake --build --preset unix

cmake --preset win       # Windows 向けクロスビルド
cmake --build --preset win

cmake --preset web       # WebAssembly
cmake --build --preset web
```

開発機は macOS または Linux を前提とします。FLTK・mpdecimal は初回ビルド時に自動取得。

**前提パッケージ（ターゲット別）**

| ターゲット | macOS | Linux |
|---|---|---|
| `unix` | `brew install cmake` | `sudo apt install cmake libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev` |
| `win` | `brew install cmake mingw-w64` | `sudo apt install cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |
| `web`  | `brew install cmake emscripten` | cmake + [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) |

### パッケージ生成

```sh
cpack --preset unix   # macOS → calcyx-mac.zip / Linux → calcyx_*.deb
cpack --preset win    # Windows → calcyx-win.zip (WSL 上で実行)
cpack --preset web    # Web → calcyx-web-<version>.zip (静的ホスティング用一式)
```

`cpack --preset web` で作成される zip には、index.html / app.js / calcyx.{js,wasm,css} / 各
ES module / samples/ / LICENSE 類が入り、そのまま静的ホスティングに展開できます。
ビルド時に `?v=<version>` 形式のキャッシュバスタークエリが index.html と app.js に注入さ
れるため、デプロイ毎にブラウザキャッシュが自動で無効化されます。

### Windows 向けクロスコンパイル

#### 初回セットアップ

```sh
# Linux
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
# macOS
brew install mingw-w64
```

#### ビルド

```sh
cmake --preset win
cmake --build --preset win
```

初回ビルド時は FLTK と mpdecimal を自動ダウンロード・クロスビルドして `deps/mingw64/` に配置する（`cmake/deps-win.cmake` の `ExternalProject_Add` による）。  
`deps/` は `.gitignore` 対象だが、手動操作は不要。2回目以降は `deps/mingw64/` が存在する限りスキップされる。


### アイコンの更新 (Windows)

`ui/icon.svg` を編集後、以下で `ui/icon.ico` を再生成する（WSL 上; `librsvg2-bin` と ImageMagick が必要）:

```sh
for size in 16 32 48 64 128 256; do
    rsvg-convert -w $size -h $size ui/icon.svg -o /tmp/icon_${size}.png
done
magick /tmp/icon_16.png /tmp/icon_32.png /tmp/icon_48.png \
       /tmp/icon_64.png /tmp/icon_128.png /tmp/icon_256.png \
       ui/icon.ico
```

`ui/icon.ico` はリポジトリに含める（`ui/calcyx.rc` から参照される）。

### アイコンの更新 (macOS)

`ui/icon.svg` を編集後、以下で `ui/icon.icns` を再生成する（`librsvg` が必要: `brew install librsvg`）:

```sh
mkdir -p ui/icon.iconset
for size in 16 32 64 128 256 512 1024; do
    rsvg-convert -w $size -h $size ui/icon.svg -o ui/icon.iconset/icon_${size}x${size}.png
done
cp ui/icon.iconset/icon_32x32.png    ui/icon.iconset/icon_16x16@2x.png
cp ui/icon.iconset/icon_64x64.png    ui/icon.iconset/icon_32x32@2x.png
cp ui/icon.iconset/icon_256x256.png  ui/icon.iconset/icon_128x128@2x.png
cp ui/icon.iconset/icon_512x512.png  ui/icon.iconset/icon_256x256@2x.png
cp ui/icon.iconset/icon_1024x1024.png ui/icon.iconset/icon_512x512@2x.png
iconutil -c icns ui/icon.iconset -o ui/icon.icns
```

`ui/icon.iconset/` は中間成果物なので `.gitignore` に含まれている。

### 実行ファイル

| パス | 内容 |
|---|---|
| `./build/ui/calcyx.app` | GUI アプリ本体 (macOS) |
| `./build/ui/calcyx-gui` | GUI アプリ本体 (Linux) |
| `./build/cli/calcyx` | CLI (macOS/Linux) |
| `./build/engine/test_types` | エンジン型システムのテスト |
| `./build/engine/test_eval` | エンジン評価器のテスト |
| `./build/ui/test_undo` | SheetView Undo/Redo テスト |
| `./build-win/ui/calcyx-gui.exe` | GUI アプリ本体 (Windows) |
| `./build-win/cli/calcyx.exe` | CLI (Windows) |
| `./build-win/engine/test_types.exe` | エンジン型システムのテスト (Windows) |
| `./build-win/engine/test_eval.exe` | エンジン評価器のテスト (Windows) |
