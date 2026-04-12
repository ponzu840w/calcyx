cmake_minimum_required(VERSION 3.15)
project(calcyx C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- 依存ライブラリ (FLTK, mpdecimal) ----
# 全プラットフォームともソースから自動ビルド (deps/ 以下に配置)
include(cmake/deps.cmake)

set(MPDECIMAL_LIB     "${DEPS_DIR}/lib/libmpdec.a")
set(MPDECIMAL_INCLUDE "${DEPS_DIR}/include")

enable_testing()

add_subdirectory(engine)
add_subdirectory(ui)
add_subdirectory(cli)
