/*
 * microvm (QEMU) boot stub
 *
 * Uses the PVH boot protocol so QEMU loads the ELF directly and jumps
 * to _pvh_entry in 32-bit protected mode. Much simpler than the i386
 * (MAME) platform's real-mode -> protected-mode dance: PVH delivers us
 * already in protected mode with flat segments, A20 enabled, paging
 * off, and interrupts disabled. EBX points at the hvm_start_info
 * struct (which we don't use), EAX carries the magic 0x336EC578.
 *
 * The .note.Xen note tells QEMU where to enter via the PHYS32_ENTRY
 * notetype (0x12); without it microvm falls back to a multiboot1 /
 * linux-loader search that wouldn't find anything.
 *
 * Linker-provided symbols: _bss_start, _bss_end, _stack_top.
 */

/* PVH boot note (Xen ELFNOTE_PHYS32_ENTRY = 0x12).
 *
 * The note section format is name + desc with both rounded up to a
 * 4-byte boundary; name "Xen" + NUL fits in 4 bytes, the desc is the
 * 32-bit entry address. */
    .section .note.Xen, "a", @note
    .balign 4
    .long   4                 /* n_namesz: 4 = strlen("Xen") + NUL */
    .long   4                 /* n_descsz: 4 = sizeof(uint32_t) */
    .long   0x12              /* n_type = XEN_ELFNOTE_PHYS32_ENTRY */
    .ascii  "Xen\0"           /* name + NUL, 4 bytes, no extra pad */
    .long   _pvh_entry        /* desc: 32-bit entry address */

    .section .text.boot, "ax", @progbits
    .code32
    .globl _pvh_entry
_pvh_entry:
    /* Load flat stack pointer at the top of the linker-reserved region. */
    movl    $_stack_top, %esp

    /* Clear .bss in 1-byte strides via rep stosb. The linker doesn't
     * have to align _bss_end up because we use byte stores. */
    cld
    movl    $_bss_start, %edi
    movl    $_bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep stosb

    /* Hand off to the C entry point in zbc_target_test.c. */
    call    _start

    /* Should not return -- SYS_EXIT terminates the VM via the
     * platform fallback. If it does, halt the CPU so the runner's
     * timeout catches it instead of leaving a runaway. */
1:
    hlt
    jmp     1b
