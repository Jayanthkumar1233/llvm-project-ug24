/* 64-bit arithmetic.
 *
 * `long long` is 64 bits here and none of it is native: addition and
 * comparison are expanded onto four 16-bit halves, and multiply, divide and
 * the variable shifts are calls into ug24-runtime/ug24_int64.c.  Every value
 * below carries across at least one limb boundary, which is where a wrong
 * carry shows up.
 *
 * The results are printed rather than compared against constants, and the
 * .expected file comes from building this same source with the host
 * compiler.  That makes the test a comparison against a normal C
 * implementation rather than against my arithmetic.
 */
#include <stdio.h>

/* printf has no 64-bit conversion, so every value goes out as two halves.
 * The masks matter: `unsigned long` is 32 bits on the uG24 and 64 on the
 * host, and without them the two builds would print different widths. */
static void show(const char *what, unsigned long long value) {
    printf("%-10s %08lx%08lx\n", what,
           (unsigned long)((value >> 32) & 0xffffffffULL),
           (unsigned long)(value & 0xffffffffULL));
}

int main(void) {
    unsigned long long a = 0x0123456789abcdefULL;
    unsigned long long b = 0xfedcba9876543210ULL;
    long long s = -1234567890123LL;
    volatile int shift = 33;   /* volatile: a variable shift, not a constant */

    show("add", a + b);
    show("sub", b - a);
    show("neg", (unsigned long long)(0 - (long long)a));

    show("shl4", a << 4);
    show("shl32", a << 32);
    show("shl-var", a << shift);
    show("shr4", a >> 4);
    show("shr32", a >> 32);
    show("shr-var", a >> shift);
    show("sar8", (unsigned long long)(s >> 8));
    show("sar-var", (unsigned long long)(s >> shift));

    show("mul", 1000000ULL * 1000000ULL);
    show("mul-lim", 0x0000000100000001ULL * 0x0000000100000001ULL);
    show("mul-carry", 0xffffffffULL * 0xffffffffULL);
    show("mul-full", a * b);

    show("udiv", 0xfffffffffffffffeULL / 3ULL);
    show("umod", 0xfffffffffffffffeULL % 7ULL);
    show("udiv-big", b / 0x0000000123456789ULL);
    show("sdiv", (unsigned long long)(-1000000000000LL / 7LL));
    show("smod", (unsigned long long)(-1000000000000LL % 7LL));
    show("sdiv-nn", (unsigned long long)(-1000000000000LL / -7LL));

    /* Comparison across a limb boundary, in both signednesses. */
    printf("cmp %d %d %d %d\n",
           0x0000000100000000ULL > 0x00000000ffffffffULL,
           -1LL < 1LL, -2LL < -1LL, (long long)a > (long long)b);

#ifdef __ug24__
    /* Division by zero yields zero here rather than trapping: there is
     * nothing on this part to trap to.  Guarded because the host takes
     * SIGFPE, and the host is where .expected comes from.  Printing the
     * same line either way keeps the two outputs identical. */
    {
        volatile unsigned long long zero = 0;
        printf("divzero %d\n", (int)(a / zero) == 0);
    }
#else
    printf("divzero 1\n");
#endif

    printf("PASS - 64-bit arithmetic\n");
    return 0;
}
