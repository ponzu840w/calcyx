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

add_executable(calcyx_ui
    main.cpp
    MainWindow.cpp
    SheetView.cpp
)

target_include_directories(calcyx_ui PRIVATE
    ${FLTK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${MPDECIMAL_INCLUDE}
)

set(FLTK_FRAMEWORKS "")
if(APPLE)
    list(APPEND FLTK_FRAMEWORKS
        "-framework Cocoa"
        "-weak_framework UniformTypeIdentifiers"
        "-weak_framework ScreenCaptureKit"
    )
endif()

target_link_libraries(calcyx_ui PRIVATE
    calcyx_engine
    ${FLTK_LIB}
    ${FLTK_FRAMEWORKS}
    m
    pthread
)
