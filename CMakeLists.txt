cmake_minimum_required(VERSION 3.25)

# ---- バージョン (git tag から取得) ----
# リリースタグの形式: v1.2.3
execute_process(
    COMMAND git describe --tags --match "v*" --abbrev=0
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE _git_tag
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _git_result
)
if(_git_result EQUAL 0 AND _git_tag MATCHES "^v[0-9]")
    string(REGEX REPLACE "^v" "" _tag_no_v "${_git_tag}")
    # project(VERSION) は数値のみ受け付けるので major.minor.patch を抽出
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CALCYX_VERSION "${_tag_no_v}")
    # パッケージファイル名にはタグの完全な文字列を使う (例: 0.2.0-alpha)
    set(CALCYX_VERSION_FULL "${_tag_no_v}")
else()
    set(CALCYX_VERSION      "0.0.0")
    set(CALCYX_VERSION_FULL "0.0.0")
endif()

# HEAD がタグ上にない場合は -dev を付加 (パッケージファイル名用)
execute_process(
    COMMAND git describe --tags --match "v*" --exact-match
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    ERROR_QUIET
    RESULT_VARIABLE _exact_result
)
if(NOT _exact_result EQUAL 0)
    set(CALCYX_VERSION_FULL "${CALCYX_VERSION_FULL}-dev")
endif()

project(calcyx VERSION "${CALCYX_VERSION}" LANGUAGES C CXX)
message(STATUS "calcyx version: ${CALCYX_VERSION_FULL}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- 依存ライブラリ (mpdecimal, FLTK) ----
# WASM: Emscripten 専用の deps (mpdecimal のみ、FLTK なし)
# その他: ソースから自動ビルド (deps/ 以下に配置)
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

    # ---- samples と LICENSE のインストール ----
    if(APPLE)
        # macOS: .app バンドル内に Resources/samples として埋め込み済み (ui/CMakeLists.txt)
        # LICENSE は zip のルートに置く
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
            DESTINATION . COMPONENT gui)
    elseif(WIN32)
        # Windows: zip 用
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/samples/"
            DESTINATION samples COMPONENT gui)
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
            DESTINATION . COMPONENT gui)
    else()
        # Linux: /usr/share/calcyx/ 以下に配置
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/samples/"
            DESTINATION share/calcyx/samples COMPONENT gui)
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
            DESTINATION share/doc/calcyx COMPONENT gui)
    endif()

    # ---- CPack ----
    set(CPACK_PACKAGE_NAME                "calcyx")
    set(CPACK_PACKAGE_VERSION             "${CALCYX_VERSION}")
    set(CPACK_PACKAGE_VENDOR              "ponzu840w")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Programmers' calculator")
    set(CPACK_STRIP_FILES                 TRUE)

    if(APPLE)
        set(CPACK_GENERATOR         "ZIP")
        set(CPACK_PACKAGE_FILE_NAME "calcyx-mac-${CALCYX_VERSION_FULL}")
    elseif(WIN32)
        set(CPACK_GENERATOR         "ZIP")
        set(CPACK_PACKAGE_FILE_NAME "calcyx-win-${CALCYX_VERSION_FULL}")
    else()
        # Linux: calcyx_0.3.0_amd64.deb (CPack DEB のデフォルト命名)
        set(CPACK_GENERATOR                 "DEB")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ponzu840w")
        set(CPACK_DEBIAN_PACKAGE_SECTION    "utils")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  TRUE)
    endif()

    include(CPack)
endif()
