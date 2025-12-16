/* ZBC 6502 linker script
 *
 * Memory layout for ZBC (Zero Board Computer) with 16-bit address space:
 *   0x0000-0x001F  Imaginary registers (__rc0-__rc31)
 *   0x0020-0x00FF  Zero page (user variables)
 *   0x0100-0x01FF  Stack (6502 hardware stack)
 *   0x0200-0xFCBF  Program area (load address = 2^(1+16/2) = 0x200)
 *   0xFCC0-0xFCDF  Reserved for soft stack (grows down)
 *   0xFCE0-0xFCFF  Semihost device (32 bytes)
 *   0xFD00-0xFEFF  VRAM (512 bytes)
 *   0xFF00-0xFFFF  Reserved area (256 bytes)
 */

/* Provide imaginary (zero page) registers for llvm-mos */
__rc0 = 0x00;
__rc1 = __rc0 + 1;
PROVIDE(__rc2 = __rc1 + 1);
__rc3 = __rc2 + 1;
PROVIDE(__rc4 = __rc3 + 1);
__rc5 = __rc4 + 1;
PROVIDE(__rc6 = __rc5 + 1);
__rc7 = __rc6 + 1;
PROVIDE(__rc8 = __rc7 + 1);
__rc9 = __rc8 + 1;
PROVIDE(__rc10 = __rc9 + 1);
__rc11 = __rc10 + 1;
PROVIDE(__rc12 = __rc11 + 1);
__rc13 = __rc12 + 1;
PROVIDE(__rc14 = __rc13 + 1);
__rc15 = __rc14 + 1;
PROVIDE(__rc16 = __rc15 + 1);
__rc17 = __rc16 + 1;
PROVIDE(__rc18 = __rc17 + 1);
__rc19 = __rc18 + 1;
PROVIDE(__rc20 = __rc19 + 1);
__rc21 = __rc20 + 1;
PROVIDE(__rc22 = __rc21 + 1);
__rc23 = __rc22 + 1;
PROVIDE(__rc24 = __rc23 + 1);
__rc25 = __rc24 + 1;
PROVIDE(__rc26 = __rc25 + 1);
__rc27 = __rc26 + 1;
PROVIDE(__rc28 = __rc27 + 1);
__rc29 = __rc28 + 1;
PROVIDE(__rc30 = __rc29 + 1);
__rc31 = __rc30 + 1;

MEMORY {
    zp      (rw)  : ORIGIN = __rc31 + 1, LENGTH = 0x100 - (__rc31 + 1)
    ram     (rwx) : ORIGIN = 0x0200, LENGTH = 0xFAC0    /* 0x200 - 0xFCBF */
    vectors (r)   : ORIGIN = 0xFFFA, LENGTH = 6         /* 6502 vectors */
}

REGION_ALIAS("c_readonly", ram);
REGION_ALIAS("c_writeable", ram);

/* Soft stack grows down from just before semihost device */
__stack = 0xFCDF;

SECTIONS {
    /* Code - entry point first */
    .text : {
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
        *(.bss .bss.* COMMON)
    } > ram

    /* Zero page variables (6502 optimization) */
    .zp.bss (NOLOAD) : {
        *(.zp.bss .zp.bss.*)
    } > zp

    .zp.data : {
        *(.zp.data .zp.data.*)
    } > zp AT > ram

    /* 6502 interrupt vectors at 0xFFFA-0xFFFF */
    .vectors : {
        SHORT(0x0000)   /* 0xFFFA: NMI vector (unused) */
        SHORT(_start)   /* 0xFFFC: Reset vector -> _start */
        SHORT(0x0000)   /* 0xFFFE: IRQ/BRK vector (unused) */
    } > vectors

    /* Discard exception handling (not needed for bare metal) */
    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}
