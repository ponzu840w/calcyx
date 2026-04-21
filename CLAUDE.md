# calcyx 開発方針

## 移植元
https://github.com/shapoco/calctus (C# / .NET)
ローカルクローン: `tmp/calctus-linux/`

## テストの方針

ctest には 36 本のテストが 3 系統 (engine 27 / gui 3 / cli 6) で登録されています
(`ctest --preset unix` で全実行)。

クロスターゲット別の登録件数は以下のとおり:

| プリセット | engine | gui | cli | 計 | 備考 |
|---|---|---|---|---|---|
| `unix` | 27 | 3 | 6 | 36 | ネイティブ Linux / macOS |
| `win` | 27 | 3 | 6 | 36 | Windows クロスビルド全テスト |
| `win-headless` | 27 | (除外) | 6 | 33 | gui ラベルを filter 除外 |
| `web` | 2 | (なし) | (なし) | 2 | `engine/types` と `engine/parser` のみ |

`win` / `win-headless` は WSL であれば `.exe` をネイティブ実行、非 WSL では `wine`
を検出してラップする (`cmake/test_runners.cmake`)。どちらも無ければ登録をまるごと
スキップする (プリセット単位で無効化)。`win` で GUI テストを実行するには WSLg
または wine による X 描画が必要。

`web` プリセットは `engine/Test_*` (test_eval が sample ファイルを 1 本ずつ評価)
をスキップする — mpdecimal の計算が WASM 下で極端に遅く、1 ファイル 数分以上
かかるため。型システムと lexer/parser の回帰は `engine/types` と `engine/parser`
でカバーする。

**テスト追加時の原則:**

- 移植元 (Calctus) に対応するテストは**絶対**に尊重する。勝手に削除・書き換えしない。
- それ以外の独自テストを追加する場合は、**事前にユーザーへ必要性と内容を
  説明し、承認を得ること**。移植元に存在しないシナリオ (calcyx 固有のバグ、
  CLI 引数、UI 操作、プラットフォーム依存の挙動など) をカバーするために
  必要と判断した時点で、どんなテストをどう追加するかを提案する。

### エンジン (`engine`, 27 本)

**エンジンのテストコードは移植元リポジトリのものを使用する。**

- 各テスト関数の冒頭に、参照元ファイルとメソッド名をコメントで明記すること
- 書式例:
  ```c
  /* 移植元: Calctus/Model/Types/ufixed113.cs - ufixed113.Test() */
  ```
- 移植元のテストコードを最大限に尊重する。

登録内訳:

- `engine/types` — `engine/test_types.c` で型システム (val / real / i64 など) の
  単体テストを実行。
- `engine/parser` — `engine/test_parser.c` でレキサー/パーサーの境界ケースを検証
  (calcyx 独自、移植元になし)。2026-04-21 の lexer.c 空白スキップバグのような
  リグレッションを検知する目的。
- `engine/Test_*` — `samples/Test_*.txt` を `engine/CMakeLists.txt` が glob で
  拾い、`engine/test_eval.c` がファイル単位で `assert(...)` 行を評価。移植元の
  Calctus テストを移植した assert 式が並んでいる。

### UI (`gui`, 3 本)

**UI のテストは独自実装。** 移植元に相当するテストがないため例外とする。

- `gui/sheet` — `ui/test_undo.cpp` が FLTK ウィンドウを生成して SheetView の
  Undo/Redo 動作を直接呼び出す。
- `ui/completion` — `ui/test_completion.cpp` で CompletionPopup の icontains /
  istartswith のランキング関数を pure function テスト。
- `ui/settings` — `ui/test_settings.cpp` で settings スキーマテーブルの往復
  (defaults → save → load → 一致) を検証。テスト専用 API
  `settings_set_path_for_test()` で conf ファイルを一時ディレクトリに差し替える。
- SheetView には以下のテスト用インターフェースがある（本番コードからは使わないこと）:
  - `row_count()` / `row_expr(int)` / `focused_row()` — 状態の読み取り
  - `test_type_and_commit(const char *)` — エディタに入力してコミット
  - `test_insert_row(int)` / `test_delete_row(int)` — 行操作

### CLI (`cli`, 6 本)

`cli/CMakeLists.txt` の `calcyx_cli_golden_test()` ヘルパで登録。
`cli/testdata/expected/*.out` と `*.err` のゴールデンファイルに対して
stdout / stderr / 終了コードを改行 LF 正規化のうえ完全一致で検証する。
引数は `cli/testdata/args/*.args` (1 行 1 引数) に分離して CMake リストの
`;` エスケープを回避している。

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

開発機は macOS または Linux を前提とします。FLTK・mpdecimal は全ターゲット（unix / win / web）で初回ビルド時に自動取得。

**前提パッケージ（ターゲット別）**

| ターゲット | macOS | Linux |
|---|---|---|
| `unix` | `brew install cmake` | `sudo apt install cmake libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev` |
| `win` | `brew install cmake mingw-w64` | `sudo apt install cmake gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |
| `web`  | `brew install cmake emscripten` | cmake + [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) |

### パッケージ生成

```sh
cpack --preset unix   # macOS → calcyx-mac-<version>.zip / Linux → calcyx-linux-<version>.deb
cpack --preset win    # Windows → calcyx-win-<version>.zip (WSL 上で実行)
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

初回ビルド時は FLTK と mpdecimal を自動ダウンロード・クロスビルドして `deps/mingw64/` に配置する（`cmake/deps.cmake` の `WIN32 AND CMAKE_CROSSCOMPILING` 分岐が `ExternalProject_Add` を実行する）。
`deps/` は `.gitignore` 対象だが、手動操作は不要。2回目以降は `deps/mingw64/` が存在する限りスキップされる。


### アイコンの更新 (Windows)

`ui/icon.svg` を編集後、以下で `ui/icon.ico` を再生成する（WSL 上; `librsvg2-bin` と ImageMagick が必要）:

```sh
for size in 16 32 48 256; do
    rsvg-convert -w $size -h $size ui/icon.svg -o /tmp/icon_${size}.png
done
magick /tmp/icon_16.png /tmp/icon_32.png /tmp/icon_48.png /tmp/icon_256.png \
       ui/icon.ico
```

`ui/icon.ico` はリポジトリに含める（`ui/calcyx.rc` から参照される）。
サイズは 16/32/48 (タイトルバー／タスクバー) と 256 (Explorer の大プレビュー) のみ。
中間サイズ (64/128) は Windows が必要に応じて自動スケールするため不要。

### アイコンの更新 (macOS)

`ui/icon.svg` を編集後、以下で `ui/icon.icns` を再生成する（`librsvg` が必要: `brew install librsvg`）:

```sh
mkdir -p ui/icon.iconset
for size in 16 32 64 128 256 512; do
    rsvg-convert -w $size -h $size ui/icon.svg -o ui/icon.iconset/icon_${size}x${size}.png
done
cp ui/icon.iconset/icon_32x32.png   ui/icon.iconset/icon_16x16@2x.png
cp ui/icon.iconset/icon_64x64.png   ui/icon.iconset/icon_32x32@2x.png
cp ui/icon.iconset/icon_256x256.png ui/icon.iconset/icon_128x128@2x.png
cp ui/icon.iconset/icon_512x512.png ui/icon.iconset/icon_256x256@2x.png
iconutil -c icns ui/icon.iconset -o ui/icon.icns
```

`ui/icon.iconset/` は中間成果物なので `.gitignore` に含まれている。
1024x1024 (512x512@2x) は Mac App Store 配布用の巨大アイコンなので、ローカル
ユーティリティでは不要として外してある。

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
