# cmake/test_runners.cmake — クロスターゲットのテスト起動ランチャー検出
#
# ホスト環境を判定し、以下の変数を設定する:
#
#   CALCYX_IS_WSL                — /proc/version に microsoft/WSL を含む環境なら TRUE
#   CALCYX_WIN_TEST_LAUNCHER     — Windows .exe を実行するためのコマンドリスト
#                                  (WSL: 空リスト=直接実行 / wine: "/usr/bin/wine")
#   CALCYX_WIN_TEST_AVAILABLE    — TRUE なら win-headless プリセットのテストを登録する
#   CALCYX_WASM_TEST_LAUNCHER    — node 実行パス (.js ファイルを引数に取る)
#   CALCYX_WASM_TEST_AVAILABLE   — TRUE なら web プリセットのテストを登録する
#
# 非対応環境ではプリセット単位で add_test を丸ごとスキップする (テスト単位
# DISABLED にせず、状態を明瞭に保つ)。

# --- WSL 判定 (/proc/version ベース) ---
# $WSL_DISTRO_NAME も使えるが、CI などで意図的に落ちる可能性もあるため
# カーネル文字列を直接読む方が確実。
set(CALCYX_IS_WSL FALSE)
if(EXISTS "/proc/version")
    file(READ "/proc/version" _calcyx_procver)
    if(_calcyx_procver MATCHES "[Mm]icrosoft" OR _calcyx_procver MATCHES "WSL")
        set(CALCYX_IS_WSL TRUE)
    endif()
endif()

# --- Windows cross-build 時のランチャー検出 ---
set(CALCYX_WIN_TEST_LAUNCHER "")
set(CALCYX_WIN_TEST_AVAILABLE FALSE)
if(CMAKE_CROSSCOMPILING AND WIN32)
    if(CALCYX_IS_WSL)
        # WSL: binfmt_misc で .exe が Linux プロセスから直接実行できる
        set(CALCYX_WIN_TEST_AVAILABLE TRUE)
        message(STATUS "Windows tests: WSL native (.exe direct)")
    else()
        find_program(CALCYX_WINE_EXE NAMES wine)
        if(CALCYX_WINE_EXE)
            set(CALCYX_WIN_TEST_LAUNCHER "${CALCYX_WINE_EXE}")
            set(CALCYX_WIN_TEST_AVAILABLE TRUE)
            message(STATUS "Windows tests: wine (${CALCYX_WINE_EXE})")
        else()
            message(STATUS "Windows tests: SKIPPED (non-WSL host with no wine)")
        endif()
    endif()
endif()

# --- Emscripten / WASM のランチャー検出 ---
set(CALCYX_WASM_TEST_LAUNCHER "")
set(CALCYX_WASM_TEST_AVAILABLE FALSE)
if(EMSCRIPTEN)
    find_program(CALCYX_NODE_EXE NAMES node nodejs)
    if(CALCYX_NODE_EXE)
        set(CALCYX_WASM_TEST_LAUNCHER "${CALCYX_NODE_EXE}")
        set(CALCYX_WASM_TEST_AVAILABLE TRUE)
        message(STATUS "WASM tests: node (${CALCYX_NODE_EXE})")
    else()
        message(STATUS "WASM tests: SKIPPED (no node found)")
    endif()
endif()

# --- 総合判定: 今回のプリセットでテストを登録すべきか ---
# (ネイティブ unix ビルドは常に登録。クロスビルドはランチャー検出次第。)
if(EMSCRIPTEN)
    set(CALCYX_TESTS_ENABLED "${CALCYX_WASM_TEST_AVAILABLE}")
    set(CALCYX_TEST_LAUNCHER "${CALCYX_WASM_TEST_LAUNCHER}")
elseif(CMAKE_CROSSCOMPILING AND WIN32)
    set(CALCYX_TESTS_ENABLED "${CALCYX_WIN_TEST_AVAILABLE}")
    set(CALCYX_TEST_LAUNCHER "${CALCYX_WIN_TEST_LAUNCHER}")
else()
    set(CALCYX_TESTS_ENABLED TRUE)
    set(CALCYX_TEST_LAUNCHER "")
endif()
