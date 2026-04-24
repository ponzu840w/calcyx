add_library(calcyx_engine STATIC
    types/ufixed113.c
    types/real.c
    types/quad.c
    types/frac.c
    types/val.c
    parser/lexer.c
    parser/parser.c
    eval/eval_ctx.c
    eval/eval.c
    eval/builtin.c
    eval/builtin_array.c
    eval/builtin_array_ops.c
    eval/builtin_array_stats.c
    eval/builtin_array_string.c
    eval/builtin_array_bits.c
    eval/builtin_array_color.c
    eval/builtin_array_encode.c
    ${CMAKE_SOURCE_DIR}/shared/completion_match.c
)

# shared/ は UI 非依存の共有モデル層 (現在は completion_match のみ; 将来
# sheet_model 等を追加予定)。engine と同じ静的ライブラリに取り込むことで
# GUI / TUI / CLI / WASM すべてから自動的に利用できる。
target_include_directories(calcyx_engine PUBLIC . ${CMAKE_SOURCE_DIR}/shared ${MPDECIMAL_INCLUDE})
target_link_libraries(calcyx_engine PUBLIC
    ${MPDECIMAL_LIB}
    $<$<NOT:$<BOOL:${WIN32}>>:m>
)

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_engine dep_mpdecimal)
endif()

add_executable(test_types test_types.c)
target_link_libraries(test_types PRIVATE calcyx_engine)

add_executable(test_eval test_eval.c)
target_link_libraries(test_eval PRIVATE calcyx_engine)
target_include_directories(test_eval PRIVATE . ${MPDECIMAL_INCLUDE})

add_executable(test_parser test_parser.c)
target_link_libraries(test_parser PRIVATE calcyx_engine)

# ===== ctest 登録 =====
# CALCYX_TESTS_ENABLED が FALSE のとき (Windows cross + wine なし + 非 WSL、
# または WASM + node なし) はプリセット単位でまるごとスキップする。
# ランチャー経由の場合 $<TARGET_FILE:test_types> が .exe / .js に展開される。

# WASM 固有のリンクオプション:
# - NODERAWFS=1: test_eval の fopen が Node の実 FS にアクセスできるよう
#   にする (test_types / test_parser は FS 不要だが付与して害はない)。
# - STACK_SIZE=1mb: tok_queue_t (~140KB) のスタック確保で emscripten の
#   デフォルト 64KB を超え、test_parser がスタックオーバーフローする
#   のを防ぐ。
if(EMSCRIPTEN)
    target_link_options(test_types  PRIVATE "-sNODERAWFS=1" "-sSTACK_SIZE=1mb")
    target_link_options(test_parser PRIVATE "-sNODERAWFS=1" "-sSTACK_SIZE=1mb")
    target_link_options(test_eval   PRIVATE "-sNODERAWFS=1" "-sSTACK_SIZE=1mb")
endif()

if(CALCYX_TESTS_ENABLED)
    add_test(NAME engine/types
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_types>)
    set_tests_properties(engine/types PROPERTIES LABELS "engine")

    add_test(NAME engine/parser
        COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_parser>)
    set_tests_properties(engine/parser PROPERTIES LABELS "engine")

    # engine/Test_* (test_eval による sample ファイル駆動テスト) は
    # WASM 下では mpdecimal の計算が極端に遅く 1 ファイル数分以上かかる
    # ため登録しない。型システムと lexer/parser の回帰は type/parser
    # テストでカバーできる。ネイティブ / Windows クロスビルドでは通常通り登録。
    if(NOT EMSCRIPTEN)
        set(SAMPLES_DIR "${CMAKE_SOURCE_DIR}/samples")
        file(GLOB SAMPLE_TEST_FILES "${SAMPLES_DIR}/Test_*.txt")
        foreach(TXT ${SAMPLE_TEST_FILES})
            get_filename_component(TNAME ${TXT} NAME_WE)
            add_test(NAME engine/${TNAME}
                COMMAND ${CALCYX_TEST_LAUNCHER} $<TARGET_FILE:test_eval> ${TXT})
            set_tests_properties(engine/${TNAME} PROPERTIES LABELS "engine")
        endforeach()
    endif()
endif()
