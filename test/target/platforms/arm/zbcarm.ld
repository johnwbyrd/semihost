/*
 * QEMU `virt` machine memory map for arm (32-bit). The virt machine
 * is shared between 32-bit and 64-bit ARM with the same memory layout,
 * so this mirrors zbcaarch64.ld except for OUTPUT_FORMAT / OUTPUT_ARCH.
 */

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
ENTRY(_reset)

MEMORY {
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 16M
}

SECTIONS {
    .text 0x40000000 : ALIGN(4) {
        /* Boot stub first -- QEMU jumps to the kernel load address,
         * ignoring ENTRY(). */
        KEEP(*(.text.boot))
        *(.text.startup)
        *(.text*)
    } > RAM

    .rodata : ALIGN(4) {
        *(.rodata*)
    } > RAM

    .data : ALIGN(4) {
        *(.data*)
    } > RAM

    .bss : ALIGN(4) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* 16 KiB stack at the top of the consumed region. */
    .stack ALIGN(8) : {
        . += 16K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.note*) *(.eh_frame) *(.comment) *(.ARM.attributes) *(.ARM.exidx*) }
}
