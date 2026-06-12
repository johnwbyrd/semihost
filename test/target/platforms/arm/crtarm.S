/*
 * arm (QEMU virt, 32-bit) boot stub
 *
 * Same shape as the aarch64 boot stub but in ARMv7-A: the CPU starts
 * at the kernel load address (the virt board's RAM base, 0x40000000)
 * in PL1 non-secure SVC mode with FP disabled. We set the stack
 * pointer, zero .bss, and call _start. QEMU jumps unconditionally to
 * the kernel image's load address regardless of the ELF entry point,
 * which is why the boot stub lives in its own .text.boot section that
 * the linker script places ahead of any other .text.
 *
 * Linker-provided symbols: _bss_start, _bss_end, _stack_top.
 */

    .section .text.boot, "ax"
    .arm
    .globl _reset
_reset:
    /* Stack pointer: top of the linker-allocated stack. ldr Rd, =sym
     * emits a PC-relative load from the literal pool, which the linker
     * fills with the absolute address. */
    ldr     sp, =_stack_top

    /* Zero .bss in 4-byte strides. The linker script aligns _bss_end
     * up to 4 bytes. */
    ldr     r0, =_bss_start
    ldr     r1, =_bss_end
    mov     r2, #0
1:
    cmp     r0, r1
    bge     2f
    str     r2, [r0], #4
    b       1b
2:

    /* Hand off to the C entry point in zbc_target_test.c. */
    bl      _start

    /* SYS_EXIT should have terminated the VM via PSCI. If it didn't,
     * park the core so the runner's timeout takes over instead of
     * leaving a runaway. */
3:
    wfi
    b       3b
