/*
 * QEMU `virt` machine memory map for riscv64. Same RAM base as
 * riscv32 (the QEMU virt machine is shared between 32- and 64-bit
 * RISC-V), so the only differences from zbcriscv32.ld are 8-byte
 * alignment for _bss_end and the OUTPUT_ARCH suffix.
 */

OUTPUT_ARCH(riscv)
ENTRY(_reset)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

SECTIONS {
    .text 0x80000000 : ALIGN(8) {
        /* Boot stub first -- QEMU's ROM trampoline jumps to 0x80000000
         * unconditionally, ignoring ENTRY(). */
        KEEP(*(.text.boot))
        *(.text.startup)
        *(.text*)
    } > RAM

    .rodata : ALIGN(8) {
        *(.rodata*)
        *(.srodata*)
    } > RAM

    .data : ALIGN(8) {
        *(.data*)
        *(.sdata*)
    } > RAM

    .bss : ALIGN(8) {
        _bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

    /* 16 KiB stack at the top of the consumed region. */
    .stack ALIGN(16) : {
        . += 16K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.note*) *(.eh_frame) *(.comment) *(.riscv.attributes) }
}
