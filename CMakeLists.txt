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
project(zbc_semihost C CXX)

# Set the default build type to Release.
# Users can override this with -DCMAKE_BUILD_TYPE=Debug or similar.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as default.")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Enable debug symbols for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g)
endif()

# C standard
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# C++ standard (for the optional C++ host library)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sanitizers (ASan + UBSan)
include(${CMAKE_SOURCE_DIR}/cmake/Sanitizers.cmake)

# Warning flags (portable)
if(MSVC)
    add_compile_options(/W4)
    # Suppress C4996 warnings for standard C functions (fopen, sprintf, etc.)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments)
endif()

# Include directories (per-tier; sources keep existing #include directives)
include_directories(
    ${CMAKE_SOURCE_DIR}/include/shared
    ${CMAKE_SOURCE_DIR}/include/c
    ${CMAKE_SOURCE_DIR}/include/cpp
)

# Shared sources: protocol primitives reused by every host/client/binding.
set(ZBC_SHARED_SOURCES
    src/shared/zbc_opcode_table.c
    src/shared/zbc_riff.c
)

# Client library
add_library(zbc_semi_client STATIC
    ${ZBC_SHARED_SOURCES}
    src/c/zbc_client.c
    src/c/zbc_api.c
)
target_compile_definitions(zbc_semi_client PRIVATE ZBC_CLIENT)

# Host library
add_library(zbc_semi_host STATIC
    ${ZBC_SHARED_SOURCES}
    src/c/zbc_host.c
    src/c/zbc_backend_dummy.c
    src/c/zbc_ansi_common.c
    src/c/zbc_ansi_secure.c
    src/c/zbc_ansi_insecure.c
)
target_compile_definitions(zbc_semi_host PRIVATE ZBC_HOST)
target_include_directories(zbc_semi_host PRIVATE ${CMAKE_SOURCE_DIR}/src/c)

# C++ host library for emulators and other host-side embedders.
# Same RIFF protocol as the C host, but a modern C++17 API: Backend/Policy
# objects, OpResult/Result value types, and a memory-mapped Device. Reuses
# the shared C sources for endianness handling so the two implementations
# cannot drift.
option(ZBC_BUILD_CPP "Build the C++17 host library (zbc_semi_hostpp)" ON)
if(ZBC_BUILD_CPP)
    add_library(zbc_semi_hostpp STATIC
        ${ZBC_SHARED_SOURCES}
        src/cpp/RiffCodec.cpp
        src/cpp/Backend.cpp
        src/cpp/FileDescTable.cpp
        src/cpp/PathValidator.cpp
        src/cpp/Policy.cpp
        src/cpp/Device.cpp
    )
    target_include_directories(zbc_semi_hostpp PUBLIC
        ${CMAKE_SOURCE_DIR}/include/shared
        ${CMAKE_SOURCE_DIR}/include/cpp
    )
    message(STATUS "C++ host library (zbc_semi_hostpp): enabled")
endif()

# Sandbox option (optional, Linux only)
option(ZBC_USE_SECCOMP "Enable seccomp sandboxing (Linux only, requires libseccomp)" OFF)

# Seccomp support (Linux)
if(ZBC_USE_SECCOMP)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "ZBC_USE_SECCOMP requires Linux")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SECCOMP REQUIRED libseccomp)
    target_compile_definitions(zbc_semi_host PRIVATE ZBC_HAVE_SECCOMP)
    target_include_directories(zbc_semi_host PRIVATE ${SECCOMP_INCLUDE_DIRS})
    target_link_libraries(zbc_semi_host PRIVATE ${SECCOMP_LIBRARIES})
    target_sources(zbc_semi_host PRIVATE src/c/platform/linux/sandbox_seccomp.c)
    message(STATUS "Seccomp sandboxing: enabled")
else()
    target_sources(zbc_semi_host PRIVATE src/c/platform/generic/sandbox_stub.c)
    message(STATUS "Sandboxing: disabled (stub)")
endif()

# Tests
enable_testing()
add_subdirectory(test)

# On-target tests (optional, requires cross-compiler and emulator)
option(ZBC_TARGET_TESTS "Enable on-target tests for emulated ZBC platforms (requires MAME)" OFF)
if(ZBC_TARGET_TESTS)
    add_subdirectory(test/target)
endif()

# Fuzzing (optional, requires Clang)
option(ENABLE_FUZZING "Enable libFuzzer-based fuzz targets (requires Clang)" OFF)
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
