/* crt6502.S - Minimal runtime support for llvm-mos
 *
 * Provides runtime functions required by llvm-mos generated code.
 */

.include "imag.S"

.text

/* __call_indir - Indirect call through __rc18/__rc19 (RS9)
 * This performs an indirect JSR via JMP. The RTS in the callee
 * returns to our caller since we used JMP not JSR.
 */
.global __call_indir
__call_indir:
    jmp (__rc18)
