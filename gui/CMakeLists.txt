cmake_minimum_required(VERSION 3.15)

# ---- FLTK (DEPS_DIR は親の CMakeLists.txt で deps.cmake により設定済み) ----
set(FLTK_LIB         "${DEPS_DIR}/lib/libfltk.a")
set(FLTK_IMAGES_LIB  "${DEPS_DIR}/lib/libfltk_images.a")
set(FLTK_PNG_LIB     "${DEPS_DIR}/lib/libfltk_png.a")
set(FLTK_Z_LIB       "${DEPS_DIR}/lib/libfltk_z.a")
set(FLTK_INCLUDE_DIR "${DEPS_DIR}/include")

message(STATUS "FLTK: ${FLTK_LIB}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- プラットフォーム別リンクフラグ ----
# ---- プラットフォーム別トレイ/ホットキーソース ----
# platform_tray_common.cpp はキー名テーブルと変換関数を提供し、
# プラットフォーム固有実装の各ファイルと一緒にリンクされる。
if(APPLE)
    set(PLATFORM_TRAY_SRC platform_tray_mac.mm)
elseif(WIN32)
    set(PLATFORM_TRAY_SRC platform_tray_win.cpp)
elseif(EMSCRIPTEN)
    set(PLATFORM_TRAY_SRC platform_tray_stub.cpp)
else()
    set(PLATFORM_TRAY_SRC platform_tray_x11.cpp)
endif()

set(PLATFORM_LIBS "")
if(APPLE)
    list(APPEND PLATFORM_LIBS
        "-framework Cocoa"
        "-framework Carbon"
        "-weak_framework UniformTypeIdentifiers"
        "-weak_framework ScreenCaptureKit"
    )
elseif(WIN32)
    list(APPEND PLATFORM_LIBS
        comctl32 shell32 gdi32 ole32 uuid ws2_32 imm32 gdiplus dbghelp
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

# ---- calcyx_ui_common (calcyx_ui と test_undo が共有する静的ライブラリ) ----
# test_undo は MainWindow / main / platform_tray_<os> は使わないため含めない。
# platform_tray_common.cpp はキー名テーブルのみを提供し、stub/x11/win/mac の
# どれとでも共存できるので common に置く。
add_library(calcyx_ui_common STATIC
    colors.cpp
    SheetView.cpp
    CompletionPopup.cpp
    PasteOptionForm.cpp
    PrefsDialog.cpp
    prefs/prefs_common.cpp
    prefs/tab_general.cpp
    prefs/tab_appearance.cpp
    prefs/tab_input.cpp
    prefs/tab_number_format.cpp
    prefs/tab_calculation.cpp
    settings_globals.cpp
    AppSettings.cpp
    app_prefs.cpp
    crash_handler.cpp
    platform_tray_common.cpp
)

target_include_directories(calcyx_ui_common PUBLIC
    ${FLTK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# crash_handler.cpp が CALCYX_VERSION_FULL を参照する
target_compile_definitions(calcyx_ui_common PRIVATE
    CALCYX_VERSION_FULL="${CALCYX_VERSION_FULL}"
)

# 依存ライブラリは PUBLIC で公開し、リンク側で再指定不要にする
target_link_libraries(calcyx_ui_common PUBLIC
    calcyx_engine
)

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_ui_common dep_mpdecimal)
endif()
if(TARGET dep_fltk)
    add_dependencies(calcyx_ui_common dep_fltk)
endif()

# ---- calcyx (App Bundle on macOS, 通常バイナリ on Windows/Linux) ----
if(APPLE)
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/icon.icns")
    set(ICON_SVG  "${CMAKE_CURRENT_SOURCE_DIR}/icon.svg")
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_source_files_properties(${ICON_SVG}  PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    file(GLOB SAMPLE_FILES "${CMAKE_SOURCE_DIR}/samples/*.txt")
    set_source_files_properties(${SAMPLE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/samples")

    add_executable(calcyx_ui MACOSX_BUNDLE
        main.cpp
        MainWindow.cpp
        platform_mac.mm
        ${PLATFORM_TRAY_SRC}
        ${ICON_FILE}
        ${ICON_SVG}
        ${SAMPLE_FILES}
    )
    set_target_properties(calcyx_ui PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME          "calcyx"
        MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE            "icon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.ponzu840w.calcyx"
        OUTPUT_NAME                        "calcyx"
    )
elseif(WIN32)
    add_executable(calcyx_ui WIN32
        main.cpp
        MainWindow.cpp
        ${PLATFORM_TRAY_SRC}
        calcyx.rc
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx-gui")
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
        ${PLATFORM_TRAY_SRC}
    )
    set_target_properties(calcyx_ui PROPERTIES OUTPUT_NAME "calcyx-gui")
    # Linux: exe の隣に samples/ をコピー
    add_custom_command(TARGET calcyx_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/samples"
            "$<TARGET_FILE_DIR:calcyx_ui>/samples"
    )
endif()

target_compile_definitions(calcyx_ui PRIVATE
    CALCYX_VERSION_FULL="${CALCYX_VERSION_FULL}"
    CALCYX_EDITION="${CALCYX_PLATFORM} GUI Edition"
)
if(NOT APPLE AND NOT WIN32)
    target_compile_definitions(calcyx_ui PRIVATE
        CALCYX_ICON_PNG="${CMAKE_CURRENT_SOURCE_DIR}/icon_128.png"
    )
endif()

target_link_libraries(calcyx_ui PRIVATE
    calcyx_ui_common
    ${FLTK_IMAGES_LIB}
    ${FLTK_PNG_LIB}
    ${FLTK_Z_LIB}
    ${FLTK_LIB}
    ${PLATFORM_LIBS}
)

if(APPLE)
    install(TARGETS calcyx_ui BUNDLE DESTINATION . COMPONENT gui)
elseif(WIN32)
    install(TARGETS calcyx_ui RUNTIME DESTINATION bin COMPONENT gui)
elseif(UNIX)
    install(TARGETS calcyx_ui RUNTIME DESTINATION bin COMPONENT gui)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/com.ponzu840w.calcyx.desktop"
        DESTINATION share/applications COMPONENT gui)
    install(FILES "${CMAKE_SOURCE_DIR}/gui/icon.svg"
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME calcyx.svg COMPONENT gui)
endif()

# ---- test_completion ----
# shared/completion_match.c の pure function テスト。FLTK に依存しない。
# completion_match 本体は calcyx_engine に含まれるので engine リンクのみで足りる。
add_executable(test_completion
    test_completion.cpp
)
target_link_libraries(test_completion PRIVATE calcyx_engine)

# ---- test_settings ----
# settings スキーマテーブル往復テスト。K_FONT エントリが FLTK を要求する
# ため test_undo と同じく calcyx_ui_common + platform_tray_stub でリンクする。
add_executable(test_settings
    test_settings.cpp
    platform_tray_stub.cpp
)
target_link_libraries(test_settings PRIVATE
    calcyx_ui_common
    ${FLTK_LIB}
    ${PLATFORM_LIBS}
)

# ===== ctest 登録 =====
if(CALCYX_TESTS_ENABLED)
    add_test(NAME gui/completion
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_completion>)
    set_tests_properties(gui/completion PROPERTIES LABELS "gui")

    add_test(NAME gui/settings
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_settings>)
    set_tests_properties(gui/settings PROPERTIES LABELS "gui")
endif()
