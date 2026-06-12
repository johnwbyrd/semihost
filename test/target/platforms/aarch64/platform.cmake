# aarch64 Platform Configuration for ZBC Target Tests
#
# Second QEMU-based ZBC target. Same toolchain trick as riscv32: clang's
# built-in aarch64 backend plus ld.lld, no separate cross-compiler to
# install. Runs under qemu-system-aarch64 on the "virt" machine, which
# exposes 32 virtio-mmio slots starting at 0x0a000000 stride 0x200.

set(ZBC_PLATFORM_NAME "aarch64")
set(ZBC_PLATFORM_aarch64_RUNNERS qemu)
set(ZBC_PLATFORM_aarch64_QEMU_MACHINE "virt")
# cortex-a57 is a widely-supported armv8-a baseline; QEMU virt accepts it
# without needing -machine type tweaks, and clang's --target codegen
# pairs with it cleanly.
set(ZBC_PLATFORM_aarch64_QEMU_EXTRA_ARGS -cpu cortex-a57)

find_program(CLANG_EXECUTABLE clang)
find_program(LD_LLD_EXECUTABLE ld.lld)
find_program(QEMU_AARCH64_EXECUTABLE qemu-system-aarch64)

if(NOT CLANG_EXECUTABLE)
    set(ZBC_PLATFORM_aarch64_FOUND FALSE)
    message(STATUS "aarch64 platform: clang not found")
    return()
endif()

if(NOT LD_LLD_EXECUTABLE)
    set(ZBC_PLATFORM_aarch64_FOUND FALSE)
    message(STATUS "aarch64 platform: ld.lld not found (install lld)")
    return()
endif()

if(NOT QEMU_AARCH64_EXECUTABLE)
    set(ZBC_PLATFORM_aarch64_FOUND FALSE)
    message(STATUS "aarch64 platform: qemu-system-aarch64 not found")
    return()
endif()

set(ZBC_PLATFORM_aarch64_FOUND TRUE)
set(ZBC_PLATFORM_aarch64_QEMU_BIN "${QEMU_AARCH64_EXECUTABLE}")
message(STATUS "aarch64 platform: clang + ld.lld + qemu-system-aarch64 available")

set(ZBC_PLATFORM_aarch64_CC "${CLANG_EXECUTABLE}")

set(ZBC_PLATFORM_aarch64_CFLAGS
    --target=aarch64-unknown-elf
    -mcpu=cortex-a57
    # general-regs-only avoids SIMD/FP codegen so we don't have to
    # enable FPEN at boot. The transports are integer-only.
    -mgeneral-regs-only
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_aarch64_LDFLAGS
    --target=aarch64-unknown-elf
    -fuse-ld=lld
    -nostdlib
    -nostartfiles
    -Wl,--build-id=none
)

set(ZBC_PLATFORM_aarch64_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crtaarch64.S
)

# QEMU platforms need the virtio/9p/vcon/composite transport sources
# beyond the base client lib; MAME platforms keep just the RIFF default.
set(ZBC_PLATFORM_aarch64_EXTRA_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_composite.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_vcon.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_9p.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_virtio.c
)

set(ZBC_PLATFORM_aarch64_PLATFORM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform_init.c
)

set(ZBC_PLATFORM_aarch64_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbcaarch64.ld
)
