cmake_minimum_required(VERSION 3.15)
project(calcyx C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- mpdecimal ----
# Mac (Homebrew), Linux (system), Windows (vcpkg or manual)
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
        "  macOS:   brew install mpdecimal\n"
        "  Linux:   apt install libmpdec-dev  (または相当)\n"
        "  Windows: vcpkg install mpdecimal  (または手動ビルド)\n"
    )
endif()

add_subdirectory(engine)
add_subdirectory(ui)
