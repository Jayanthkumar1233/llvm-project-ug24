/* 02_integers - arithmetic whose answers you can check by hand.
 *
 * Every line below is checkable without trusting any toolchain: 25 + 17 is
 * 42, the sum of 1 to 100 is 5050, 48879 is 0xBEEF.  The point is to exercise
 * the ALU, the flags, and the multiply, divide and shift helpers -- the uG24
 * has no 16- or 32-bit arithmetic, so each of these is a library call.
 *
 * int is 16 bits here and long is 32; see the README.
 */
#include <stdio.h>

int main(void)
{
    printf("uG24 sample 2: integers\n");

    printf("  25 + 17          = %d\n", 25 + 17);
    printf("  100 - 142        = %d\n", 100 - 142);
    printf("  12 * 11          = %d\n", 12 * 11);
    printf("  1000 / 7         = %d remainder %d\n", 1000 / 7, 1000 % 7);
    printf("  -1000 / 7        = %d remainder %d\n", -1000 / 7, -1000 % 7);

    unsigned sum = 0;
    for (unsigned i = 1; i <= 100; i++)
        sum += i;
    printf("  sum 1..100       = %u\n", sum);

    printf("  48879 in hex     = %04X and %04x\n", 48879u, 48879u);
    printf("  1 << 15          = %u\n", 1u << 15);
    printf("  0xBEEF >> 4      = %u\n", 0xBEEFu >> 4);

    long big = 1000000L;
    printf("  1000000 * 3      = %ld\n", big * 3);
    printf("  1000000 / 7      = %ld remainder %ld\n", big / 7, big % 7);

    long long huge = 1000000000LL;
    printf("  1e9 * 9          = %lld\n", huge * 9);

    return 0;
}
