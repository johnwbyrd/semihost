/*
 * riscv64 (QEMU virt) boot stub
 *
 * Same shape as the riscv32 boot stub but with 64-bit register width:
 * the stack pointer is 8 bytes and .bss zeroing strides 8 bytes at a
 * time via sd (the linker script aligns _bss_end up to 8).
 *
 * QEMU's `-bios none -kernel` ROM trampoline jumps to 0x80000000
 * unconditionally regardless of the ELF entry, so this stub lives in
 * its own .text.boot section that the linker script places ahead of
 * any other .text.
 *
 * Linker-provided symbols: _bss_start, _bss_end, _stack_top.
 */

    .section .text.boot, "ax"
    .globl _reset
_reset:
    /* Stack pointer: top of the linker-allocated stack region. */
    la      sp, _stack_top

    /* Zero .bss in 8-byte strides. The linker rounds _bss_end up to a
     * multiple of 8. */
    la      t0, _bss_start
    la      t1, _bss_end
1:
    bge     t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:

    /* Hand off to the C entry point. */
    call    _start

    /* Should not return. Park the hart so the runner's timeout takes
     * over instead of leaving a runaway. */
3:
    wfi
    j       3b
