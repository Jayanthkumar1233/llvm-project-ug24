/* 05_float - soft floating point.
 *
 * The heaviest program in the kit: the uG24 has no FPU, so every operation
 * below is a library call, and printing one is an integer reconstruction of
 * the decimal digits.  It is the best single check that the whole toolchain
 * and runtime survived a round trip.
 *
 * Note that `double` is IEEE *single* precision on this target -- 32 bits, like
 * AVR -- so these values carry about seven significant digits and no more.
 * 355.0/113.0 really is 3.141593 here; a host printing 3.14159292 is using a
 * 64-bit double and is not disagreeing with us.
 */
#include <stdio.h>

int main(void)
{
    printf("uG24 sample 5: floating point\n");

    float a = 355.0f, b = 113.0f;
    printf("  355 / 113        = %f\n", (double)(a / b));

    float x = 1.5f, y = 2.25f;
    printf("  1.5 + 2.25       = %f\n", (double)(x + y));
    printf("  1.5 * 2.25       = %f\n", (double)(x * y));
    printf("  2.25 - 1.5       = %f\n", (double)(y - x));
    printf("  2.25 / 1.5       = %f\n", (double)(y / x));

    /* An accumulation, so a rounding error in either direction shows up. */
    float sum = 0.0f;
    for (int i = 1; i <= 10; i++)
        sum += 1.0f / (float)i;
    printf("  sum 1/1..1/10    = %f\n", (double)sum);

    float big = 1144.22f;
    printf("  1144.22          = %f\n", (double)big);
    printf("  precision 2      = %.2f\n", (double)big);
    printf("  precision 0      = %.0f\n", (double)big);

    printf("  negative         = %f\n", (double)(-x));
    printf("  integer-valued   = %f\n", (double)(float)42);

    int n = (int)(a / b * 100.0f);
    printf("  (int)(pi * 100)  = %d\n", n);

    return 0;
}
