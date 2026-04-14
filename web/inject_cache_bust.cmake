# ビルド成果物の静的ファイルにキャッシュバスタークエリを注入するスクリプト。
#
# 使い方:
#   cmake -DINPUT_FILE=<path> -DCACHE_BUST=<value> -P inject_cache_bust.cmake
#
# 置換パターン:
#   - index.html:  href="foo.css"   → href="foo.css?v=<CACHE_BUST>"
#                   src="app.js"     →  src="app.js?v=<CACHE_BUST>"
#                  (拡張子 css/js/svg/wasm が対象。既に ?v= を含むものは除外)
#   - app.js:      from './foo.js'  → from './foo.js?v=<CACHE_BUST>'
#
# ソースは無改造のまま、ビルド出力だけを書き換えるのが意図。

if(NOT DEFINED INPUT_FILE OR NOT DEFINED CACHE_BUST)
    message(FATAL_ERROR "INPUT_FILE and CACHE_BUST must be defined")
endif()

if(NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR "INPUT_FILE does not exist: ${INPUT_FILE}")
endif()

file(READ "${INPUT_FILE}" _content)

# href="xxx.ext" / src="xxx.ext" への注入 (HTML 用)
# 否定文字クラス [^"?] でクエリを含むものを除外し二重適用を防ぐ
string(REGEX REPLACE
    "(href|src)=\"([^\"?]+\\.(css|js|svg|wasm))\""
    "\\1=\"\\2?v=${CACHE_BUST}\""
    _content "${_content}"
)

# from './xxx.js' への注入 (ES module 静的 import 用)
string(REGEX REPLACE
    "from '(\\./[^'?]+\\.js)'"
    "from '\\1?v=${CACHE_BUST}'"
    _content "${_content}"
)

file(WRITE "${INPUT_FILE}" "${_content}")
message(STATUS "Injected cache-bust '${CACHE_BUST}' into ${INPUT_FILE}")
