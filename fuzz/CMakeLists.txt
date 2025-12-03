# Fuzzing targets using libFuzzer
#
# libFuzzer requires Clang. Build with:
#   cmake -B build-fuzz -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
#   cmake --build build-fuzz
#
# Run fuzzer:
#   ./build-fuzz/fuzz/fuzz_riff_parser corpus/riff_parser/

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

# RIFF parser fuzz target
add_executable(fuzz_riff_parser
    fuzz_riff_parser.c
)
target_compile_options(fuzz_riff_parser PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_riff_parser PRIVATE ${FUZZ_FLAGS})
target_link_libraries(fuzz_riff_parser zbc_semi_host)
target_compile_definitions(fuzz_riff_parser PRIVATE ZBC_HOST)
