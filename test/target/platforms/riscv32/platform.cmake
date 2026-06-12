# riscv32 Platform Configuration for ZBC Target Tests
#
# First QEMU-based target platform. Uses clang's built-in RISC-V backend
# and ld.lld so no separate cross-toolchain has to be installed -- on a
# stock Ubuntu, `apt install clang lld qemu-system-misc` is enough.
# Runs under qemu-system-riscv32 on the "virt" machine, which exposes
# virtio-mmio devices out of the box.

set(ZBC_PLATFORM_NAME "riscv32")
set(ZBC_PLATFORM_riscv32_RUNNERS qemu)
set(ZBC_PLATFORM_riscv32_QEMU_MACHINE "virt")
# Skip OpenSBI; load the ELF straight into RAM at 0x80000000 and start
# the hart there. qemu-system-aarch64 does NOT accept this flag (it
# reads "none" as a filename), which is why it lives per-platform.
set(ZBC_PLATFORM_riscv32_QEMU_EXTRA_ARGS -bios none)

find_program(CLANG_EXECUTABLE clang)
find_program(LD_LLD_EXECUTABLE ld.lld)
find_program(QEMU_RISCV32_EXECUTABLE qemu-system-riscv32)

if(NOT CLANG_EXECUTABLE)
    set(ZBC_PLATFORM_riscv32_FOUND FALSE)
    message(STATUS "riscv32 platform: clang not found")
    return()
endif()

if(NOT LD_LLD_EXECUTABLE)
    set(ZBC_PLATFORM_riscv32_FOUND FALSE)
    message(STATUS "riscv32 platform: ld.lld not found (install lld)")
    return()
endif()

if(NOT QEMU_RISCV32_EXECUTABLE)
    set(ZBC_PLATFORM_riscv32_FOUND FALSE)
    message(STATUS "riscv32 platform: qemu-system-riscv32 not found")
    return()
endif()

set(ZBC_PLATFORM_riscv32_FOUND TRUE)
set(ZBC_PLATFORM_riscv32_QEMU_BIN "${QEMU_RISCV32_EXECUTABLE}")
message(STATUS "riscv32 platform: clang + ld.lld + qemu-system-riscv32 available")

set(ZBC_PLATFORM_riscv32_CC "${CLANG_EXECUTABLE}")

set(ZBC_PLATFORM_riscv32_CFLAGS
    --target=riscv32-unknown-elf
    # M (mul/div) avoids libgcc helper calls; C (compressed) shrinks
    # text; both are present on QEMU's virt machine. Skip A, F, D since
    # we never atomic, never float.
    -march=rv32imc
    -mabi=ilp32
    -mno-relax
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_riscv32_LDFLAGS
    --target=riscv32-unknown-elf
    -fuse-ld=lld
    -nostdlib
    -nostartfiles
    -Wl,--build-id=none
)

set(ZBC_PLATFORM_riscv32_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crtriscv32.S
)

# QEMU platforms need the virtio/9p/vcon/composite transport sources
# beyond the base client lib; MAME platforms keep just the RIFF default.
set(ZBC_PLATFORM_riscv32_EXTRA_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_composite.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_vcon.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_9p.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_virtio.c
)

set(ZBC_PLATFORM_riscv32_PLATFORM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform_init.c
)

set(ZBC_PLATFORM_riscv32_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbcriscv32.ld
)
