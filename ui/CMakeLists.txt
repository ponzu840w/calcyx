cmake_minimum_required(VERSION 3.15)

# ---- FLTK (DEPS_DIR は親の CMakeLists.txt で deps.cmake により設定済み) ----
set(FLTK_LIB        "${DEPS_DIR}/lib/libfltk.a")
set(FLTK_INCLUDE_DIR "${DEPS_DIR}/include")

message(STATUS "FLTK: ${FLTK_LIB}")

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
    find_package(X11 REQUIRED)
    list(APPEND PLATFORM_LIBS
        pthread
        ${X11_LIBRARIES}
        Xfixes Xrender Xcursor Xft Xinerama fontconfig
    )
endif()

# ---- calcyx (App Bundle on macOS, 通常バイナリ on Windows/Linux) ----
if(APPLE)
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/icon.icns")
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    file(GLOB SAMPLE_FILES "${CMAKE_SOURCE_DIR}/samples/*.txt")
    set_source_files_properties(${SAMPLE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/samples")

    add_executable(calcyx_ui MACOSX_BUNDLE
        main.cpp
        MainWindow.cpp
        SheetView.cpp
        CompletionPopup.cpp
        PasteOptionForm.cpp
        builtin_docs.cpp
        app_prefs.cpp
        ${ICON_FILE}
        ${SAMPLE_FILES}
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
        CompletionPopup.cpp
        PasteOptionForm.cpp
        builtin_docs.cpp
        app_prefs.cpp
        calcyx.rc
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx")
    # exe の隣に samples/ をコピー (File > Examples が参照する)
    add_custom_command(TARGET calcyx_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/samples"
            "$<TARGET_FILE_DIR:calcyx_ui>/samples"
    )
else()
    add_executable(calcyx_ui
        main.cpp
        MainWindow.cpp
        SheetView.cpp
        CompletionPopup.cpp
        PasteOptionForm.cpp
        builtin_docs.cpp
        app_prefs.cpp
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx")
    # Linux: exe の隣に samples/ をコピー
    add_custom_command(TARGET calcyx_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/samples"
            "$<TARGET_FILE_DIR:calcyx_ui>/samples"
    )
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
    CompletionPopup.cpp
    PasteOptionForm.cpp
    builtin_docs.cpp
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
