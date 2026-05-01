cmake_minimum_required(VERSION 3.25)

# ---- バージョン (git tag から取得) ----
# リリースタグの形式: v1.2.3
# タグ上: v0.2.0-alpha → 0.2.0-alpha
# タグなし: v0.2.0-alpha の 3 コミット後 → 0.2.0-alpha-dev-3-gabcdef1
execute_process(
    COMMAND git describe --tags --match "v*" --long --dirty
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE _git_describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _git_result
)
if(_git_result EQUAL 0 AND _git_describe MATCHES "^v[0-9]")
    # _git_describe の形式: v0.2.0-alpha-3-gabcdef1[-dirty]
    # 末尾の -dirty を分離
    set(_dirty "")
    if(_git_describe MATCHES "-dirty$")
        set(_dirty "-dirty")
        string(REGEX REPLACE "-dirty$" "" _git_describe "${_git_describe}")
    endif()
    # 末尾の -<n>-g<hash> を分離する
    string(REGEX MATCH "^(v.+)-([0-9]+)-g([0-9a-f]+)$" _m "${_git_describe}")
    set(_base_tag  "${CMAKE_MATCH_1}")   # v0.2.0-alpha
    set(_n_commits "${CMAKE_MATCH_2}")   # 3
    set(_hash      "${CMAKE_MATCH_3}")   # abcdef1

    string(REGEX REPLACE "^v" "" _tag_no_v "${_base_tag}")
    # project(VERSION) は数値のみ受け付けるので major.minor.patch を抽出
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CALCYX_VERSION "${_tag_no_v}")

    if(_n_commits EQUAL 0)
        # HEAD がタグ上: リリースビルド
        set(CALCYX_VERSION_FULL "${_tag_no_v}${_dirty}")
    else()
        # タグからコミットが進んでいる: 開発中
        set(CALCYX_VERSION_FULL "${_tag_no_v}-dev-${_n_commits}-g${_hash}${_dirty}")
    endif()
else()
    set(CALCYX_VERSION      "0.0.0")
    set(CALCYX_VERSION_FULL "0.0.0-dev")
endif()

project(calcyx VERSION "${CALCYX_VERSION}" LANGUAGES C CXX)
message(STATUS "calcyx version: ${CALCYX_VERSION_FULL}")

# ---- プラットフォーム名 ----
if(WIN32)
    set(CALCYX_PLATFORM "Windows")
elseif(APPLE)
    set(CALCYX_PLATFORM "macOS")
else()
    set(CALCYX_PLATFORM "Linux")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- バイナリサイズ最適化 (Release 時の LTO + gc-sections) ----
# Emscripten は独自に最適化フラグを制御するためスキップ。
# Debug/RelWithDebInfo では無効 (デバッグ性を優先)。
if(NOT EMSCRIPTEN AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
    if(_ipo_ok)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO (IPO) enabled for Release build")
    else()
        message(STATUS "LTO (IPO) not supported: ${_ipo_msg}")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(-ffunction-sections -fdata-sections)
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        else()
            add_link_options(-Wl,--gc-sections)
        endif()
    endif()
endif()

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

# FTXUI (tui サブディレクトリ + cli の統合バイナリが参照するため、ここで定義)
if(NOT EMSCRIPTEN)
    set(FTXUI_INCLUDE_DIR   "${DEPS_DIR}/include")
    set(FTXUI_COMPONENT_LIB "${DEPS_DIR}/lib/libftxui-component.a")
    set(FTXUI_DOM_LIB       "${DEPS_DIR}/lib/libftxui-dom.a")
    set(FTXUI_SCREEN_LIB    "${DEPS_DIR}/lib/libftxui-screen.a")
endif()

enable_testing()
include(cmake/test_runners.cmake)

# ---- shared/i18n_table.c の完全性チェック (ja に未訳・死蔵が無いか) ----
# WASM プリセットでは TUI/GUI を構築しないが、 翻訳辞書の整合性は
# クロスターゲットで共通なので unix / win プリセットでのみ走らせる。
if(CALCYX_TESTS_ENABLED AND NOT EMSCRIPTEN)
    add_test(NAME i18n/coverage
             COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_i18n.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(i18n/coverage PROPERTIES LABELS "i18n")
endif()

add_subdirectory(engine)

if(EMSCRIPTEN)
    add_subdirectory(web)

    # ---- CPack (WASM/Web 配布用 zip) ----
    set(CPACK_PACKAGE_NAME                "calcyx")
    set(CPACK_PACKAGE_VERSION             "${CALCYX_VERSION}")
    set(CPACK_PACKAGE_VENDOR              "ponzu840w")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Programmers' calculator (web)")
    set(CPACK_GENERATOR                   "ZIP")
    set(CPACK_PACKAGE_FILE_NAME           "calcyx-web-${CALCYX_VERSION_FULL}")
    set(CPACK_COMPONENTS_ALL              web)
    set(CPACK_ARCHIVE_COMPONENT_INSTALL   OFF)
    include(CPack)
else()
    # tui / cli を gui より先に追加: macOS で gui/ が POST_BUILD で
    # calcyx_cli を .app バンドル内に埋め込む際、 calcyx_cli が同じ
    # ディレクトリスコープで作成済みである必要があるため (CMake 3.29+ は
    # 別ディレクトリのターゲットへの add_custom_command を拒否する)。
    add_subdirectory(tui)
    add_subdirectory(cli)
    add_subdirectory(gui)

    # ---- samples / LICENSE / THIRD_PARTY_LICENSES のインストール ----
    if(APPLE)
        # macOS: .app バンドル内に Resources/samples として埋め込み済み (gui/CMakeLists.txt)
        # LICENSE と README_MAC は dmg のルートに置く (= drag-and-drop の前にユーザに見せる)。
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
                      "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt"
                      "${CMAKE_SOURCE_DIR}/README_MAC.txt"
            DESTINATION . COMPONENT gui)
    elseif(WIN32)
        # Windows: zip 用
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/samples/"
            DESTINATION samples COMPONENT gui)
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
                      "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt"
            DESTINATION . COMPONENT gui)
    else()
        # Linux: /usr/share/doc/calcyx/ 以下に配置
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/samples/"
            DESTINATION share/calcyx/samples COMPONENT gui)
        install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
                      "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt"
            DESTINATION share/doc/calcyx COMPONENT gui)
    endif()

    # ---- CPack ----
    set(CPACK_PACKAGE_NAME                "calcyx")
    set(CPACK_PACKAGE_VERSION             "${CALCYX_VERSION}")
    set(CPACK_PACKAGE_VENDOR              "ponzu840w")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Engineer's calculator")
    set(CPACK_PACKAGE_DESCRIPTION "calcyx is a scratchpad-style calculator for engineers and\n programmers. Expressions are entered one per line and re-\n evaluated on the fly; results from earlier lines are usable\n as values in later ones. Built-in functions cover hexadecimal,\n binary, bit operations, ECC, RGB colour conversions, and the\n usual scientific and engineering operators.\n .\n The program ships as a single binary (calcyx) that selects\n between a TUI and a CLI batch mode automatically, plus a\n separate FLTK-based GUI (calcyx-gui).")
    set(CPACK_PACKAGE_HOMEPAGE_URL        "https://github.com/ponzu840w/calcyx")
    set(CPACK_STRIP_FILES                 TRUE)

    if(APPLE)
        # macOS: .dmg を mount image として配布。 ユーザーが Applications
        # にドラッグするだけで使えるよう DragNDrop ジェネレータを使う。
        # CLI 統合バイナリは .app バンドル内 (Contents/MacOS/calcyx-cli)
        # に埋め込み済みなので CPACK_COMPONENTS_ALL=gui で gui コンポー
        # ネントのみに絞り、 dmg ルートに `bin/` や `share/` が裸で並ば
        # ないようにする。 cli を取り出したい人向けの案内は README_MAC.txt。
        set(CPACK_GENERATOR         "DragNDrop")
        set(CPACK_PACKAGE_FILE_NAME "calcyx-mac-${CALCYX_VERSION_FULL}")
        set(CPACK_DMG_VOLUME_NAME   "calcyx ${CALCYX_VERSION_FULL}")
        set(CPACK_COMPONENTS_ALL    gui)
        set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
    elseif(WIN32)
        set(CPACK_GENERATOR         "ZIP")
        set(CPACK_PACKAGE_FILE_NAME "calcyx-win-${CALCYX_VERSION_FULL}")
    else()
        # Linux: calcyx-linux-<version>.deb
        set(CPACK_GENERATOR                 "DEB")
        set(CPACK_PACKAGE_FILE_NAME         "calcyx-linux-${CALCYX_VERSION_FULL}")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ponzu840w <ponzu840w@yahoo.co.jp>")
        set(CPACK_DEBIAN_PACKAGE_SECTION    "utils")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE   "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  TRUE)
    endif()

    include(CPack)
endif()
