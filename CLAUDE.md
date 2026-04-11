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
cmake -S . -B build
cmake --build build
```

Mac では `brew install mpdecimal` が必要。

### Windows 向けクロスコンパイル（WSL 上）

`cmake/toolchain-mingw64.cmake` を使う：

```sh
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
cmake --build build-win
```

FLTK と mpdecimal は `/usr/x86_64-w64-mingw32/` 以下に MinGW-w64 向けのものが必要。
apt にない場合はソースからクロスコンパイルする。

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
| `./build/ui/calcyx` | GUI アプリ本体 |
| `./build/engine/test_types` | エンジン型システムのテスト |
| `./build/engine/test_eval` | エンジン評価器のテスト |
| `./build/ui/test_undo` | SheetView Undo/Redo テスト |
