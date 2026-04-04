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
)

target_include_directories(calcyx_engine PUBLIC . ${MPDECIMAL_INCLUDE})
target_link_libraries(calcyx_engine PUBLIC ${MPDECIMAL_LIB})

add_executable(test_types test_types.c)
target_link_libraries(test_types PRIVATE calcyx_engine m)

# ===== eval テストハーネス =====
set(SAMPLES_DIR "${CMAKE_SOURCE_DIR}/tmp/calctus-linux/Samples")

add_executable(test_eval test_eval.c)
target_link_libraries(test_eval PRIVATE calcyx_engine m)
target_include_directories(test_eval PRIVATE . ${MPDECIMAL_INCLUDE})

enable_testing()

add_test(NAME Test_Types COMMAND test_types)

# 各 Test_*.txt ファイルを ctest ターゲットとして登録
file(GLOB SAMPLE_TEST_FILES "${SAMPLES_DIR}/Test_*.txt")
foreach(TXT ${SAMPLE_TEST_FILES})
    get_filename_component(TNAME ${TXT} NAME_WE)
    add_test(NAME ${TNAME}
             COMMAND test_eval ${TXT})
endforeach()
