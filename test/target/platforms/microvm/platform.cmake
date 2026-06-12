# microvm Platform Configuration for ZBC Target Tests
#
# i386 ZBC target running under QEMU's "microvm" machine, which is a
# stripped-down x86 board: no BIOS, PVH boot protocol, virtio-mmio for
# all devices (no PCI), and isa-debug-exit for termination. This is the
# x86 entry into the QEMU virt-class story -- proves the same client
# binary that boots on riscv* and aarch64 also boots on a stock x86
# emulator with no firmware in sight.
#
# Toolchain: clang's built-in i386 backend + ld.lld; no separate
# cross-compiler. Same trick the riscv* and aarch64 platforms use.

set(ZBC_PLATFORM_NAME "microvm")
set(ZBC_PLATFORM_microvm_RUNNERS qemu)
set(ZBC_PLATFORM_microvm_QEMU_MACHINE "microvm")

# microvm doesn't load a BIOS by default, but it also doesn't expose a
# termination device by default: -device isa-debug-exit adds the
# I/O-port-write-to-exit mechanism the platform_init.c hook uses.
# Without this, the only way to stop QEMU is the runner timeout.
set(ZBC_PLATFORM_microvm_QEMU_EXTRA_ARGS
    -device isa-debug-exit,iobase=0xf4,iosize=0x01
)

# microvm exits with a non-zero process code on every isa-debug-exit
# write (it computes (val<<1)|1, so 0 is unreachable). The harness
# already prints "RESULT: PASS" or "RESULT: FAIL" on stdout, so we
# grade by that instead -- handled by the runner via PASS/FAIL regex
# on every QEMU test (uniform across platforms; see qemu.cmake).

find_program(CLANG_EXECUTABLE clang)
find_program(LD_LLD_EXECUTABLE ld.lld)
find_program(QEMU_I386_EXECUTABLE qemu-system-i386)

if(NOT CLANG_EXECUTABLE)
    set(ZBC_PLATFORM_microvm_FOUND FALSE)
    message(STATUS "microvm platform: clang not found")
    return()
endif()

if(NOT LD_LLD_EXECUTABLE)
    set(ZBC_PLATFORM_microvm_FOUND FALSE)
    message(STATUS "microvm platform: ld.lld not found (install lld)")
    return()
endif()

if(NOT QEMU_I386_EXECUTABLE)
    set(ZBC_PLATFORM_microvm_FOUND FALSE)
    message(STATUS "microvm platform: qemu-system-i386 not found")
    return()
endif()

set(ZBC_PLATFORM_microvm_FOUND TRUE)
set(ZBC_PLATFORM_microvm_QEMU_BIN "${QEMU_I386_EXECUTABLE}")
message(STATUS "microvm platform: clang + ld.lld + qemu-system-i386 available")

set(ZBC_PLATFORM_microvm_CC "${CLANG_EXECUTABLE}")

set(ZBC_PLATFORM_microvm_CFLAGS
    --target=i386-unknown-elf
    -march=i386
    # No SSE/FPU init in the boot stub, so disable codegen that would
    # require it.
    -mno-sse
    -mno-mmx
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_microvm_LDFLAGS
    --target=i386-unknown-elf
    -fuse-ld=lld
    -nostdlib
    -nostartfiles
    # Produce a plain ET_EXEC at fixed addresses; without these,
    # clang's i386-unknown-elf driver defaults to a PIE/DYN image
    # with a dynamic linker interpreter, which PVH boot can't use.
    -no-pie
    -static
    -Wl,--build-id=none
    # Bare-metal i386 code is absolute, not PIC. ld.lld refuses to
    # write R_386_32 relocations into an executable .text section by
    # default; -z notext relaxes that check (GNU ld accepts it
    # implicitly).
    -Wl,-z,notext
)

set(ZBC_PLATFORM_microvm_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crtmicrovm.S
)

set(ZBC_PLATFORM_microvm_EXTRA_CLIENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_composite.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_vcon.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_transport_9p.c
    ${CMAKE_SOURCE_DIR}/src/c/zbc_virtio.c
)

set(ZBC_PLATFORM_microvm_PLATFORM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/platform_init.c
    ${CMAKE_SOURCE_DIR}/test/target/common/qemu_platform_init.c
)

set(ZBC_PLATFORM_microvm_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbcmicrovm.ld
)
