# riscv64 Platform Configuration for ZBC Target Tests
#
# 64-bit sibling of the riscv32 platform. Same QEMU "virt" machine,
# same memory map (RAM at 0x80000000, virtio-mmio at 0x10001000 stride
# 0x1000, sifive_test at 0x100000), same toolchain trick (clang's
# built-in riscv64 backend + ld.lld -- no separate cross-compiler).

set(ZBC_PLATFORM_NAME "riscv64")
set(ZBC_PLATFORM_riscv64_RUNNERS qemu)
set(ZBC_PLATFORM_riscv64_QEMU_MACHINE "virt")
# Skip OpenSBI (which qemu-system-riscv64 loads by default). Same as
# the riscv32 platform; qemu-system-aarch64 does NOT accept this flag.
set(ZBC_PLATFORM_riscv64_QEMU_EXTRA_ARGS -bios none)

find_program(CLANG_EXECUTABLE clang)
find_program(LD_LLD_EXECUTABLE ld.lld)
find_program(QEMU_RISCV64_EXECUTABLE qemu-system-riscv64)

if(NOT CLANG_EXECUTABLE)
    set(ZBC_PLATFORM_riscv64_FOUND FALSE)
    message(STATUS "riscv64 platform: clang not found")
    return()
endif()

if(NOT LD_LLD_EXECUTABLE)
    set(ZBC_PLATFORM_riscv64_FOUND FALSE)
    message(STATUS "riscv64 platform: ld.lld not found (install lld)")
    return()
endif()

if(NOT QEMU_RISCV64_EXECUTABLE)
    set(ZBC_PLATFORM_riscv64_FOUND FALSE)
    message(STATUS "riscv64 platform: qemu-system-riscv64 not found")
    return()
endif()

set(ZBC_PLATFORM_riscv64_FOUND TRUE)
set(ZBC_PLATFORM_riscv64_QEMU_BIN "${QEMU_RISCV64_EXECUTABLE}")
message(STATUS "riscv64 platform: clang + ld.lld + qemu-system-riscv64 available")

set(ZBC_PLATFORM_riscv64_CC "${CLANG_EXECUTABLE}")

set(ZBC_PLATFORM_riscv64_CFLAGS
    --target=riscv64-unknown-elf
    # M (mul/div) avoids libgcc helper calls; C (compressed) shrinks
    # text; both are present on QEMU's virt machine. Skip A, F, D since
    # we never atomic, never float.
    -march=rv64imc
    -mabi=lp64
    # medany code model: the default medlow can only reach symbols in
    # the low/high 2 GiB of the address space via 21-bit immediates,
    # which excludes our RAM base at 0x80000000.
    -mcmodel=medany
    -mno-relax
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_riscv64_LDFLAGS
    --target=riscv64-unknown-elf
    -fuse-ld=lld
    -nostdlib
    -nostartfiles
    -Wl,--build-id=none
)

set(ZBC_PLATFORM_riscv64_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crtriscv64.S
)

# QEMU platforms need the virtio/9p/vcon/composite transport sources
# beyond the base client lib.
set(ZBC_PLATFORM_riscv64_EXTRA_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_composite.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_vcon.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_9p.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_virtio.c
)

set(ZBC_PLATFORM_riscv64_PLATFORM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform_init.c
    ${CMAKE_SOURCE_DIR}/test/target/common/qemu_platform_init.c
)

set(ZBC_PLATFORM_riscv64_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbcriscv64.ld
)
