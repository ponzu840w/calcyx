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
    CALCYX_EDITION="${CALCYX_PLATFORM} CLI"
)

set_target_properties(calcyx_cli PROPERTIES OUTPUT_NAME "calcyx")

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_cli dep_mpdecimal)
endif()

install(TARGETS calcyx_cli RUNTIME DESTINATION bin COMPONENT cli)

# ===== ctest 登録 =====

# -e モード: 基本評価
add_test(NAME cli/eval_basic COMMAND calcyx_cli -e "1+1")
set_tests_properties(cli/eval_basic PROPERTIES
    PASS_REGULAR_EXPRESSION "2"
    LABELS "cli")

# -o both モード
add_test(NAME cli/eval_both COMMAND calcyx_cli -o both -e "3*7")
set_tests_properties(cli/eval_both PROPERTIES
    PASS_REGULAR_EXPRESSION "3\\*7 = 21"
    LABELS "cli")

# セミコロンコメント除去
add_test(NAME cli/comment COMMAND calcyx_cli -e "1+1 ; これはコメント")
set_tests_properties(cli/comment PROPERTIES
    PASS_REGULAR_EXPRESSION "2"
    LABELS "cli")

# 複数 -e: 文脈共有
add_test(NAME cli/multi_e COMMAND calcyx_cli -e "x=5" -e "x*2")
set_tests_properties(cli/multi_e PROPERTIES
    PASS_REGULAR_EXPRESSION "10"
    LABELS "cli")

# ファイルモード
add_test(NAME cli/file_mode
    COMMAND calcyx_cli "${CMAKE_CURRENT_SOURCE_DIR}/testdata/basic.txt")
set_tests_properties(cli/file_mode PROPERTIES
    PASS_REGULAR_EXPRESSION "0xFF"
    LABELS "cli")

# エラー時の終了コード 1
add_test(NAME cli/error_exit COMMAND calcyx_cli -e "1++")
set_tests_properties(cli/error_exit PROPERTIES
    WILL_FAIL TRUE
    LABELS "cli")
