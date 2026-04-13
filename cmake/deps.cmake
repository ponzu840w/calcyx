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

set(MPDECIMAL_VERSION "4.0.0")
set(FLTK_VERSION      "1.4.4")

# ---- mpdecimal ----
set(_mpdec_stamp "${DEPS_DIR}/lib/libmpdec-${MPDECIMAL_VERSION}.a.stamp")
if(NOT EXISTS "${_mpdec_stamp}")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_mpdec_host --host=x86_64-w64-mingw32)
    else()
        set(_mpdec_host "")
    endif()

    ExternalProject_Add(dep_mpdecimal
        URL      https://www.bytereef.org/software/mpdecimal/releases/mpdecimal-${MPDECIMAL_VERSION}.tar.gz
        URL_HASH SHA256=942445c3245b22730fd41a67a7c5c231d11cb1b9936b9c0f76334fb7d0b4468c
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure ${_mpdec_host}
        BUILD_COMMAND     ${CMAKE_MAKE_PROGRAM} -j
        INSTALL_COMMAND   ${CMAKE_MAKE_PROGRAM} install prefix=${DEPS_DIR}
                COMMAND   ${CMAKE_COMMAND} -E rm -f
                              "${DEPS_DIR}/lib/libmpdec.dll.a"
                              "${DEPS_DIR}/lib/libmpdec++.dll.a"
                COMMAND   ${CMAKE_COMMAND} -E touch "${_mpdec_stamp}"
    )
endif()

# ---- FLTK (1.4.x 必須: insert_position() が 1.3.x にはない) ----
set(_fltk_stamp "${DEPS_DIR}/lib/libfltk-${FLTK_VERSION}.a.stamp")
if(NOT EXISTS "${_fltk_stamp}")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_fltk_toolchain -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    else()
        set(_fltk_toolchain "")
    endif()

    ExternalProject_Add(dep_fltk
        URL      https://github.com/fltk/fltk/releases/download/release-${FLTK_VERSION}/fltk-${FLTK_VERSION}-source.tar.gz
        URL_HASH SHA256=94b464cce634182c8407adac1be5fc49678986ca93285699b444352af89b4efe
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CMAKE_ARGS
            ${_fltk_toolchain}
            -DCMAKE_INSTALL_PREFIX=${DEPS_DIR}
            -DFLTK_BUILD_TEST=OFF
            -DFLTK_BUILD_FLUID=OFF
            -DFLTK_BUILD_FLTK_OPTIONS=OFF
            # システム zlib/png/jpeg ではなく同梱版を使う。
            # Linux ネイティブでも libfltk_z.a 等が生成され、プラットフォーム間で
            # リンク構成が揃うため（SVG 描画のため fltk_images + fltk_z が必要）
            -DFLTK_USE_SYSTEM_ZLIB=OFF
            -DFLTK_USE_SYSTEM_LIBPNG=OFF
            -DFLTK_USE_SYSTEM_LIBJPEG=OFF
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
                COMMAND   ${CMAKE_COMMAND} -E touch "${_fltk_stamp}"
    )
endif()
