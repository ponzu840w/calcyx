# Windows 向け依存ライブラリ (mpdecimal, FLTK) の自動取得・クロスビルド
# CMakeLists.txt から WIN32 クロスコンパイル時にのみ include される
#
# 初回 cmake --build 時にダウンロード・ビルド・インストールが走る。
# deps/mingw64/ にライブラリが揃っていれば何もしない。

include(ExternalProject)

set(MINGW_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/mingw64")

# ---- mpdecimal ----
if(NOT EXISTS "${MINGW_DEPS_DIR}/lib/libmpdec.a")
    ExternalProject_Add(dep_mpdecimal
        URL      https://www.bytereef.org/software/mpdecimal/releases/mpdecimal-4.0.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --host=x86_64-w64-mingw32
        BUILD_COMMAND     ${CMAKE_MAKE_PROGRAM} -j
        INSTALL_COMMAND   ${CMAKE_MAKE_PROGRAM} install prefix=${MINGW_DEPS_DIR}
                COMMAND   ${CMAKE_COMMAND} -E rm -f
                              ${MINGW_DEPS_DIR}/lib/libmpdec.dll.a
                              ${MINGW_DEPS_DIR}/lib/libmpdec++.dll.a
    )
endif()

# ---- FLTK (1.4.x 必須: insert_position() が 1.3.x にはない) ----
if(NOT EXISTS "${MINGW_DEPS_DIR}/lib/libfltk.a")
    ExternalProject_Add(dep_fltk
        URL https://github.com/fltk/fltk/releases/download/release-1.4.4/fltk-1.4.4-source.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CMAKE_ARGS
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_INSTALL_PREFIX=${MINGW_DEPS_DIR}
            -DFLTK_BUILD_TEST=OFF
            -DFLTK_BUILD_FLUID=OFF
            -DFLTK_BUILD_FLTK_OPTIONS=OFF
    )
endif()
