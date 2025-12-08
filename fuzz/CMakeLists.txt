# Fuzzing targets using libFuzzer
#
# libFuzzer requires Clang. Build with:
#   cmake -B build-fuzz -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
#   cmake --build build-fuzz
#
# Run fuzzer:
#   ./build-fuzz/fuzz/fuzz_riff_parser ./build-fuzz/fuzz/corpus/riff_parser/

# Require Clang for libFuzzer
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzing requires Clang (libFuzzer). Use -DCMAKE_C_COMPILER=clang")
endif()

# Fuzzer compile/link flags
set(FUZZ_FLAGS
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=undefined
)

# Generate malformed corpus at build time
set(CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpus/riff_parser)
set(CORPUS_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/gen_malformed_corpus.py)
set(CORPUS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/corpus.stamp)

add_custom_command(
    OUTPUT ${CORPUS_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CORPUS_DIR}
    COMMAND python3 ${CORPUS_GENERATOR} ${CORPUS_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${CORPUS_STAMP}
    DEPENDS ${CORPUS_GENERATOR}
    COMMENT "Generating malformed RIFF corpus"
)

add_custom_target(fuzz_corpus DEPENDS ${CORPUS_STAMP})

# RIFF parser fuzz target
add_executable(fuzz_riff_parser
    fuzz_riff_parser.c
)
add_dependencies(fuzz_riff_parser fuzz_corpus)
target_compile_options(fuzz_riff_parser PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_riff_parser PRIVATE ${FUZZ_FLAGS})
target_link_libraries(fuzz_riff_parser zbc_semi_host)
target_compile_definitions(fuzz_riff_parser PRIVATE ZBC_HOST)
