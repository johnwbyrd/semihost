; Minimal 6502 Runtime Support - Assembly constants
;
; Location that when used with BIT will set V flag.
; Compensates for the lack of a SEV instruction on 6502.

.section .rodata,"a",@progbits

.global __set_v
__set_v:
  .byte 0xFF
