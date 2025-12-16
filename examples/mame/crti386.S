/*
 * x86 Boot Stub for ZBC
 *
 * The i386 CPU starts in 16-bit real mode at CS:IP = 0xF000:0xFFF0,
 * which maps to physical address 0xFFFF0000 + 0xFFF0 = 0xFFFFFFF0.
 *
 * ZBC runs with A20 enabled, so we place boot code at the actual addresses:
 *   - Reset vector at 0xFFFFFFF0
 *   - Boot16 code at 0xFFFFFF00
 *
 * This boot stub:
 * 1. Starts at 0xFFFFFFF0 (reset vector)
 * 2. Loads a GDT with flat 32-bit segments
 * 3. Switches to 32-bit protected mode
 * 4. Jumps to _start
 */

/*
 * 32-bit protected mode entry point - goes in main .text
 */
.code32
.section .text.startup, "ax", @progbits
.global _start32
_start32:
    /* Set up data segments with selector 0x10 (data segment) */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up stack pointer - use top of low memory before semihost device */
    /* For 32-bit ZBC: semihost at 0xFFFEFE00, so stack just below */
    mov $0xFFFEFDFC, %esp

    /* Jump to C entry point */
    jmp _start

/*
 * 16-bit boot code and data at 0xFFFFFF00-0xFFFFFFFF
 */
.code16
.section .boot16, "ax", @progbits

/* Global Descriptor Table */
.align 8
_gdt:
    /* Null descriptor (required) */
    .quad 0x0000000000000000

    /* Code segment: base=0, limit=4GB, 32-bit, executable, readable */
    .word 0xFFFF        /* Limit 15:0 */
    .word 0x0000        /* Base 15:0 */
    .byte 0x00          /* Base 23:16 */
    .byte 0x9A          /* Access: present, ring 0, code, exec/read */
    .byte 0xCF          /* Granularity: 4KB, 32-bit, limit 19:16 = 0xF */
    .byte 0x00          /* Base 31:24 */

    /* Data segment: base=0, limit=4GB, 32-bit, writable */
    .word 0xFFFF        /* Limit 15:0 */
    .word 0x0000        /* Base 15:0 */
    .byte 0x00          /* Base 23:16 */
    .byte 0x92          /* Access: present, ring 0, data, read/write */
    .byte 0xCF          /* Granularity: 4KB, 32-bit, limit 19:16 = 0xF */
    .byte 0x00          /* Base 31:24 */
_gdt_end:

_gdt_ptr:
    .word _gdt_end - _gdt - 1   /* GDT limit (23) */
    .long _gdt                   /* GDT linear base address */

/*
 * Mode switch code - in .boot16 section at 0xFFFFFF00
 * This is jumped to from the reset vector.
 *
 * Layout of .boot16 section at 0xFFFFFF00:
 *   0xFFFFFF00: _gdt (24 bytes = 3 descriptors)
 *   0xFFFFFF18: _gdt_ptr (6 bytes)
 *   0xFFFFFF1E: _setup16 (mode switch code)
 *
 * CS.base = 0xFFFF0000, so CS-relative offsets are:
 *   _gdt_ptr = 0xFFFFFF18 - 0xFFFF0000 = 0xFF18
 *   _setup16 = 0xFFFFFF1E - 0xFFFF0000 = 0xFF1E
 */
_setup16:
    cli                         /* Disable interrupts */

    /* A20 is already enabled by ZBC - no need to toggle port 0x92 */

    /* Load GDT using CS-relative addressing with hardcoded offset */
    .byte 0x66                  /* operand size prefix for 32-bit GDT ptr */
    lgdt %cs:0xFF18             /* _gdt_ptr at CS:0xFF18 */

    /* Enable protected mode (set PE bit in CR0) */
    mov %cr0, %eax
    or $1, %al
    mov %eax, %cr0

    /* Far jump to 32-bit code - flushes prefetch, loads CS */
    /* In 16-bit mode, we need explicit encoding for 32-bit far jump */
    .byte 0x66                  /* operand size override */
    .byte 0xEA                  /* far jump opcode */
    .long _start32              /* 32-bit offset */
    .word 0x0008                /* segment selector */

/*
 * Reset vector at 0xFFFFFFF0 - must fit in 16 bytes
 * Jump backwards to the setup code in .boot16
 */
.section .reset, "ax", @progbits
.global _reset
_reset:
    /* Near jump to _setup16 in CS segment
     * _setup16 is at CS:0xFF1E (0xFFFFFF1E)
     * We're at CS:0xFFF0 (0xFFFFFFF0)
     */
    jmp _setup16                /* Near jump within CS segment */
    .fill 13, 1, 0x90           /* NOP padding to fill 16 bytes */
