#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

static void test(const char *name, ...)
{
    va_list ap;
    printf("test: %s\n", name);
    va_start(ap, name);
    printf("  %d\n", va_arg(ap, int));
    printf("  %u\n", va_arg(ap, unsigned));
    printf("  %lx\n", va_arg(ap, unsigned long));
    printf("  %s\n", va_arg(ap, char *));
    printf("  %c\n", va_arg(ap, int));
    va_end(ap);
}

int main(void)
{
    int x = -42; unsigned u = 42; unsigned long big = 0xDEADBEEFUL;
    printf("signed=%d\n", x);
    printf("unsigned=%u\n", u);
    printf("hex=%x HEX=%X oct=%o\n", u, u, u);
    printf("long=%08lX\n", big);
    printf("[%10d]\n", 42);
    printf("[%-10d]\n", 42);
    printf("[%010d]\n", 42);
    printf("[%+d]\n", 42);
    printf("[% d]\n", 42);
    printf("[%#x]\n", 0x2A);
    printf("[%.3s]\n", "abcdef");
    printf("[%*d]\n", 8, 123);
    printf("[%.*s]\n", 3, "abcdef");
    printf("char=%c\n", 'A');
    printf("string=%s\n", "uG24");
    printf("percent=%%\n");
    test("variadic", -11, 222u, 3333UL, "hello", 'Z');
    return 0;
}
