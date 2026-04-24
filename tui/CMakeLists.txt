cmake_minimum_required(VERSION 3.15)

# tui/ — FTXUI ベースの TUI コンポーネント。
# Phase 2 リファクタ (2026-04-24) で単独バイナリ calcyx-tui を廃止し、
# cli/ の calcyx に統合した。ここでは TUI ロジックを静的ライブラリ
# calcyx_tui_lib として提供し、cli/ とテストが共有する。
#
# FTXUI_* 変数は root CMakeLists.txt で定義済み。

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TUI_LIB_SRC
    TuiApp.cpp
    TuiSheet.cpp
    TuiCompletion.cpp
    keymap.cpp
)

add_library(calcyx_tui_lib STATIC ${TUI_LIB_SRC})

target_include_directories(calcyx_tui_lib PUBLIC
    ${FTXUI_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(calcyx_tui_lib PUBLIC
    calcyx_engine
    ${FTXUI_COMPONENT_LIB}
    ${FTXUI_DOM_LIB}
    ${FTXUI_SCREEN_LIB}
)

if(TARGET dep_ftxui)
    add_dependencies(calcyx_tui_lib dep_ftxui)
endif()
if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_tui_lib dep_mpdecimal)
endif()

# ---- テスト ----
# TUI は端末も ScreenInteractive も使わず、OnEvent を直接叩いて state を検証する
# end-to-end シナリオテスト (描画結果は stderr にダンプのみ、比較はしない)。
add_executable(test_tui_sheet test_tui_sheet.cpp)
target_link_libraries(test_tui_sheet PRIVATE calcyx_tui_lib)

add_executable(test_tui_app test_tui_app.cpp)
target_link_libraries(test_tui_app PRIVATE calcyx_tui_lib)

if(WIN32)
    target_link_libraries(test_tui_sheet PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(test_tui_app   PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(CALCYX_TESTS_ENABLED)
    add_test(NAME tui/sheet
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_tui_sheet>)
    set_tests_properties(tui/sheet PROPERTIES LABELS "tui")

    add_test(NAME tui/app
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_tui_app>)
    set_tests_properties(tui/app PROPERTIES LABELS "tui")
endif()
