# calcyx ベンチマーク記録

`scripts/bench-compare.sh` による計測結果を記録していく。

## 計測方法

- 対象バイナリ: `gui/calcyx-gui` (Linux), `gui/calcyx-gui.exe` (Windows)
- 起動時間: `CALCYX_BENCH_EXIT_MS=200` フックで自動終了させ、
  起動〜終了までの壁時計時間を計測
- RSS: Linux は `/proc/PID/status` の VmHWM をポーリング、
  Windows は .NET `Process.WorkingSet64` を 50ms 毎にポーリング
- いずれも 3 回計測して中央値を採用
- hook 未対応のリビジョンでは起動時間は `-` (タイムアウトで kill)

## Phase 1: LTO+gc-sections / WASM 初期メモリ / トレイキー表共通化 / アイコン削減

計測環境:

- Linux: WSL2 (Ubuntu 24.04, kernel 5.15.90.1), gcc 13.3.0, glibc 2.39
- Windows (MinGW クロス): x86_64-w64-mingw32-g++ 13-win32
- Windows 実行: PowerShell 5.1.26100.8115 (WSL interop 経由)

### Comparison: 369dd6c5 → 0783394e

#### linux

| metric | before (369dd6c5) | after (0783394e) | Δ |
|---|---:|---:|---:|
| binary bytes | 2,940,440 | 2,811,760 | -128,680 (-4.4%) |
| text         | 2,218,679 | 2,170,737 | -47,942 (-2.2%) |
| data         | 65,608 | 65,400 | -208 (-0.3%) |
| bss          | 27,104 | 27,104 | +0 (+0.0%) |
| startup (s)  | - | 0.257 | - |
| peak RSS (KB)| 20,652 | 20,576 | -76 (-0.4%) |

#### win

| metric | before (369dd6c5) | after (0783394e) | Δ |
|---|---:|---:|---:|
| binary bytes | 7,980,499 | 5,153,434 | -2,827,065 (-35.4%) |
| text         | 1,879,068 | 1,726,540 | -152,528 (-8.1%) |
| data         | 33,276 | 32,988 | -288 (-0.9%) |
| bss          | 35,376 | 35,280 | -96 (-0.3%) |
| startup (s)  | - | 1.517 | - |
| peak RSS (KB)| 20,344 | 20,336 | -8 (-0.0%) |

### 考察

- **Linux バイナリ -128 KB (-4.4%)**: 主に LTO+gc-sections の効果。
  text セクション -48 KB は FLTK/mpdecimal の未使用関数が dead-strip された分。
- **Windows バイナリ -2.83 MB (-35.4%)**: 想定をはるかに超える削減。
  アイコン縮小 (-84 KB) に加え、`--gc-sections` により静的リンクした
  mingw ランタイム (libgcc/libstdc++) から大量の未使用コードが
  除去されたと推測される。
- **起動時間**: after は Linux 0.26 秒 / Windows 1.52 秒。before は
  hook 未対応のため計測不可。Windows の 1.5 秒は WSL interop 経由
  で実行しているオーバーヘッド (powershell.exe の起動含む) が支配的。
- **RSS**: 両プラットフォームとも約 20 MB で有意差なし。Phase 1 は
  RAM に効く変更をしていないため妥当。

## Phase 2: 内部リファクタリング (J / I / E / G)

Phase 2 の目的は「保守性向上」。機能追加・バグ修正は行わず、
以下 4 項目のファイル構成を整理した:

- **J**: `test_undo` と `calcyx_ui` の共通コード 9 ファイルを
  `calcyx_ui_common` 静的ライブラリに集約 (二重コンパイル解消)
- **I**: `engine/eval/builtin_array.c` (2197 行) をカテゴリ別
  6 ファイルに分割
- **E**: `gui/settings_globals.cpp` の init/load/save を
  スキーマテーブル駆動に変更
- **G**: `gui/PrefsDialog.cpp` (1244 行) を 5 タブファイルに分割

計測環境は Phase 1 と同一。

### Comparison: e9b89d62 → a5ddb055

#### linux

| metric | before (e9b89d62) | after (a5ddb055) | Δ |
|---|---:|---:|---:|
| binary bytes | 2,811,760 | 2,810,496 | -1,264 (-0.0%) |
| text         | 2,170,737 | 2,168,236 | -2,501 (-0.1%) |
| data         | 65,400 | 69,144 | +3,744 (+5.7%) |
| bss          | 27,104 | 27,104 | +0 (+0.0%) |
| startup (s)  | 0.258 | 0.255 | -0.003s |
| peak RSS (KB)| 20,560 | 20,752 | +192 (+0.9%) |

#### win

| metric | before (e9b89d62) | after (a5ddb055) | Δ |
|---|---:|---:|---:|
| binary bytes | 5,153,434 | 5,112,817 | -40,617 (-0.8%) |
| text         | 1,726,540 | 1,723,792 | -2,748 (-0.2%) |
| data         | 32,988 | 33,404 | +416 (+1.3%) |
| bss          | 35,280 | 35,312 | +32 (+0.1%) |
| startup (s)  | 1.604 | 1.600 | -0.004s |
| peak RSS (KB)| 20,316 | 20,276 | -40 (-0.2%) |

### 考察

- **バイナリサイズはほぼ据え置き**: Phase 2 はリファクタリングが主眼で、
  バイナリへの直接影響は想定していない。結果として Linux -0.0%、
  Windows -0.8% とノイズ程度で、回帰は発生していない。
- **text セクションが若干減少** (Linux -2.5 KB, Windows -2.7 KB):
  J の重複コンパイル解消と I の分割によりインライン境界が変わった
  ことで、LTO が使える機会が増えた副次効果と思われる。
- **data セクション +3.7 KB (Linux)**: E のスキーマテーブル
  (`SETTINGS_TABLE[]` 40+ エントリ) が読み取り専用データとして
  追加された分。許容範囲内。
- **保守性の定性的効果**: 単ファイル 1244 行の PrefsDialog.cpp が
  134 行に縮まり、タブ単位 (70〜400 行) で編集可能になった。
  builtin_array.c も 2197 行から 249 行 + 6 ファイルに分割されて
  incremental build 時に再コンパイル範囲が 1/6 以下になる。
  ベンチ対象外の効果だが、開発体験への影響は大きい。
- **RSS / 起動時間は不変**: ランタイム挙動は同一なので当然の結果。
