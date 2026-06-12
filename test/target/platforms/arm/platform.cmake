# arm (32-bit) Platform Configuration for ZBC Target Tests
#
# AArch32 sibling of the aarch64 platform. Same QEMU "virt" machine,
# same memory map (RAM at 0x40000000, virtio-mmio at 0x0a000000 stride
# 0x200, 32 slots), same toolchain trick (clang + ld.lld, no separate
# cross-compiler). PSCI calls go via SMC #0 rather than aarch64's
# HVC #0 -- the cortex-a15 starts in PL1 non-secure SVC mode and SMC
# traps to QEMU's PSCI emulation.

set(ZBC_PLATFORM_NAME "arm")
set(ZBC_PLATFORM_arm_RUNNERS qemu)
set(ZBC_PLATFORM_arm_QEMU_MACHINE "virt")
# The QEMU arm virt machine defaults to secure=off, which selects HVC
# as the PSCI conduit; platform_init.c emits "hvc #0" accordingly.
# If you ever add ,secure=on to the machine line (or move to a CPU
# whose default conduit differs), flip the inline-asm mnemonic in
# arm_psci_call() to "smc #0" -- otherwise SYS_EXIT decodes as UNDEF
# and the VM hangs forever after the suite prints RESULT: PASS.
set(ZBC_PLATFORM_arm_QEMU_EXTRA_ARGS -cpu cortex-a15)

find_program(CLANG_EXECUTABLE clang)
find_program(LD_LLD_EXECUTABLE ld.lld)
find_program(QEMU_ARM_EXECUTABLE qemu-system-arm)

if(NOT CLANG_EXECUTABLE)
    set(ZBC_PLATFORM_arm_FOUND FALSE)
    message(STATUS "arm platform: clang not found")
    return()
endif()

if(NOT LD_LLD_EXECUTABLE)
    set(ZBC_PLATFORM_arm_FOUND FALSE)
    message(STATUS "arm platform: ld.lld not found (install lld)")
    return()
endif()

if(NOT QEMU_ARM_EXECUTABLE)
    set(ZBC_PLATFORM_arm_FOUND FALSE)
    message(STATUS "arm platform: qemu-system-arm not found")
    return()
endif()

set(ZBC_PLATFORM_arm_FOUND TRUE)
set(ZBC_PLATFORM_arm_QEMU_BIN "${QEMU_ARM_EXECUTABLE}")
message(STATUS "arm platform: clang + ld.lld + qemu-system-arm available")

set(ZBC_PLATFORM_arm_CC "${CLANG_EXECUTABLE}")

set(ZBC_PLATFORM_arm_CFLAGS
    --target=arm-none-eabi
    -mcpu=cortex-a15
    # Force ARM (a32) instruction encoding; Thumb (t32) would also work
    # but ARM keeps the boot stub's relative branches simple.
    -marm
    # No VFP/Neon setup in the boot stub, so use soft-float ABI and
    # disable FP codegen.
    -mfloat-abi=soft
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_arm_LDFLAGS
    --target=arm-none-eabi
    -fuse-ld=lld
    -nostdlib
    -nostartfiles
    -no-pie
    -static
    -Wl,--build-id=none
)

set(ZBC_PLATFORM_arm_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crtarm.S
)

set(ZBC_PLATFORM_arm_EXTRA_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_composite.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_vcon.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_9p.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_virtio.c
)

set(ZBC_PLATFORM_arm_PLATFORM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform_init.c
    ${CMAKE_SOURCE_DIR}/test/target/common/qemu_platform_init.c
)

set(ZBC_PLATFORM_arm_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbcarm.ld
)
