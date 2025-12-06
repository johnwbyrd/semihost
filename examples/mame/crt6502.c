/* crt6502.c - Minimal runtime support for llvm-mos
 *
 * Provides runtime functions required by llvm-mos generated code.
 * These are shift operations that the 6502 doesn't have natively.
 */

/* 16-bit left shift */
unsigned int __ashlhi3(unsigned int n, char amt) {
    while (amt--)
        n <<= 1;
    return n;
}

/* 32-bit left shift */
unsigned long __ashlsi3(unsigned long n, char amt) {
    while (amt--)
        n <<= 1;
    return n;
}

/* 16-bit unsigned right shift */
unsigned int __lshrhi3(unsigned int n, char amt) {
    while (amt--)
        n >>= 1;
    return n;
}

/* 32-bit unsigned right shift */
unsigned long __lshrsi3(unsigned long n, char amt) {
    while (amt--)
        n >>= 1;
    return n;
}

/* 16-bit signed right shift */
int __ashrhi3(int n, char amt) {
    while (amt--)
        n >>= 1;
    return n;
}

/* 32-bit signed right shift */
long __ashrsi3(long n, char amt) {
    while (amt--)
        n >>= 1;
    return n;
}
