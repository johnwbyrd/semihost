/*
 * QEMU `virt` machine memory map for riscv32. RAM starts at
 * 0x80000000 and the machine defaults to 128 MiB; we only need a few
 * tens of KiB for the test binary plus the stack.
 */

OUTPUT_ARCH(riscv)
ENTRY(_reset)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

SECTIONS {
    .text 0x80000000 : ALIGN(4) {
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
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* 16 KiB stack at the top of the consumed region. */
    .stack ALIGN(16) : {
        . += 16K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.note*) *(.eh_frame) *(.comment) *(.riscv.attributes) }
}
