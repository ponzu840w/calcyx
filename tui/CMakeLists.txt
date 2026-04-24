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
