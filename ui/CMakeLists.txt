cmake_minimum_required(VERSION 3.15)

# ---- FLTK の検索 ----
if(WIN32 AND CMAKE_CROSSCOMPILING)
    set(MINGW_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/mingw64")
    set(FLTK_LIB        "${MINGW_DEPS_DIR}/lib/libfltk.a")
    set(FLTK_INCLUDE_DIR "${MINGW_DEPS_DIR}/include")
else()
    find_library(FLTK_LIB fltk
        HINTS
            /opt/homebrew/lib
            /usr/local/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
    )
    find_path(FLTK_INCLUDE_DIR FL/Fl.H
        HINTS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )
    if(NOT FLTK_LIB OR NOT FLTK_INCLUDE_DIR)
        message(STATUS "FLTK not found — skipping UI target")
        return()
    endif()
endif()

message(STATUS "Found FLTK: ${FLTK_LIB}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- プラットフォーム別リンクフラグ ----
set(PLATFORM_LIBS "")
if(APPLE)
    list(APPEND PLATFORM_LIBS
        "-framework Cocoa"
        "-weak_framework UniformTypeIdentifiers"
        "-weak_framework ScreenCaptureKit"
    )
elseif(WIN32)
    list(APPEND PLATFORM_LIBS
        comctl32 gdi32 ole32 uuid ws2_32 imm32 gdiplus
        -static -static-libgcc -static-libstdc++
    )
else()
    # Linux
    list(APPEND PLATFORM_LIBS pthread)
    find_package(X11)
    if(X11_FOUND)
        list(APPEND PLATFORM_LIBS ${X11_LIBRARIES})
    endif()
endif()

# ---- calcyx (App Bundle on macOS, 通常バイナリ on Windows/Linux) ----
if(APPLE)
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/icon.icns")
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(calcyx_ui MACOSX_BUNDLE
        main.cpp
        MainWindow.cpp
        SheetView.cpp
        ${ICON_FILE}
    )
    set_target_properties(calcyx_ui PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME          "calcyx"
        MACOSX_BUNDLE_BUNDLE_VERSION       "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_ICON_FILE            "icon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.ponzu840w.calcyx"
        OUTPUT_NAME                        "calcyx"
    )
elseif(WIN32)
    add_executable(calcyx_ui WIN32
        main.cpp
        MainWindow.cpp
        SheetView.cpp
        calcyx.rc
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx")
else()
    add_executable(calcyx_ui
        main.cpp
        MainWindow.cpp
        SheetView.cpp
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx")
endif()

target_include_directories(calcyx_ui PRIVATE
    ${FLTK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${MPDECIMAL_INCLUDE}
)

target_link_libraries(calcyx_ui PRIVATE
    calcyx_engine
    ${FLTK_LIB}
    ${PLATFORM_LIBS}
    m
)

# ExternalProject のビルド完了を待ってからリンクする
if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_ui dep_mpdecimal)
endif()
if(TARGET dep_fltk)
    add_dependencies(calcyx_ui dep_fltk)
endif()

# ---- test_undo ----
add_executable(test_undo
    test_undo.cpp
    SheetView.cpp
)

target_include_directories(test_undo PRIVATE
    ${FLTK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${MPDECIMAL_INCLUDE}
)

target_link_libraries(test_undo PRIVATE
    calcyx_engine
    ${FLTK_LIB}
    ${PLATFORM_LIBS}
    m
)

if(TARGET dep_mpdecimal)
    add_dependencies(test_undo dep_mpdecimal)
endif()
if(TARGET dep_fltk)
    add_dependencies(test_undo dep_fltk)
endif()
