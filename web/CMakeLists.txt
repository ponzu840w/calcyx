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
    "-sEXPORTED_FUNCTIONS=['_wasm_init','_wasm_reset','_wasm_eval_line','_wasm_get_result','_wasm_get_error']"
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

# ---- ビルド後: 静的ファイルを build-wasm/web/ にコピー ----
add_custom_command(TARGET calcyx_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/static"
        "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "静的ファイルをコピー"
)
