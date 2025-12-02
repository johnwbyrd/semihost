# ZBC Semihosting Libraries
#
# Build:
#   cmake -B build
#   cmake --build build
#   ctest --test-dir build
#
# Or for a specific generator:
#   cmake -B build -G "Unix Makefiles"
#   cmake -B build -G "Visual Studio 17 2022"
#   cmake -B build -G "Xcode"

cmake_minimum_required(VERSION 3.10)
project(zbc_semihost C)

# C standard
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Warning flags (portable)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Client library
add_library(zbc_semi_client STATIC
    src/client/zbc_client_core.c
    src/client/zbc_client_syscalls.c
)

# Host library
add_library(zbc_semi_host STATIC
    src/host/zbc_host_core.c
)

# Tests
add_subdirectory(test)
