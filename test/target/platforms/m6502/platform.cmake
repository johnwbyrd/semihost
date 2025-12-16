# m6502 Platform Configuration for ZBC Target Tests
#
# Detects llvm-mos compiler and sets up cross-compilation.

set(ZBC_PLATFORM_NAME "m6502")
set(ZBC_PLATFORM_MAME_MACHINE "zbcm6502")

# Look for mos-clang (llvm-mos compiler)
find_program(LLVM_MOS_CLANG mos-clang
    HINTS
        $ENV{LLVM_MOS}/bin
        ${LLVM_MOS}/bin
        $ENV{HOME}/git/llvm-mos/build/install/bin
    DOC "llvm-mos clang compiler (mos-clang)"
)

if(LLVM_MOS_CLANG)
    set(ZBC_PLATFORM_m6502_FOUND TRUE)
    message(STATUS "m6502 platform: mos-clang found at ${LLVM_MOS_CLANG}")
else()
    set(ZBC_PLATFORM_m6502_FOUND FALSE)
    message(STATUS "m6502 platform: mos-clang not found (set LLVM_MOS)")
    return()
endif()

# Platform-specific compiler and flags
set(ZBC_PLATFORM_m6502_CC "${LLVM_MOS_CLANG}")

set(ZBC_PLATFORM_m6502_CFLAGS
    -nostdlib
    -nostartfiles
    -ffreestanding
    -Oz
    -fno-inline-functions
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_m6502_LDFLAGS
    ""
)

# Platform support files (CRT + runtime library)
set(ZBC_PLATFORM_m6502_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crt6502.S
    ${CMAKE_CURRENT_LIST_DIR}/crt6502.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime6502.S
    ${CMAKE_CURRENT_LIST_DIR}/runtime6502.c
)

set(ZBC_PLATFORM_m6502_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbc6502.ld
)
