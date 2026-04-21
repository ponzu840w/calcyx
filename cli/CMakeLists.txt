if(WIN32)
    set(_cli_rc calcyx.rc)
else()
    set(_cli_rc "")
endif()

add_executable(calcyx_cli main.c ${_cli_rc})

target_include_directories(calcyx_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
    ${MPDECIMAL_INCLUDE}
)

target_link_libraries(calcyx_cli PRIVATE
    calcyx_engine
)

target_compile_definitions(calcyx_cli PRIVATE
    CALCYX_VERSION_FULL="${CALCYX_VERSION_FULL}"
    CALCYX_EDITION="${CALCYX_PLATFORM} CLI Edition"
)

set_target_properties(calcyx_cli PROPERTIES OUTPUT_NAME "calcyx")

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_cli dep_mpdecimal)
endif()

install(TARGETS calcyx_cli RUNTIME DESTINATION bin COMPONENT cli)

# ===== ctest 登録 =====
#
# CLI 出力は `testdata/expected/*.out`, `testdata/expected/*.err` の
# ゴールデンファイルに対して改行を LF 正規化したうえで完全一致で検証する。
# 書式の退行 (プレフィックス変更・桁区切り崩れ等) をそのまま落とせる。
#
# 各テストの引数は `testdata/args/<name>.args` (1 行 1 引数) に分離。
# `;` を含む引数 (comment テスト) のエスケープを避けるため、add_test
# の COMMAND に直接書かずファイルから読み取る。

set(CLI_TESTDATA "${CMAKE_CURRENT_SOURCE_DIR}/testdata")
set(CLI_GOLDEN_RUNNER "${CLI_TESTDATA}/run_golden.cmake")

function(calcyx_cli_golden_test name)
    set(options)
    set(oneValueArgs STDOUT STDERR EXIT WORKDIR)
    set(multiValueArgs)
    cmake_parse_arguments(P "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(cmd
        "${CMAKE_COMMAND}"
        "-DCLI=$<TARGET_FILE:calcyx_cli>"
        "-DARGS_FILE=${CLI_TESTDATA}/args/${name}.args")
    if(P_STDOUT)
        list(APPEND cmd "-DSTDOUT=${CLI_TESTDATA}/expected/${P_STDOUT}")
    endif()
    if(P_STDERR)
        list(APPEND cmd "-DSTDERR=${CLI_TESTDATA}/expected/${P_STDERR}")
    endif()
    if(DEFINED P_EXIT)
        list(APPEND cmd "-DEXIT=${P_EXIT}")
    endif()
    if(P_WORKDIR)
        list(APPEND cmd "-DWORKDIR=${P_WORKDIR}")
    endif()
    list(APPEND cmd "-P" "${CLI_GOLDEN_RUNNER}")

    add_test(NAME cli/${name} COMMAND ${cmd})
    set_tests_properties(cli/${name} PROPERTIES LABELS "cli")
endfunction()

# -e モード: 基本評価
calcyx_cli_golden_test(eval_basic STDOUT eval_basic.out)

# -o both モード
calcyx_cli_golden_test(eval_both  STDOUT eval_both.out)

# セミコロンコメント除去
calcyx_cli_golden_test(comment    STDOUT comment.out)

# 複数 -e: 文脈共有
calcyx_cli_golden_test(multi_e    STDOUT multi_e.out)

# ファイルモード (basic.txt を testdata 直下で開く)
calcyx_cli_golden_test(file_mode
    STDOUT file_mode.out
    WORKDIR "${CLI_TESTDATA}")

# エラー時の終了コード 1
calcyx_cli_golden_test(error_exit STDERR error_exit.err EXIT 1)
