/*
 * QEMU `virt` machine memory map for aarch64. RAM starts at
 * 0x40000000 and the machine defaults to 1 GiB; we use only a small
 * region for code + stack.
 */

OUTPUT_ARCH(aarch64)
ENTRY(_reset)

MEMORY {
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 16M
}

SECTIONS {
    .text 0x40000000 : ALIGN(8) {
        /* Boot stub first -- QEMU jumps to the kernel load address,
         * ignoring ENTRY(). */
        KEEP(*(.text.boot))
        *(.text.startup)
        *(.text*)
    } > RAM

    .rodata : ALIGN(8) {
        *(.rodata*)
    } > RAM

    .data : ALIGN(8) {
        *(.data*)
    } > RAM

    .bss : ALIGN(8) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

    /* 16 KiB stack at the top of the consumed region. */
    .stack ALIGN(16) : {
        . += 16K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.note*) *(.eh_frame) *(.comment) *(.ARM.attributes) }
}
