/*
 * aarch64 (QEMU virt) boot stub
 *
 * Entry point for `qemu-system-aarch64 -machine virt -bios none -kernel
 * zbc_target_test_aarch64.elf`. The CPU starts at 0x40000000 (the virt
 * board's RAM base) in EL1; we set the stack pointer, zero .bss, and
 * call _start. The boot stub lives in its own .text.boot section so
 * the linker script can place it ahead of any other code -- QEMU jumps
 * unconditionally to the kernel image's load address, not to the
 * ELF entry point.
 *
 * Linker-provided symbols: _bss_start, _bss_end, _stack_top.
 */

    .section .text.boot, "ax"
    .globl _reset
_reset:
    /* Stack pointer: top of the linker-allocated stack. adrp + add
     * synthesises a 64-bit absolute address that doesn't depend on
     * pc-relative range. */
    adrp    x0, _stack_top
    add     x0, x0, :lo12:_stack_top
    mov     sp, x0

    /* Zero .bss in 8-byte strides. The linker script aligns both
     * _bss_start and _bss_end to 8 bytes, so the post-increment store
     * doesn't overshoot. */
    adrp    x0, _bss_start
    add     x0, x0, :lo12:_bss_start
    adrp    x1, _bss_end
    add     x1, x1, :lo12:_bss_end
1:
    cmp     x0, x1
    b.ge    2f
    str     xzr, [x0], #8
    b       1b
2:

    /* Hand off to the C entry point in zbc_target_test.c. */
    bl      _start

    /* SYS_EXIT should have terminated the VM via PSCI; if it didn't,
     * park the core so the runner's timeout takes over instead of
     * leaving a runaway. */
3:
    wfi
    b       3b
