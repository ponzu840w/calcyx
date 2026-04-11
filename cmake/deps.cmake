# 依存ライブラリ (FLTK, mpdecimal) の自動取得・ビルド
# 全プラットフォーム共通。CMakeLists.txt から include される。
#
# インストール先:
#   deps/native/   — macOS / Linux ネイティブビルド
#   deps/mingw64/  — Windows クロスビルド (WSL MinGW-w64)

include(ExternalProject)

if(WIN32 AND CMAKE_CROSSCOMPILING)
    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/mingw64")
else()
    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/native")
endif()

# ---- mpdecimal ----
if(NOT EXISTS "${DEPS_DIR}/lib/libmpdec.a")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_mpdec_host --host=x86_64-w64-mingw32)
    else()
        set(_mpdec_host "")
    endif()

    ExternalProject_Add(dep_mpdecimal
        URL      https://www.bytereef.org/software/mpdecimal/releases/mpdecimal-4.0.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure ${_mpdec_host}
        BUILD_COMMAND     ${CMAKE_MAKE_PROGRAM} -j
        INSTALL_COMMAND   ${CMAKE_MAKE_PROGRAM} install prefix=${DEPS_DIR}
                COMMAND   ${CMAKE_COMMAND} -E rm -f
                              "${DEPS_DIR}/lib/libmpdec.dll.a"
                              "${DEPS_DIR}/lib/libmpdec++.dll.a"
    )
endif()

# ---- FLTK (1.4.x 必須: insert_position() が 1.3.x にはない) ----
if(NOT EXISTS "${DEPS_DIR}/lib/libfltk.a")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_fltk_toolchain -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    else()
        set(_fltk_toolchain "")
    endif()

    ExternalProject_Add(dep_fltk
        URL https://github.com/fltk/fltk/releases/download/release-1.4.4/fltk-1.4.4-source.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CMAKE_ARGS
            ${_fltk_toolchain}
            -DCMAKE_INSTALL_PREFIX=${DEPS_DIR}
            -DFLTK_BUILD_TEST=OFF
            -DFLTK_BUILD_FLUID=OFF
            -DFLTK_BUILD_FLTK_OPTIONS=OFF
    )
endif()
