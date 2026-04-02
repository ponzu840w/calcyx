add_library(calcyx_engine STATIC
    types/ufixed113.c
    types/real.c
    types/quad.c
    types/frac.c
    types/val.c
    parser/lexer.c
    parser/parser.c
)

target_include_directories(calcyx_engine PUBLIC . ${MPDECIMAL_INCLUDE})
target_link_libraries(calcyx_engine PUBLIC ${MPDECIMAL_LIB})

add_executable(test_types test_types.c)
target_link_libraries(test_types PRIVATE calcyx_engine m)
