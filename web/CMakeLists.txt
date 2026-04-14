cmake_minimum_required(VERSION 3.15)

# Emscripten ビルド専用。非 WASM 環境では何もしない。
if(NOT EMSCRIPTEN)
    return()
endif()

set(CMAKE_C_STANDARD 99)

# WASM バイナリ (= calcyx.js + calcyx.wasm)
add_executable(calcyx_wasm
    ${CMAKE_SOURCE_DIR}/engine/calcyx_wasm.c
)

target_include_directories(calcyx_wasm PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
    ${MPDECIMAL_INCLUDE}
)

target_link_libraries(calcyx_wasm PRIVATE calcyx_engine)

target_link_options(calcyx_wasm PRIVATE
    # 公開する C 関数
    "-sEXPORTED_FUNCTIONS=['_wasm_init','_wasm_reset','_wasm_eval_line','_wasm_get_result','_wasm_get_error','_wasm_result_visible']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    # ES6 モジュールとして出力
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=CalcyxModule"
    "-sENVIRONMENT=web"
    # スタックサイズを増やす (再帰的評価器対策)
    "-sSTACK_SIZE=1mb"
    # ヒープを 32MB に拡大し mpdecimal の動的確保に対応
    "-sINITIAL_MEMORY=33554432"
    # ヒープ自動拡張を有効化 (大きな値の計算に備える)
    "-sALLOW_MEMORY_GROWTH=1"
    # クラッシュ時に関数名付きスタックトレースを出力 (デバッグ用)
    "--profiling-funcs"
    "--no-entry"
)

set_target_properties(calcyx_wasm PROPERTIES
    OUTPUT_NAME "calcyx"
    SUFFIX      ".js"
)

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_wasm dep_mpdecimal)
endif()

# ---- バージョン情報を JS に埋め込む ----
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/static/version.js.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.js"
    @ONLY
)

# ---- ビルド後: 静的ファイルをコピー・サンプルをルートから同期して manifest を生成 ----
# キャッシュバスター値にはビルド毎に変わる CALCYX_VERSION_FULL を流用する
# (git 未コミット時は 0.0.0-dev のため、リリースブランチでビルドした方が良い)。
add_custom_command(TARGET calcyx_wasm POST_BUILD
    # 静的ファイルをコピー
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/static"
        "${CMAKE_CURRENT_BINARY_DIR}"
    # ルートの samples/ をビルド出力へコピー (web/static/samples/ より優先)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/samples"
        "${CMAKE_CURRENT_BINARY_DIR}/samples"
    # manifest.json を生成 (app.js が動的読み込みに使用)
    COMMAND ${CMAKE_COMMAND}
        "-DSRC_DIR=${CMAKE_SOURCE_DIR}/samples"
        "-DOUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/samples/manifest.json"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/gen_manifest.cmake"
    # index.html と app.js に ?v=${CALCYX_VERSION_FULL} を注入
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/index.html"
        "-DCACHE_BUST=${CALCYX_VERSION_FULL}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/inject_cache_bust.cmake"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/app.js"
        "-DCACHE_BUST=${CALCYX_VERSION_FULL}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/inject_cache_bust.cmake"
    COMMENT "静的ファイルをコピー・サンプルを同期・manifest.json を生成・キャッシュバスター注入"
)

# ---- install() ルール (cpack 入力) ----
# POST_BUILD で生成済みのファイルを zip 化する。WASM ビルドでしか呼ばれない。
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    "${CMAKE_CURRENT_BINARY_DIR}/app.js"
    "${CMAKE_CURRENT_BINARY_DIR}/calcyx.css"
    "${CMAKE_CURRENT_BINARY_DIR}/calcyx.js"
    "${CMAKE_CURRENT_BINARY_DIR}/calcyx.wasm"
    "${CMAKE_CURRENT_BINARY_DIR}/colors.js"
    "${CMAKE_CURRENT_BINARY_DIR}/favicon.svg"
    "${CMAKE_CURRENT_BINARY_DIR}/highlight.js"
    "${CMAKE_CURRENT_BINARY_DIR}/paste-dialog.js"
    "${CMAKE_CURRENT_BINARY_DIR}/version.js"
    DESTINATION .
    COMPONENT web
)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/samples/"
    DESTINATION samples
    COMPONENT web
)
install(FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt"
    DESTINATION .
    COMPONENT web
)
