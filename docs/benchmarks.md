# calcyx ベンチマーク記録

`scripts/bench-compare.sh` による計測結果を記録していく。

## 計測方法

- 対象バイナリ: `ui/calcyx-gui` (Linux), `ui/calcyx-gui.exe` (Windows)
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
