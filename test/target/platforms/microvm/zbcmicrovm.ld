/*
 * QEMU microvm memory map. We pick 0x01000000 (16 MiB) for the kernel
 * base: well above the 1 MiB conventional / VGA hole, plenty of slack
 * below the 0xFEB00000 virtio-mmio window microvm exposes.
 *
 * KEEP(*(.note.Xen)) keeps the PVH boot note in the loaded image;
 * QEMU's microvm loader walks the ELF notes at load time looking for
 * XEN_ELFNOTE_PHYS32_ENTRY to decide where to jump.
 */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_pvh_entry)

MEMORY {
    RAM (rwx) : ORIGIN = 0x01000000, LENGTH = 16M
}

SECTIONS {
    /* PVH note in its own output section so the linker keeps its
     * SHT_NOTE type (it would otherwise be merged into .text and
     * QEMU's PVH loader would not find it). Anchored at the RAM base
     * so it falls at a predictable offset in the ELF. ALIGN(4) here:
     * 32-bit ELF notes use 4-byte alignment per the spec; ALIGN(8)
     * confused readelf and (likely) QEMU's note walker. */
    .note.Xen 0x01000000 : ALIGN(4) {
        KEEP(*(.note.Xen))
    } > RAM

    .text : ALIGN(8) {
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
        _bss_end = .;
    } > RAM

    /* 16 KiB stack at the top of the consumed region. */
    .stack ALIGN(16) : {
        . += 16K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : { *(.eh_frame) *(.comment) *(.note.GNU-stack) }
}
