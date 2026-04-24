cmake_minimum_required(VERSION 3.15)

# ---- FTXUI (DEPS_DIR は親の CMakeLists.txt で deps.cmake により設定済み) ----
set(FTXUI_INCLUDE_DIR   "${DEPS_DIR}/include")
set(FTXUI_COMPONENT_LIB "${DEPS_DIR}/lib/libftxui-component.a")
set(FTXUI_DOM_LIB       "${DEPS_DIR}/lib/libftxui-dom.a")
set(FTXUI_SCREEN_LIB    "${DEPS_DIR}/lib/libftxui-screen.a")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FTXUI の静的ライブラリはリンク順が dom → screen → component であっても
# 参照が残ることがあるため、--start-group / --end-group は使わず全リストを
# 明示的に並べる (MinGW の ld も対応)。

set(TUI_SRC
    main.cpp
    TuiApp.cpp
    TuiSheet.cpp
    TuiCompletion.cpp
    keymap.cpp
)

if(WIN32)
    add_executable(calcyx_tui ${TUI_SRC} calcyx.rc)
else()
    add_executable(calcyx_tui ${TUI_SRC})
endif()

set_target_properties(calcyx_tui PROPERTIES OUTPUT_NAME "calcyx-tui")

target_include_directories(calcyx_tui PRIVATE
    ${FTXUI_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(calcyx_tui PRIVATE
    CALCYX_VERSION_FULL="${CALCYX_VERSION_FULL}"
    CALCYX_EDITION="${CALCYX_PLATFORM} TUI Edition"
)

target_link_libraries(calcyx_tui PRIVATE
    calcyx_engine
    ${FTXUI_COMPONENT_LIB}
    ${FTXUI_DOM_LIB}
    ${FTXUI_SCREEN_LIB}
)

if(WIN32)
    target_link_libraries(calcyx_tui PRIVATE
        -static -static-libgcc -static-libstdc++
    )
endif()

if(TARGET dep_ftxui)
    add_dependencies(calcyx_tui dep_ftxui)
endif()
if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_tui dep_mpdecimal)
endif()

install(TARGETS calcyx_tui RUNTIME DESTINATION bin COMPONENT tui)

# ---- テスト ----
# tui は端末も ScreenInteractive も使わず、OnEvent を直接叩いて state を検証する
# end-to-end シナリオテスト (描画結果は stderr にダンプのみ、比較はしない)。
set(TUI_TEST_COMMON_SRC
    TuiApp.cpp
    TuiSheet.cpp
    TuiCompletion.cpp
    keymap.cpp
)

add_executable(test_tui_sheet
    test_tui_sheet.cpp
    ${TUI_TEST_COMMON_SRC}
)
target_include_directories(test_tui_sheet PRIVATE
    ${FTXUI_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_tui_sheet PRIVATE
    calcyx_engine
    ${FTXUI_COMPONENT_LIB}
    ${FTXUI_DOM_LIB}
    ${FTXUI_SCREEN_LIB}
)

add_executable(test_tui_app
    test_tui_app.cpp
    ${TUI_TEST_COMMON_SRC}
)
target_include_directories(test_tui_app PRIVATE
    ${FTXUI_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_tui_app PRIVATE
    calcyx_engine
    ${FTXUI_COMPONENT_LIB}
    ${FTXUI_DOM_LIB}
    ${FTXUI_SCREEN_LIB}
)

if(WIN32)
    target_link_libraries(test_tui_sheet PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(test_tui_app   PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(TARGET dep_ftxui)
    add_dependencies(test_tui_sheet dep_ftxui)
    add_dependencies(test_tui_app   dep_ftxui)
endif()

if(CALCYX_TESTS_ENABLED)
    add_test(NAME tui/sheet
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_tui_sheet>)
    set_tests_properties(tui/sheet PROPERTIES LABELS "tui")

    add_test(NAME tui/app
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_tui_app>)
    set_tests_properties(tui/app PROPERTIES LABELS "tui")
endif()
