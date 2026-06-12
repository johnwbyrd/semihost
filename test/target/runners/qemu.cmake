# QEMU Runner for ZBC Target Tests
#
# Each per-test invocation gets its own sandbox directory under
# ${CMAKE_CURRENT_BINARY_DIR}/qemu-sandbox/<test-name> that is mounted
# into the guest as 9p mount_tag "zbc". Console traffic rides
# virtio-console wired to QEMU's stdio chardev so the test binary's
# TARGET_PRINT output reaches ctest.
#
# A per-platform platform.cmake supplies the specific
# qemu-system-<arch> binary, the QEMU machine name, and any extra
# arguments via ZBC_PLATFORM_<p>_QEMU_BIN / _QEMU_MACHINE /
# _QEMU_EXTRA_ARGS.

set(ZBC_RUNNER_QEMU_FOUND TRUE)
message(STATUS "QEMU runner: enabled (per-platform binary detection)")

#
# add_qemu_test(name
#               ELF      <path>
#               QEMU_BIN <qemu-system-* path>
#               MACHINE  <machine>
#               [EXTRA_ARGS <list>]
#               [TIMEOUT <seconds>]
#               [INPUT   <stdin text>])
#
# Registers a CTest test that boots an ELF under qemu-system-<arch>
# with a virtio-console + virtio-9p loadout. The per-test sandbox at
# ${CMAKE_CURRENT_BINARY_DIR}/qemu-sandbox/<name> is created at
# configure time and exposed as 9p mount_tag "zbc".
#
function(add_qemu_test name)
    cmake_parse_arguments(ARG
        ""                                          # options
        "ELF;QEMU_BIN;MACHINE;TIMEOUT;INPUT"        # one-value
        "EXTRA_ARGS"                                # multi-value
        ${ARGN})

    if(NOT ARG_ELF)
        message(FATAL_ERROR "add_qemu_test(${name}) missing ELF")
    endif()
    if(NOT ARG_QEMU_BIN)
        message(FATAL_ERROR "add_qemu_test(${name}) missing QEMU_BIN")
    endif()
    if(NOT ARG_MACHINE)
        message(FATAL_ERROR "add_qemu_test(${name}) missing MACHINE")
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 30)
    endif()

    set(sandbox_dir ${CMAKE_CURRENT_BINARY_DIR}/qemu-sandbox/${name})
    file(MAKE_DIRECTORY ${sandbox_dir})

    # No -global virtio-mmio.force-legacy=false: the ZBC client now
    # handles both legacy (v1, the QEMU virt default) and modern (v2)
    # virtio-mmio, so a guest binary boots on stock QEMU as-is.
    #
    # -bios is per-platform: qemu-system-riscv32 needs "-bios none" to
    # skip OpenSBI, but qemu-system-aarch64 treats "none" as a file
    # path and fails. Each platform's QEMU_EXTRA_ARGS contributes what
    # it needs.
    set(qemu_cmd
        ${ARG_QEMU_BIN}
        -machine ${ARG_MACHINE}
        -no-reboot
        -display none
        -monitor none
        -serial none
        -kernel ${ARG_ELF}
        -fsdev local,id=fs0,path=${sandbox_dir},security_model=none
        -device virtio-9p-device,fsdev=fs0,mount_tag=zbc
        -device virtio-serial-device
        -device virtconsole,chardev=c0
        -chardev stdio,id=c0,signal=off
        ${ARG_EXTRA_ARGS}
    )

    if(ARG_INPUT)
        # Pipe stdin through sh so SYS_READC has something to read.
        string(JOIN " " qemu_cmd_str ${qemu_cmd})
        add_test(
            NAME ${name}
            COMMAND sh -c "echo '${ARG_INPUT}' | ${qemu_cmd_str}"
        )
    else()
        add_test(NAME ${name} COMMAND ${qemu_cmd})
    endif()

    set_tests_properties(${name} PROPERTIES
        LABELS "target;qemu;${ARG_MACHINE}"
        TIMEOUT ${ARG_TIMEOUT}
    )

    message(STATUS "Registered QEMU test: ${name} (${ARG_MACHINE})")
endfunction()
