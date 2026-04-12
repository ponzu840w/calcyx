# cmake/toolchain-emscripten.cmake
# Emscripten (WebAssembly) クロスコンパイル用ツールチェーン
# 使い方: cmake --preset wasm

execute_process(
    COMMAND em-config EMSCRIPTEN_ROOT
    OUTPUT_VARIABLE _EM_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT _EM_ROOT OR NOT EXISTS "${_EM_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
    message(FATAL_ERROR
        "Emscripten が見つかりません。\n"
        "macOS: brew install emscripten\n"
        "その他: https://emscripten.org/docs/getting_started/downloads.html"
    )
endif()

include("${_EM_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
