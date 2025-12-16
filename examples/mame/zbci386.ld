/* ZBC i386 linker script
 *
 * Memory layout for ZBC (Zero Board Computer) with 32-bit address space:
 *   0x00001000-0xFFFEFDFF  Program area (code, data, bss, stack)
 *   0xFFFEFDE0-0xFFFEFDFF  Semihost device (32 bytes)
 *   0xFFFEFE00-0xFFFEFFFF  VRAM (512 bytes)
 *   0xFFFFFF00-0xFFFFFFEF  16-bit boot code and GDT
 *   0xFFFFFFF0-0xFFFFFFFF  Reset vector
 *
 * The i386 starts in 16-bit real mode at CS:IP = F000:FFF0.
 * With A20 enabled (ZBC default), the CPU fetches from 0xFFFFFFF0.
 * The boot stub switches to 32-bit protected mode and jumps to _start.
 */

ENTRY(_reset)

MEMORY {
    /* Main program area - starts at 4KB to avoid null pointer issues */
    ram     (rwx) : ORIGIN = 0x00001000, LENGTH = 0xFFFEDDFF

    /* 16-bit boot code and GDT at top of address space
     * CS.base = 0xFFFF0000, so code at 0xFFFFFF00 is CS:0xFF00 */
    boot16  (rx)  : ORIGIN = 0xFFFFFF00, LENGTH = 0xF0

    /* Reset vector at 0xFFFFFFF0 (CS:0xFFF0) */
    reset   (rx)  : ORIGIN = 0xFFFFFFF0, LENGTH = 0x10
}

/* Stack at top of RAM, below semihost device */
/* Semihost is at 0xFFFEFE00, so stack ends at 0xFFFEFDFC */
__stack_top = 0xFFFEFDFC;

SECTIONS {
    /* Main program code */
    .text 0x00001000 : {
        *(.text.startup)
        *(.text .text.*)
    } > ram

    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    } > ram

    /* Initialized data */
    .data : {
        *(.data .data.*)
    } > ram

    /* Uninitialized data */
    .bss : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    } > ram

    /* 16-bit boot code and GDT at top of address space */
    .boot16 0xFFFFFF00 : {
        *(.boot16)
    } > boot16

    /* Reset vector at 0xFFFFFFF0 */
    .reset 0xFFFFFFF0 : {
        *(.reset)
    } > reset

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
    }
}
