; Minimal 6502 test - just write to VRAM
; VRAM is at 0xFD00 for 16-bit ZBC

.text
.global _start
_start:
    lda #'H'
    sta $FD00
    lda #'I'
    sta $FD01
    lda #'!'
    sta $FD02
loop:
    jmp loop
