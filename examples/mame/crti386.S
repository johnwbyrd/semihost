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

    /* Set up stack pointer from linker script symbol */
    mov $__stack_top, %esp

    /* Jump to C entry point */
    jmp _start

/*
 * 16-bit boot code and data at 0xFFFFFF00-0xFFFFFFFF
 *
 * The .boot16 section is placed at 0xFFFFFF00 by the linker script.
 * In real mode, CS.base = 0xFFFF0000, so addresses within this section
 * have CS-relative offsets of (linear_addr - 0xFFFF0000).
 *
 * Layout:
 *   0xFFFFFF00 (_gdt):     GDT (24 bytes = 3 descriptors)
 *   0xFFFFFF18 (_gdt_ptr): GDT pointer (6 bytes)
 *   0xFFFFFF1E (_setup16): Mode switch code
 */
.equ CS_BASE,      0xFFFF0000
.equ BOOT16_START, 0xFFFFFF00
.equ GDT_PTR_CS,   (BOOT16_START + 24) - CS_BASE  /* 0xFF18 */

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
 * Mode switch code - jumped to from the reset vector
 */
_setup16:
    cli                         /* Disable interrupts */

    /* A20 is already enabled by ZBC */

    /* Load GDT using CS-relative addressing (see GDT_PTR_CS above) */
    .byte 0x66                  /* operand size prefix for 32-bit GDT ptr */
    lgdt %cs:GDT_PTR_CS         /* _gdt_ptr at CS:0xFF18 */

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
 */
.section .reset, "ax", @progbits
.global _reset
_reset:
    jmp _setup16                /* Near jump to mode switch code */
    .fill 13, 1, 0x90           /* NOP padding to fill 16 bytes */
