# ZBC On-Target Tests
#
# Cross-compiled tests that run on emulated ZBC platforms (MAME, QEMU).
#
# Usage:
#   cmake -B build -DZBC_TARGET_TESTS=ON
#   cmake --build build
#   ctest --test-dir build -L target
#
# Options:
#   -DZBC_TARGET_PLATFORMS="i386;m6502"  Platforms to build (default: all available)
#   -DMAME_PATH=/path/to/mame            Path to MAME executable

cmake_minimum_required(VERSION 3.16)

message(STATUS "")
message(STATUS "=== ZBC On-Target Tests ===")

# Include runner configurations
include(${CMAKE_CURRENT_SOURCE_DIR}/runners/mame.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/runners/qemu.cmake)

# Check if any runner is available
if(NOT ZBC_RUNNER_MAME_FOUND AND NOT ZBC_RUNNER_QEMU_FOUND)
    message(STATUS "No test runners available - skipping on-target tests")
    message(STATUS "Install MAME and set MAME_PATH to enable")
    return()
endif()

# Supported platforms
set(ZBC_SUPPORTED_PLATFORMS i386 m6502 riscv32 riscv64 aarch64)

# Allow user to specify platforms, default to all
if(NOT ZBC_TARGET_PLATFORMS)
    set(ZBC_TARGET_PLATFORMS ${ZBC_SUPPORTED_PLATFORMS})
endif()

# Detect available platforms
set(ZBC_AVAILABLE_PLATFORMS "")
foreach(platform ${ZBC_TARGET_PLATFORMS})
    if(NOT platform IN_LIST ZBC_SUPPORTED_PLATFORMS)
        message(WARNING "Unknown platform: ${platform}")
        continue()
    endif()

    # Include platform configuration
    include(${CMAKE_CURRENT_SOURCE_DIR}/platforms/${platform}/platform.cmake)

    if(ZBC_PLATFORM_${platform}_FOUND)
        list(APPEND ZBC_AVAILABLE_PLATFORMS ${platform})
    endif()
endforeach()

if(NOT ZBC_AVAILABLE_PLATFORMS)
    message(STATUS "No platforms available - skipping on-target tests")
    message(STATUS "For i386: install gcc-multilib")
    message(STATUS "For m6502: install llvm-mos and set LLVM_MOS_PATH")
    return()
endif()

message(STATUS "Available platforms: ${ZBC_AVAILABLE_PLATFORMS}")

# Client library sources (needed for each platform build)
set(ZBC_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_client.c
    ${CMAKE_SOURCE_DIR}/src/shared/zbc_riff.c
    ${CMAKE_SOURCE_DIR}/src/shared/zbc_opcode_table.c
)

# Test source
set(ZBC_TARGET_TEST_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/zbc_target_test.c
)

#
# Build test executable for each available platform
#
foreach(platform ${ZBC_AVAILABLE_PLATFORMS})
    set(target_name zbc_target_test_${platform})

    message(STATUS "Configuring ${target_name}")

    # Create custom target for cross-compilation
    # We use add_custom_command because CMake's built-in cross-compilation
    # requires toolchain files, which is more complex than we need.

    set(ELF_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.elf)

    # Get platform-specific settings
    set(CC ${ZBC_PLATFORM_${platform}_CC})
    set(CFLAGS ${ZBC_PLATFORM_${platform}_CFLAGS})
    set(LDFLAGS ${ZBC_PLATFORM_${platform}_LDFLAGS})
    set(CRT ${ZBC_PLATFORM_${platform}_CRT})
    set(LD ${ZBC_PLATFORM_${platform}_LD})
    set(PLATFORM_SOURCES ${ZBC_PLATFORM_${platform}_PLATFORM_SOURCES})
    set(EXTRA_CLIENT_SOURCES ${ZBC_PLATFORM_${platform}_EXTRA_CLIENT_SOURCES})
    set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platforms/${platform})

    # Build command
    add_custom_command(
        OUTPUT ${ELF_OUTPUT}
        COMMAND ${CC}
            ${CFLAGS}
            -I${CMAKE_SOURCE_DIR}/include/c
            -I${CMAKE_SOURCE_DIR}/include/shared
            -I${CMAKE_CURRENT_SOURCE_DIR}
            -I${PLATFORM_DIR}
            -DZBC_CLIENT
            ${LDFLAGS}
            -T${LD}
            -o ${ELF_OUTPUT}
            ${ZBC_TARGET_TEST_SOURCE}
            ${ZBC_CLIENT_SOURCES}
            ${EXTRA_CLIENT_SOURCES}
            ${PLATFORM_SOURCES}
            ${CRT}
        DEPENDS
            ${ZBC_TARGET_TEST_SOURCE}
            ${ZBC_CLIENT_SOURCES}
            ${EXTRA_CLIENT_SOURCES}
            ${PLATFORM_SOURCES}
            ${CRT}
            ${LD}
            ${CMAKE_CURRENT_SOURCE_DIR}/zbc_target_harness.h
        COMMENT "Building ${target_name}"
        VERBATIM
    )

    add_custom_target(${target_name} ALL DEPENDS ${ELF_OUTPUT})

    # Register tests with each runner this platform declares it supports.
    # Per-runner config (machine name, qemu flags, etc.) lives in the
    # platform's platform.cmake as ZBC_PLATFORM_<p>_<RUNNER>_* variables.
    foreach(runner ${ZBC_PLATFORM_${platform}_RUNNERS})
        if(runner STREQUAL "mame")
            if(NOT ZBC_RUNNER_MAME_FOUND)
                continue()
            endif()
            set(mame_machine ${ZBC_PLATFORM_${platform}_MAME_MACHINE})
            if(NOT mame_machine)
                message(WARNING
                    "Platform ${platform} declares mame runner but "
                    "ZBC_PLATFORM_${platform}_MAME_MACHINE is unset")
                continue()
            endif()

            add_mame_test(
                zbc_target_${platform}_mame
                ${ELF_OUTPUT}
                ${mame_machine}
                TIMEOUT 60
                INPUT "A"  # Input for READC test
            )

            set_tests_properties(zbc_target_${platform}_mame PROPERTIES
                DEPENDS ${target_name}
            )
        elseif(runner STREQUAL "qemu")
            if(NOT ZBC_RUNNER_QEMU_FOUND)
                continue()
            endif()
            set(qemu_bin ${ZBC_PLATFORM_${platform}_QEMU_BIN})
            set(qemu_machine ${ZBC_PLATFORM_${platform}_QEMU_MACHINE})
            if(NOT qemu_bin OR NOT qemu_machine)
                message(WARNING
                    "Platform ${platform} declares qemu runner but "
                    "ZBC_PLATFORM_${platform}_QEMU_BIN / _QEMU_MACHINE "
                    "are unset")
                continue()
            endif()

            add_qemu_test(
                zbc_target_${platform}_qemu
                ELF        ${ELF_OUTPUT}
                QEMU_BIN   ${qemu_bin}
                MACHINE    ${qemu_machine}
                EXTRA_ARGS ${ZBC_PLATFORM_${platform}_QEMU_EXTRA_ARGS}
                TIMEOUT    60
                INPUT      "A"
            )

            set_tests_properties(zbc_target_${platform}_qemu PROPERTIES
                DEPENDS ${target_name}
            )
        else()
            message(WARNING
                "Platform ${platform} declared unknown runner: ${runner}")
        endif()
    endforeach()

endforeach()

message(STATUS "=== End ZBC On-Target Tests ===")
message(STATUS "")
