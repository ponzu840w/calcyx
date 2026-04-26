# cli/ — 統合エントリポイント。
#
# Phase 2 リファクタ (2026-04-24) で CLI と TUI を単一バイナリ calcyx に統合。
# main.cpp が argv / stdin の判定を行い、対話モードでは tui/ の TuiApp を
# 呼び出す。非対話 (pipe) や -e / -o / -b / -r が指定されたときは従来の
# CLI ロジックを実行する。calcyx_tui_lib 経由で FTXUI をリンクする。
#
# ターゲット名は互換性維持のため calcyx_cli のまま (ctest の $<TARGET_FILE>
# 参照や既存テストハーネスで使われる)。出力ファイル名は "calcyx"。

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(_cli_rc calcyx.rc)
else()
    set(_cli_rc "")
endif()

add_executable(calcyx_cli main.cpp ${_cli_rc})

target_include_directories(calcyx_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/shared
    ${MPDECIMAL_INCLUDE}
)

target_link_libraries(calcyx_cli PRIVATE
    calcyx_tui_lib
)

target_compile_definitions(calcyx_cli PRIVATE
    CALCYX_VERSION_FULL="${CALCYX_VERSION_FULL}"
    CALCYX_EDITION="${CALCYX_PLATFORM} Edition"
)

set_target_properties(calcyx_cli PROPERTIES OUTPUT_NAME "calcyx")

# 開発ツリーで TUI の File > Samples が動くよう exe の隣に samples/ をコピー。
# パッケージ (cpack) では別途 ${CMAKE_SOURCE_DIR}/CMakeLists.txt の install
# ルールで適切な場所 (Linux: share/calcyx/samples, Windows zip: samples/) に
# 配置されるので、本ルールは build ツリーのみに作用する。
add_custom_command(TARGET calcyx_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/samples"
        "$<TARGET_FILE_DIR:calcyx_cli>/samples"
)

if(WIN32)
    target_link_libraries(calcyx_cli PRIVATE
        -static -static-libgcc -static-libstdc++
    )
endif()

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_cli dep_mpdecimal)
endif()
if(TARGET dep_ftxui)
    add_dependencies(calcyx_cli dep_ftxui)
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

# クロスターゲットで Windows .exe を wine 経由で叩くなど、ランチャーを
# ランナー内の execute_process に差し込む必要がある。LAUNCHER は `;`
# 区切りリスト文字列として渡すため、空の場合も安全に動く。
string(REPLACE ";" "|" _cli_launcher "${CALCYX_TEST_LAUNCHER}")

function(calcyx_cli_golden_test name)
    set(options)
    set(oneValueArgs STDOUT STDERR EXIT WORKDIR)
    set(multiValueArgs)
    cmake_parse_arguments(P "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(cmd
        "${CMAKE_COMMAND}"
        "-DCLI=$<TARGET_FILE:calcyx_cli>"
        "-DARGS_FILE=${CLI_TESTDATA}/args/${name}.args")
    if(_cli_launcher)
        # | 区切りをランナー側で ; に戻してリスト化する
        list(APPEND cmd "-DLAUNCHER=${_cli_launcher}")
    endif()
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

if(CALCYX_TESTS_ENABLED)
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
endif()
