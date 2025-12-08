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

# Sanitizers (ASan + UBSan)
include(${CMAKE_SOURCE_DIR}/cmake/Sanitizers.cmake)

# Warning flags (portable)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-variadic-macros)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Shared sources used by both client and host
set(ZBC_SHARED_SOURCES
    src/zbc_opcode_table.c
    src/zbc_riff.c
)

# Client library
add_library(zbc_semi_client STATIC
    ${ZBC_SHARED_SOURCES}
    src/zbc_client.c
)
target_compile_definitions(zbc_semi_client PRIVATE ZBC_CLIENT)

# Host library
add_library(zbc_semi_host STATIC
    ${ZBC_SHARED_SOURCES}
    src/zbc_host.c
    src/zbc_backend_dummy.c
    src/zbc_ansi_common.c
    src/zbc_ansi_secure.c
    src/zbc_ansi_insecure.c
)
target_compile_definitions(zbc_semi_host PRIVATE ZBC_HOST)
target_include_directories(zbc_semi_host PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Sandbox options (optional, platform-specific)
option(ZBC_USE_SECCOMP "Enable seccomp sandboxing (Linux only, requires libseccomp)" OFF)
option(ZBC_USE_SEATBELT "Enable Seatbelt sandboxing (macOS only)" OFF)

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
    target_sources(zbc_semi_host PRIVATE src/platform/linux/sandbox_seccomp.c)
    message(STATUS "Seccomp sandboxing: enabled")
endif()

# Seatbelt support (macOS)
if(ZBC_USE_SEATBELT)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        message(FATAL_ERROR "ZBC_USE_SEATBELT requires macOS")
    endif()
    target_compile_definitions(zbc_semi_host PRIVATE ZBC_HAVE_SEATBELT)
    target_sources(zbc_semi_host PRIVATE src/platform/darwin/sandbox_seatbelt.c)
    message(STATUS "Seatbelt sandboxing: enabled")
endif()

# Stub if neither sandbox option enabled
if(NOT ZBC_USE_SECCOMP AND NOT ZBC_USE_SEATBELT)
    target_sources(zbc_semi_host PRIVATE src/platform/generic/sandbox_stub.c)
    message(STATUS "Sandboxing: disabled (stub)")
endif()

# Tests
enable_testing()
add_subdirectory(test)

# Fuzzing (optional, requires Clang)
option(ENABLE_FUZZING "Enable libFuzzer-based fuzz targets (requires Clang)" OFF)
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
