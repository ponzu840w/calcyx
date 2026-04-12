cmake_minimum_required(VERSION 3.15)
project(calcyx C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- 依存ライブラリ (mpdecimal, FLTK) ----
if(EMSCRIPTEN)
    include(cmake/deps-emscripten.cmake)
else()
    include(cmake/deps.cmake)
endif()

set(MPDECIMAL_LIB     "${DEPS_DIR}/lib/libmpdec.a")
set(MPDECIMAL_INCLUDE "${DEPS_DIR}/include")

enable_testing()

add_subdirectory(engine)

if(EMSCRIPTEN)
    add_subdirectory(web)
else()
    add_subdirectory(ui)
    add_subdirectory(cli)
endif()
