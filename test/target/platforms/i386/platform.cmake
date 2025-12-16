# i386 Platform Configuration for ZBC Target Tests
#
# Detects gcc -m32 capability and sets up cross-compilation.

set(ZBC_PLATFORM_NAME "i386")
set(ZBC_PLATFORM_MAME_MACHINE "zbci386")

# Check for 32-bit capable GCC
find_program(GCC_EXECUTABLE gcc)

if(GCC_EXECUTABLE)
    # Test if gcc supports -m32
    execute_process(
        COMMAND ${GCC_EXECUTABLE} -m32 -x c -E -
        INPUT_FILE /dev/null
        OUTPUT_QUIET
        ERROR_QUIET
        RESULT_VARIABLE GCC_M32_RESULT
    )

    if(GCC_M32_RESULT EQUAL 0)
        set(ZBC_PLATFORM_i386_FOUND TRUE)
        message(STATUS "i386 platform: gcc -m32 available")
    else()
        set(ZBC_PLATFORM_i386_FOUND FALSE)
        message(STATUS "i386 platform: gcc found but -m32 not supported (install gcc-multilib)")
        return()
    endif()
else()
    set(ZBC_PLATFORM_i386_FOUND FALSE)
    message(STATUS "i386 platform: gcc not found")
    return()
endif()

# Platform-specific compiler and flags
set(ZBC_PLATFORM_i386_CC "${GCC_EXECUTABLE}")

set(ZBC_PLATFORM_i386_CFLAGS
    -m32
    -march=i386
    -nostdlib
    -nostartfiles
    -ffreestanding
    -fno-pie
    -O2
    -Wall
    -Wextra
)

set(ZBC_PLATFORM_i386_LDFLAGS
    -m32
    -no-pie
    -Wl,--build-id=none
)

# Platform support files
set(ZBC_PLATFORM_i386_CRT
    ${CMAKE_CURRENT_LIST_DIR}/crti386.S
)

set(ZBC_PLATFORM_i386_LD
    ${CMAKE_CURRENT_LIST_DIR}/zbci386.ld
)
