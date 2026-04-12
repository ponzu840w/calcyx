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

if(TARGET dep_mpdecimal)
    add_dependencies(calcyx_engine dep_mpdecimal)
endif()

add_executable(test_types test_types.c)
target_link_libraries(test_types PRIVATE calcyx_engine $<$<NOT:$<BOOL:${WIN32}>>:m>)

add_executable(test_eval test_eval.c)
target_link_libraries(test_eval PRIVATE calcyx_engine $<$<NOT:$<BOOL:${WIN32}>>:m>)
target_include_directories(test_eval PRIVATE . ${MPDECIMAL_INCLUDE})

# ===== ctest 登録 =====

add_test(NAME engine/types COMMAND test_types)
set_tests_properties(engine/types PROPERTIES LABELS "engine")

set(SAMPLES_DIR "${CMAKE_SOURCE_DIR}/samples")
file(GLOB SAMPLE_TEST_FILES "${SAMPLES_DIR}/Test_*.txt")
foreach(TXT ${SAMPLE_TEST_FILES})
    get_filename_component(TNAME ${TXT} NAME_WE)
    add_test(NAME engine/${TNAME} COMMAND test_eval ${TXT})
    set_tests_properties(engine/${TNAME} PROPERTIES LABELS "engine")
endforeach()
