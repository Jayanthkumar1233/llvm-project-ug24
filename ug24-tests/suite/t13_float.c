/* Single-precision soft floating point.
 *
 * The uG24 has no FPU, so every operation here is a call into
 * ug24-runtime/ug24_float.c.  `double` is IEEE single on this target, which
 * is why the host build below uses `float` throughout: the two have to be
 * comparing the same arithmetic.
 *
 * Results are printed as bit patterns rather than as decimals wherever the
 * point is the arithmetic.  That makes the comparison against the host exact
 * and independent of either side's decimal conversion.
 */
#include <stdio.h>

static void show(const char *what, float value) {
    union { float f; unsigned long bits; } u;
    u.f = value;
    printf("%-9s %08lx\n", what, (unsigned long)(u.bits & 0xffffffffUL));
}

int main(void) {
    volatile float a = 3.5f, b = 0.125f, c = -2.0f, zero = 0.0f;
    volatile float big = 16777216.0f;     /* 2^24: the last exact integer */
    volatile long i = -1234567;

    /* Exactly representable operands and results: no rounding is involved,
     * so the answer is the same on any conforming implementation. */
    show("add", a + b);
    show("sub", a - b);
    show("mul", a * b);
    show("div", a / b);
    show("neg", -a);
    show("addneg", a + c);
    show("mulneg", a * c);
    show("zero", a * zero);
    show("big+1", big + 1.0f);            /* rounds back to 2^24 */
    show("big*2", big * 2.0f);
    show("tiny", 1.0f / 1024.0f);

    /* Inexact, but the correctly rounded result is the same everywhere. */
    show("third", 1.0f / 3.0f);
    show("tenth", 1.0f / 10.0f);
    show("sqrt2ish", 1.41421356f * 1.41421356f);

    /* Conversions. */
    show("fromint", (float)i);
    show("fromuint", (float)(unsigned long)4000000000UL);
    show("fromll", (float)(long long)-1234567890123LL);
    printf("toint %ld %ld %ld\n", (long)(a + b), (long)c, (long)(-a));
    printf("touint %lu\n", (unsigned long)big);

    /* Comparison, including the special cases. */
    printf("cmp %d %d %d %d %d %d\n",
           a > b, a < b, a == a, c < zero, zero == -zero, a >= a);

    /* Infinity and NaN, reached by arithmetic rather than by a literal. */
    {
        volatile float huge = 3.0e38f;
        float inf = huge * 10.0f;
        float nan = inf - inf;
        show("inf", inf);
        printf("nan-cmp %d %d\n", nan == nan, nan != nan);
    }

    /* Decimal conversion.  Values chosen to terminate in binary, so the
     * digits are not a question of how either side rounds. */
    printf("f %.3f %.3f %.3f %.1f\n", 3.25f, -0.5f, 100.0f, 0.0f);
    printf("g %g %g\n", 1.5f, 0.125f);
    printf("e %.3e %.3e\n", 1.5f, -8192.0f);
    printf("w |%10.2f|%-10.2f|\n", 1.25f, 1.25f);

    /* Digits, against a hosted printf.
     *
     * Two separate things are being checked here.  The values are printed at
     * several precisions and diffed against glibc, which catches a formatter
     * that is merely close -- the fraction is generated from the mantissa by
     * exact integer arithmetic, and an earlier version that multiplied the
     * float by ten repeatedly printed 1144.22f as 1144.219970 where the value
     * rounds to 1144.219971.
     *
     * And the whole first expression folds to a constant at compile time, so
     * the program contains no floating-point arithmetic at all.  That is the
     * case where nothing drags the float runtime out of libug24.a, and where
     * %f used to print "<fp?>".  The compiler now asks for the formatter
     * whenever a float reaches a variadic call. */
    {
        float folded = 879 * 9 / 50.0f + 52 + 934 * 8 / 8.0f;
        volatile float values[8];
        int i;

        printf("folded %f\n", (double)folded);

        values[0] = 0.1f;        values[1] = 1.0f / 3.0f;
        values[2] = 1144.22f;    values[3] = 123456.789f;
        values[4] = 0.999999f;   values[5] = 1e9f;
        values[6] = 0.0625f;     values[7] = -1144.22f;

        for (i = 0; i < 8; i++)
            printf("d %.6f %.0f %.1f %.9f %.2f\n",
                   (double)values[i], (double)values[i], (double)values[i],
                   (double)values[i], (double)values[i]);
    }

    printf("PASS - soft float\n");
    return 0;
}
