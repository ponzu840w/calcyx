# calcyx

[日本語READMEはこちら](README-JP.md)

calcyx is a multi-platform unofficial port of [Calctus](https://github.com/shapoco/calctus) (C# / .NET).

<img width="400" alt="screenshot-2026 5月01_230538" src="https://github.com/user-attachments/assets/0e448568-4759-4a52-834c-c0df72e66acc" />
<img width="400" alt="スクリーンショット 0008-05-01 22 52 53" src="https://github.com/user-attachments/assets/d017deda-33fa-4723-8ccb-9698266a720d" />

The calculation engine has been re-implemented in C and is driven by per-platform front-ends. The current release covers the desktop trio (Windows, macOS, Linux) — GUI (FLTK) / CLI / TUI (FTXUI) — and a [Web edition](https://ponzu840w.jp/app/calcyx/) (emscripten).

(Almost the entire project is the work of Claude Code.)

## Overview

A scratchpad-style calculator: expressions are listed one per line and evaluated in sequence. Earlier-line results are usable in later lines, and editing a line re-evaluates everything in place. Built-in support for SI prefixes and a wide range of functions makes it convenient for engineering work.

## A. Install from the binary distribution

Pre-built binaries are available from [Releases](../../releases).

| Platform | GUI | CLI / TUI |
|---|---|---|
| macOS (dmg) | drag `calcyx.app` into `/Applications` | put the entry point inside the `.app` on your `PATH` (see `README_MAC.txt`) |
| macOS (Homebrew) | `brew tap ponzu840w/calcyx && brew install calcyx` | included by the same command |
| Linux | `sudo dpkg -i calcyx_*.deb` | included by the same command |
| Windows | place `calcyx-gui.exe` anywhere | place `calcyx.exe` anywhere (PATH setup optional) |

The `calcyx(.exe)` binary unifies the CLI and TUI: launched on a TTY without arguments it runs as the TUI, and when invoked with `-e` / `-o` / `-b` / `-r` or via a non-interactive stream it runs as the CLI.

## B. Build & install from source

A macOS or Linux development host is required.

### Required packages

FLTK and mpdecimal are fetched automatically at build time.

**Required**

| Host | Command |
|---|---|
| macOS | `brew install cmake` |
| Linux | `sudo apt install cmake` |
| Linux (GUI) | `sudo apt install libx11-dev libxext-dev libxft-dev libxfixes-dev libxrender-dev libxcursor-dev libxinerama-dev libfontconfig1-dev libgl1-mesa-dev` |

**Per-target additions**

| Target | Host | Command |
|---|---|---|
| `win` (Windows cross-build) | macOS | `brew install mingw-w64` |
| `win` (Windows cross-build) | Linux | `sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64` |
| `web` (WebAssembly) | macOS | `brew install emscripten` |
| `web` (WebAssembly) | Linux | see the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) |

### Build

```sh
git clone https://github.com/ponzu840w/calcyx.git
cd calcyx

# macOS / Linux native
cmake --preset unix
cmake --build --preset unix

# Windows cross-build
cmake --preset win
cmake --build --preset win

# Web (WebAssembly)
cmake --preset web
cmake --build --preset web
```

(Use the `unix-debug` / `win-debug` presets for debug builds.)

### Install with cmake

For native installation on the development host, run `cmake --install` after building.

```sh
# [GUI]
# macOS
sudo cmake --install build/ --component gui --prefix /Applications
# Linux
sudo cmake --install build/ --component gui --prefix ~/.local

# [CLI / TUI]
# macOS / Linux
cmake --install build/ --component cli --prefix ~/.local
```

Windows does not support `cmake --install`; produce a zip with the command below and unpack it where you like.

### Producing distribution packages

```sh
# macOS, Linux
cpack --preset unix   # -> macOS: calcyx-mac-<version>.dmg
                      #    Linux: calcyx-linux-<version>.deb
# Windows
cpack --preset win    # -> calcyx-win-<version>.zip
# Web (full set for static hosting)
cpack --preset web    # -> calcyx-web-<version>.zip
```

The version is taken automatically from a git tag (format: `v1.2.3`). When HEAD is not on a tag, `-dev` is appended to the file name.

## Tests

Run with the same preset name used to build.

```sh
ctest --preset unix          # all tests (macOS / Linux)
ctest --preset unix-debug    # all tests under an AddressSanitizer build
ctest --preset win           # all tests for the Windows cross-build (WSL native / wine)
ctest --preset web           # WebAssembly run via node

# To skip the GUI tests, append `*-headless`
ctest --preset unix-headless
ctest --preset win-headless
```

## Architecture

```
engine/   C99 calculation engine (types / parser / eval)
shared/   shared front-end layer (sheet_model / settings / i18n / path_utf8 etc.)
gui/      FLTK GUI (macOS / Linux / Windows)
cli/      CLI entry point (the body of the `calcyx` binary)
tui/      TUI front-end (FTXUI; statically linked into `calcyx`)
web/      Web front-end (Vanilla JS + WebAssembly)
```

The engine and shared layer are written in pure C99 and are shared by every front-end.

### Build artefacts

| Path | Contents |
|---|---|
| `build/gui/calcyx.app` | GUI application (macOS) |
| `build/gui/calcyx-gui` | GUI application (Linux) |
| `build/cli/calcyx` | CLI (macOS / Linux) |
| `build-win/gui/calcyx-gui.exe` | GUI application (Windows) |
| `build-win/cli/calcyx.exe` | CLI (Windows) |

## Differences from Calctus

calcyx is a port of Calctus, but the syntax and behaviour diverge at the points listed below.

### Since v0.3.0

- **String slicing is inclusive at both ends.** `s[2:4]` was end-exclusive in Calctus (2 characters); in calcyx it is inclusive at both ends (3 characters), to match the array and bit-field slicing rules.
- **`;` introduces a line comment.** Everything after `;` is ignored.

## Upstream

This software is based on [Calctus](https://github.com/shapoco/calctus) (Copyright (c) 2022 shapoco, MIT License).

## Third-party licenses

This software uses the following open-source libraries.

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
