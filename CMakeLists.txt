cmake_minimum_required(VERSION 3.15)
project(calcyx C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# mpdecimal: Mac では Homebrew、他プラットフォームはシステムか手動指定
find_library(MPDECIMAL_LIB mpdec HINTS /opt/homebrew/lib)
find_path(MPDECIMAL_INCLUDE mpdecimal.h HINTS /opt/homebrew/include)
if(NOT MPDECIMAL_LIB OR NOT MPDECIMAL_INCLUDE)
    message(FATAL_ERROR "mpdecimal not found. Install via: brew install mpdecimal")
endif()

add_subdirectory(engine)
add_subdirectory(ui)
