/*
 * riscv32 (QEMU virt) boot stub
 *
 * Entry point for `qemu-system-riscv32 -machine virt -bios none -kernel
 * zbc_target_test_riscv32.elf`. QEMU loads the ELF, parks hart 0 at the
 * ELF entry, and lets it run. We set the stack pointer, clear .bss, and
 * call _start (the C entry in zbc_target_test.c, matching the i386
 * platform's _start32 -> _start handoff). If _start returns we spin --
 * semihosting SYS_EXIT is what actually stops the VM, and the QEMU
 * runner enforces a timeout in case it doesn't.
 *
 * Symbols _bss_start, _bss_end, and _stack_top are provided by the
 * linker script.
 */

    /* Dedicated section so the linker can place this AT 0x80000000,
     * ahead of any other .text. QEMU's built-in ROM trampoline jumps
     * to 0x80000000 unconditionally, ignoring the ELF entry point, so
     * the boot stub MUST be the first instruction at that address. */
    .section .text.boot, "ax"
    .globl _reset
_reset:
    /* Stack pointer: top of the linker-allocated stack region. */
    la      sp, _stack_top

    /* Zero .bss. The linker rounds _bss_end up to a 4-byte boundary. */
    la      t0, _bss_start
    la      t1, _bss_end
1:
    bge     t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    /* Hand off to the C entry point. */
    call    _start

    /* Should not return. Park the hart so the runner's timeout takes
     * over instead of leaving a runaway. */
3:
    wfi
    j       3b
