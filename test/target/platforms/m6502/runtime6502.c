/*
 * Minimal 6502 Runtime Support
 *
 * Provides compiler runtime functions needed for bare-metal 6502 code.
 * These are normally provided by llvm-mos-sdk but we want to be standalone.
 */

#include <stddef.h>

/*
 * __memset - memory fill used by compiler for struct initialization
 */
void __memset(char *ptr, char value, size_t num)
{
    for (; num; ptr++, num--)
        *ptr = value;
}

/*
 * __udivmodhi4 - 16-bit unsigned division with remainder
 * Returns quotient, stores remainder in *rem
 */
unsigned __udivmodhi4(unsigned a, unsigned b, unsigned *rem)
{
    unsigned q = 0;

    if (!b || b > a) {
        *rem = a;
        return 0;
    }

    /* Simple subtraction-based division */
    while (a >= b) {
        a -= b;
        q++;
    }

    *rem = a;
    return q;
}

/*
 * Provide division operators that use __udivmodhi4
 */
unsigned __udivhi3(unsigned a, unsigned b)
{
    unsigned rem;
    return __udivmodhi4(a, b, &rem);
}

unsigned __umodhi3(unsigned a, unsigned b)
{
    unsigned rem;
    __udivmodhi4(a, b, &rem);
    return rem;
}

/*
 * __mulhi3 - 16-bit unsigned multiplication
 */
unsigned __mulhi3(unsigned a, unsigned b)
{
    unsigned result = 0;
    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}
