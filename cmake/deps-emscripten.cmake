# cmake/deps-emscripten.cmake
# Emscripten ビルド用の依存ライブラリ (mpdecimal のみ; FLTK 不要)
# CMakeLists.txt から EMSCRIPTEN 時に include される。

include(ExternalProject)

set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/emscripten")

if(NOT EXISTS "${DEPS_DIR}/lib/libmpdec.a")
    ExternalProject_Add(dep_mpdecimal
        URL      https://www.bytereef.org/software/mpdecimal/releases/mpdecimal-4.0.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND emconfigure <SOURCE_DIR>/configure
        BUILD_COMMAND     emmake ${CMAKE_MAKE_PROGRAM} -j
        INSTALL_COMMAND   emmake ${CMAKE_MAKE_PROGRAM} install prefix=${DEPS_DIR}
                COMMAND   ${CMAKE_COMMAND} -E rm -f
                              "${DEPS_DIR}/lib/libmpdec.so"
                              "${DEPS_DIR}/lib/libmpdec++.so"
    )
endif()
