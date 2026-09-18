//===-- t11_vla.c - variable-length arrays and alloca --------------------===//
//
// These move SP after the prologue has run, which is why the target needs a
// frame pointer: every frame offset computed against SP becomes wrong the
// moment a VLA is allocated.  Before P3 became the frame pointer, a function
// holding both a VLA and a fixed-size local printed the right answers and
// then returned into its own stack.
//
// The cases below are the ones that broke it: a VLA beside a fixed array, a
// VLA nested inside another, a VLA whose address is handed to a call, and
// alloca, which has the same shape.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <string.h>

static int fixed_and_vla(void)
{
    int fixed[10];
    for (int i = 0; i < 10; i++)
        fixed[i] = i;

    int n = 4;
    int vla[n];
    for (int i = 0; i < n; i++)
        vla[i] = i + 1;

    return fixed[9] * 100 + vla[0] * 10 + vla[3];
}

static int sum_vla(int n)
{
    int a[n];
    for (int i = 0; i < n; i++)
        a[i] = i + 1;
    int s = 0;
    for (int i = 0; i < n; i++)
        s += a[i];
    return s;
}

static int nested(int n)
{
    int outer[n];
    outer[0] = n;
    {
        int inner[n * 2];
        for (int i = 0; i < n * 2; i++)
            inner[i] = i;
        outer[0] += inner[n];
    }
    return outer[0];
}

/* The VLA's address crosses a call, so SP moves again for the argument. */
static int with_call(int n)
{
    char buf[n];
    memset(buf, 'A', n);
    buf[n - 1] = '\0';
    return (int)strlen(buf);
}

static int alloca_test(int n)
{
    char *p = __builtin_alloca(n);
    for (int i = 0; i < n; i++)
        p[i] = (char)i;
    return p[n - 1];
}

/* Recursion on top of a VLA: each frame allocates its own. */
static int recursive_vla(int n)
{
    if (n == 0)
        return 0;
    int a[n];
    a[n - 1] = n;
    return a[n - 1] + recursive_vla(n - 1);
}

static int failures = 0;

static void check(const char *label, int got, int want)
{
    printf("  %-24s = %d", label, got);
    if (got != want) {
        printf("   WRONG, expected %d", want);
        failures++;
    }
    printf("\n");
}

int main(void)
{
    check("fixed array beside vla", fixed_and_vla(), 914);
    check("sum of vla 1..10",       sum_vla(10),      55);
    check("sum of vla, n = 1",      sum_vla(1),        1);
    check("vla nested in a vla",    nested(4),         8);
    check("vla passed to a call",   with_call(8),      7);
    check("alloca",                 alloca_test(6),    5);
    check("recursion with a vla",   recursive_vla(6), 21);

    printf("\n");
    if (failures == 0)
        printf("PASS - 7 checks, all correct\n");
    else
        printf("FAIL - wrong answers: %d\n", failures);
    return failures;
}
