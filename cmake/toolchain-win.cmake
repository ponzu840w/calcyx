# MinGW-w64 クロスコンパイル用ツールチェーンファイル
# WSL (Ubuntu) 上で Windows x86_64 バイナリをビルドする
# 使い方: cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

# クロスコンパイル対象のルートパス
# deps/mingw64/ はプロジェクト内にビルドした依存ライブラリ (FLTK, mpdecimal) の配置先
set(MINGW_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/../deps/mingw64")
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 "${MINGW_DEPS_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# .a を .dll.a より優先して静的リンクにする (DLL 依存を避ける)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dll.a")
