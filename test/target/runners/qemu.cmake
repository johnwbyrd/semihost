# QEMU Runner for ZBC Target Tests (Stub)
#
# Placeholder for future QEMU support.
# QEMU would need a ZBC machine implementation similar to MAME.

set(ZBC_RUNNER_QEMU_FOUND FALSE)
message(STATUS "QEMU runner: not implemented (future)")

function(add_qemu_test name elf_path machine)
    message(STATUS "Skipping QEMU test ${name} - QEMU runner not implemented")
endfunction()
