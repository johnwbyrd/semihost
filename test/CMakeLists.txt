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
    test_main.c
)

# Include paths
target_include_directories(zbc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against the semihosting libraries
target_link_libraries(zbc_tests
    zbc_semi_client
    zbc_semi_host
)

# Register test with CTest
enable_testing()
add_test(NAME zbc_semihost_tests COMMAND zbc_tests)

# Static allocation check
# Verify that library sources don't use malloc/calloc/realloc/free
file(GLOB_RECURSE LIB_SOURCES
    "${CMAKE_SOURCE_DIR}/src/client/*.c"
    "${CMAKE_SOURCE_DIR}/src/host/*.c"
)

foreach(src ${LIB_SOURCES})
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

message(STATUS "Static allocation check passed - no malloc/calloc/realloc in library sources")
