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
set(FTXUI_VERSION     "5.0.0")

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
# Linux で Xft/fontconfig が欠けていると FLTK が silently XLFD にフォールバックするため早期失敗させる。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    find_package(Fontconfig REQUIRED)
    find_package(X11 REQUIRED COMPONENTS Xft)
endif()

# patch-fltk.py が変わったら stamp / PREFIX も変わってキャッシュが自動的に
# 無効化されるよう、パッチスクリプトの内容ハッシュを両者に埋め込む。
# CMAKE_CONFIGURE_DEPENDS に登録して、スクリプト更新時に cmake 再構成が
# 自動で走るようにする。
set(_fltk_patch "${CMAKE_SOURCE_DIR}/cmake/patch-fltk.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_fltk_patch}")
file(SHA256 "${_fltk_patch}" _fltk_patch_hash)
string(SUBSTRING "${_fltk_patch_hash}" 0 12 _fltk_patch_tag)

set(_fltk_stamp "${DEPS_DIR}/lib/libfltk-${FLTK_VERSION}-${_fltk_patch_tag}.a.stamp")
if(NOT EXISTS "${_fltk_stamp}")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_fltk_toolchain -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    else()
        set(_fltk_toolchain "")
    endif()

    include(ProcessorCount)
    ProcessorCount(_nproc)
    if(_nproc EQUAL 0)
        set(_nproc 4)
    endif()

    # PREFIX にハッシュを含めることで、新しいパッチでは src/build/stamp が
    # 別ディレクトリになり、ExternalProject 内部の stamp 経由でスキップされず
    # 確実にパッチ適用から再実行される。
    ExternalProject_Add(dep_fltk
        PREFIX "dep_fltk-${_fltk_patch_tag}"
        URL      https://github.com/fltk/fltk/releases/download/release-${FLTK_VERSION}/fltk-${FLTK_VERSION}-source.tar.gz
        URL_HASH SHA256=94b464cce634182c8407adac1be5fc49678986ca93285699b444352af89b4efe
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND ${CMAKE_COMMAND} -E env python3 ${_fltk_patch} <SOURCE_DIR>
        CMAKE_ARGS
            ${_fltk_toolchain}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${DEPS_DIR}
            -DFLTK_BUILD_TEST=OFF
            -DFLTK_BUILD_FLUID=OFF
            -DFLTK_BUILD_FLTK_OPTIONS=OFF
            -DFLTK_USE_SYSTEM_ZLIB=OFF
            -DFLTK_USE_SYSTEM_LIBPNG=OFF
            -DFLTK_USE_SYSTEM_LIBJPEG=OFF
        BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR> -j ${_nproc}
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
                COMMAND   ${CMAKE_COMMAND} -E touch "${_fltk_stamp}"
    )
endif()

# ---- FTXUI (TUI 用; screen / dom / component の静的ライブラリ) ----
# FLTK と同じパターン: stamp が無ければ ExternalProject_Add でビルド・インストール。
# install target は ftxui::{screen,dom,component} の CMake config を
# ${DEPS_DIR}/lib/cmake/ftxui/ に書き出すが、calcyx は -l 直指定でリンクするので
# find_package は呼ばない (ネイティブ / mingw の両方で同じ扱いにできる)。
# stamp サフィックス -p2: tui/ftxui_calcyx_input.patch を適用したリビジョン。
# (1) 0x18 (CAN) / 0x1A (SUB) を DROP せず SPECIAL として通す (Ctrl+Z/X 用)。
# (2) X10 マウス形式 (\e[M + 3 生バイト) を ParseCSI で消費する
#     (SGR モードを無視する端末でマウス移動が text 入力として漏れるバグ修正)。
# パッチを更新したらサフィックスをインクリメントしてリビルドを誘発する。
set(_ftxui_patch  "${CMAKE_CURRENT_SOURCE_DIR}/tui/ftxui_calcyx_input.patch")
set(_ftxui_stamp  "${DEPS_DIR}/lib/libftxui-${FTXUI_VERSION}-p2.a.stamp")
if(NOT EXISTS "${_ftxui_stamp}")
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        set(_ftxui_toolchain -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    else()
        set(_ftxui_toolchain "")
    endif()

    include(ProcessorCount)
    ProcessorCount(_nproc_ftxui)
    if(_nproc_ftxui EQUAL 0)
        set(_nproc_ftxui 4)
    endif()

    ExternalProject_Add(dep_ftxui
        URL      https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v${FTXUI_VERSION}.tar.gz
        URL_HASH SHA256=a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND     patch -p1 -N --silent -i "${_ftxui_patch}" ||
                          ${CMAKE_COMMAND} -E echo "ftxui patch already applied"
        CMAKE_ARGS
            ${_ftxui_toolchain}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${DEPS_DIR}
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5
            -DFTXUI_BUILD_EXAMPLES=OFF
            -DFTXUI_BUILD_DOCS=OFF
            -DFTXUI_BUILD_TESTS=OFF
            -DFTXUI_ENABLE_INSTALL=ON
            -DFTXUI_QUIET=ON
            -DBUILD_SHARED_LIBS=OFF
        BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR> -j ${_nproc_ftxui}
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
                COMMAND   ${CMAKE_COMMAND} -E touch "${_ftxui_stamp}"
    )
endif()
