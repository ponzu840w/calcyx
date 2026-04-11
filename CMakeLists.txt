cmake_minimum_required(VERSION 3.15)
project(calcyx C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- mpdecimal ----
if(WIN32 AND CMAKE_CROSSCOMPILING)
    # deps/mingw64/ へのクロスビルドを ExternalProject で自動化
    include(cmake/deps-mingw64.cmake)
    set(MINGW_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/mingw64")
    set(MPDECIMAL_LIB     "${MINGW_DEPS_DIR}/lib/libmpdec.a")
    set(MPDECIMAL_INCLUDE "${MINGW_DEPS_DIR}/include")
else()
    find_library(MPDECIMAL_LIB mpdec
        HINTS
            /opt/homebrew/lib        # macOS Homebrew (Apple Silicon)
            /usr/local/lib           # macOS Homebrew (Intel) / Linux
            /usr/lib
            /usr/lib/x86_64-linux-gnu
    )
    find_path(MPDECIMAL_INCLUDE mpdecimal.h
        HINTS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )
    if(NOT MPDECIMAL_LIB OR NOT MPDECIMAL_INCLUDE)
        message(FATAL_ERROR
            "mpdecimal not found.\n"
            "  macOS: brew install mpdecimal\n"
            "  Linux: apt install libmpdec-dev\n"
        )
    endif()
endif()

add_subdirectory(engine)
add_subdirectory(ui)
