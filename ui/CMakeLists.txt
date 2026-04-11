cmake_minimum_required(VERSION 3.15)

# FLTK を手動で検索（Homebrew / システム）
find_library(FLTK_LIB fltk HINTS /opt/homebrew/lib /usr/local/lib)
find_path(FLTK_INCLUDE_DIR FL/Fl.H HINTS /opt/homebrew/include /usr/local/include)

if(NOT FLTK_LIB OR NOT FLTK_INCLUDE_DIR)
    message(STATUS "FLTK not found — skipping UI target")
    return()
endif()

message(STATUS "Found FLTK: ${FLTK_LIB}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FLTK_FRAMEWORKS "")
if(APPLE)
    list(APPEND FLTK_FRAMEWORKS
        "-framework Cocoa"
        "-weak_framework UniformTypeIdentifiers"
        "-weak_framework ScreenCaptureKit"
    )
endif()

# ---- calcyx (App Bundle on macOS) ----
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
else()
    add_executable(calcyx_ui
        main.cpp
        MainWindow.cpp
        SheetView.cpp
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
    ${FLTK_FRAMEWORKS}
    m
    pthread
)

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
    ${FLTK_FRAMEWORKS}
    m
    pthread
)
