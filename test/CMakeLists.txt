# ZBC Semihosting Test Suite
#
# Build with: cmake -B build && cmake --build build && ctest --test-dir build
#
# Directory structure:
#   test/
#   ├── common/     - Shared test infrastructure (harness, mocks)
#   ├── host/       - Tests that run on build machine
#   └── target/     - Cross-compiled tests for emulators

cmake_minimum_required(VERSION 3.10)

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

# Host tests (always built)
add_subdirectory(host)
