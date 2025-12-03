# ZBC Semihosting Test Suite
#
# Build with: cmake -B build && cmake --build build && ctest --test-dir build

cmake_minimum_required(VERSION 3.10)

# Test executable
add_executable(zbc_tests
    test_harness.c
    mock_device.c
    mock_memory.c
    test_client_builder.c
    test_roundtrip.c
    test_ansi_insecure.c
    test_ansi_secure.c
    test_main.c
)

# Include paths
target_include_directories(zbc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Tests need both client and host definitions
target_compile_definitions(zbc_tests PRIVATE ZBC_CLIENT ZBC_HOST)

# Link against the semihosting libraries
target_link_libraries(zbc_tests
    zbc_semi_client
    zbc_semi_host
)

# Register test with CTest
add_test(NAME zbc_semihost_tests COMMAND zbc_tests)

# Static allocation check
# Verify that library sources don't use malloc/calloc/realloc/free
# Exception: ANSI backend may allocate for tracking open files
file(GLOB LIB_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

foreach(src ${LIB_SOURCES})
    # Skip ANSI backend files - they may allocate for tracking open files
    string(FIND "${src}" "zbc_ansi_" is_ansi_backend)
    if(is_ansi_backend GREATER -1)
        continue()
    endif()

    file(READ "${src}" content)
    # Check for allocation function calls
    string(REGEX MATCH "(malloc|calloc|realloc)[ \t]*\\(" found "${content}")
    if(found)
        message(FATAL_ERROR
            "Allocation function found in ${src}:\n"
            "  ${found}\n"
            "Libraries must not allocate memory - caller provides buffers.")
    endif()
endforeach()

message(STATUS "Static allocation check passed - no malloc/calloc/realloc in library sources (ANSI backend exempt)")
