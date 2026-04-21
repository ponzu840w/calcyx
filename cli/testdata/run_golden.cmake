# cli/testdata/run_golden.cmake — CLI ゴールデンファイル比較ランナー
#
# add_test から `cmake -D ... -P` で起動される。CLI を実行して stdout /
# stderr / 終了コードを期待値と完全一致で比較する (改行は LF に正規化)。
#
# 入力変数:
#   CLI         — 実行する calcyx_cli のパス ($<TARGET_FILE:calcyx_cli>)
#   ARGS_FILE   — 1 行 1 引数の引数ファイル。空行は無視
#   STDOUT      — stdout の期待値ファイル (任意)
#   STDERR      — stderr の期待値ファイル (任意)
#   EXIT        — 期待する終了コード (既定 0)
#   WORKDIR     — CLI 実行時のカレントディレクトリ (任意)
#
# 引数ファイルを使う理由: CMake のリストは `;` 区切りなので、テスト対象
# 文字列 "1+1 ; これはコメント" のような `;` 入り引数を add_test 行に
# 直接書くとエスケープが複雑になる。ファイル経由なら透過的に扱える。

if(NOT CLI)
    message(FATAL_ERROR "run_golden: CLI variable is required")
endif()
if(NOT ARGS_FILE)
    message(FATAL_ERROR "run_golden: ARGS_FILE variable is required")
endif()
if(NOT DEFINED EXIT)
    set(EXIT 0)
endif()

# 引数ファイルを読み込んでリスト化 (空行は除外)
file(STRINGS "${ARGS_FILE}" ARGS_LIST)
set(FILTERED_ARGS)
foreach(a IN LISTS ARGS_LIST)
    if(NOT a STREQUAL "")
        list(APPEND FILTERED_ARGS "${a}")
    endif()
endforeach()

set(EXECUTE_OPTS
    COMMAND "${CLI}" ${FILTERED_ARGS}
    OUTPUT_VARIABLE ACTUAL_STDOUT
    ERROR_VARIABLE  ACTUAL_STDERR
    RESULT_VARIABLE ACTUAL_EXIT)
if(WORKDIR)
    list(APPEND EXECUTE_OPTS WORKING_DIRECTORY "${WORKDIR}")
endif()
execute_process(${EXECUTE_OPTS})

# CRLF → LF 正規化 (Windows 実行時にも差分が出ないようにする)
string(REPLACE "\r\n" "\n" ACTUAL_STDOUT "${ACTUAL_STDOUT}")
string(REPLACE "\r\n" "\n" ACTUAL_STDERR "${ACTUAL_STDERR}")

set(HAS_FAILURE FALSE)

# 終了コード
if(NOT ACTUAL_EXIT STREQUAL "${EXIT}")
    message("exit code mismatch: expected=${EXIT} actual=${ACTUAL_EXIT}")
    set(HAS_FAILURE TRUE)
endif()

# stdout 比較
if(STDOUT)
    file(READ "${STDOUT}" EXPECTED_STDOUT)
    string(REPLACE "\r\n" "\n" EXPECTED_STDOUT "${EXPECTED_STDOUT}")
    if(NOT ACTUAL_STDOUT STREQUAL EXPECTED_STDOUT)
        message("stdout mismatch:")
        message("--- expected (${STDOUT}) ---")
        message("${EXPECTED_STDOUT}")
        message("--- actual ---")
        message("${ACTUAL_STDOUT}")
        message("--- end ---")
        set(HAS_FAILURE TRUE)
    endif()
endif()

# stderr 比較
if(STDERR)
    file(READ "${STDERR}" EXPECTED_STDERR)
    string(REPLACE "\r\n" "\n" EXPECTED_STDERR "${EXPECTED_STDERR}")
    if(NOT ACTUAL_STDERR STREQUAL EXPECTED_STDERR)
        message("stderr mismatch:")
        message("--- expected (${STDERR}) ---")
        message("${EXPECTED_STDERR}")
        message("--- actual ---")
        message("${ACTUAL_STDERR}")
        message("--- end ---")
        set(HAS_FAILURE TRUE)
    endif()
endif()

if(HAS_FAILURE)
    message(FATAL_ERROR "golden mismatch for ${ARGS_FILE}")
endif()
