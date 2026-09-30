/* 03_control - branches, calls, recursion and indirect calls.
 *
 * Exercises what the encodings alone cannot show: that relative branches go
 * where they should in both directions, that RA is saved across a nested
 * call, that the stack unwinds correctly from recursion, and that a call
 * through a function pointer lands in the right place.
 *
 * Every answer is a textbook value: 8! is 40320, the 15th Fibonacci number
 * is 610, gcd(1071, 462) is 21.
 */
#include <stdio.h>

static unsigned factorial(unsigned n)
{
    return n < 2 ? 1 : n * factorial(n - 1);
}

static unsigned fib(unsigned n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static unsigned gcd(unsigned a, unsigned b)
{
    while (b) { unsigned t = a % b; a = b; b = t; }
    return a;
}

static unsigned twice(unsigned x)  { return x * 2; }
static unsigned square(unsigned x) { return x * x; }

int main(void)
{
    printf("uG24 sample 3: control flow\n");

    printf("  8!               = %u\n", factorial(8));
    printf("  fib(15)          = %u\n", fib(15));
    printf("  gcd(1071, 462)   = %u\n", gcd(1071, 462));

    /* A loop that counts both ways, so a branch whose displacement has the
     * wrong sign cannot pass. */
    unsigned up = 0, down = 0;
    for (unsigned i = 0; i < 10; i++) up += i;
    for (unsigned i = 10; i-- > 0; )  down += i;
    printf("  up=%u down=%u     (equal)\n", up, down);

    /* switch, which the compiler may turn into a chain of compares. */
    for (int k = 0; k < 4; k++) {
        const char *name;
        switch (k) {
        case 0:  name = "zero";  break;
        case 1:  name = "one";   break;
        case 2:  name = "two";   break;
        default: name = "many";  break;
        }
        printf("  switch(%d)        = %s\n", k, name);
    }

    unsigned (*fn[2])(unsigned) = { twice, square };
    printf("  fn[0](21)        = %u\n", fn[0](21));
    printf("  fn[1](7)         = %u\n", fn[1](7));

    return 0;
}
