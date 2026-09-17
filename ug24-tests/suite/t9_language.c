//===-- t9_language.c - Every C construct the uG24 toolchain supports -----===//
//
// One program that uses each kind of statement, declaration and operator in
// the C language, compiled by the uG24 cross compiler and executed on the
// simulator.  It is written so the output is identical on a normal host
// compiler, so the two can be diffed against each other -- values are kept
// inside 16 bits, because int is 16 bits on this target and 32 on a PC.
//
// NOT INCLUDED, because they do not link on this target today:
//
//   float / double     no FPU and no soft-float library -> __addsf3 undefined
//   long long * and /  no 64-bit helpers -> __muldi3 / __divdi3 undefined
//                      (shifts, add and compare on long long do work)
//   setjmp / longjmp   no setjmp.h
//
//   Build and run:  ug24-tests/ug24-run.sh ug24-tests/suite/t9_language.c
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

//===----------------------------------------------------------------------===//
// Preprocessor: object-like, function-like, stringify, paste, conditionals
//===----------------------------------------------------------------------===//

#define LIMIT        10
#define SQUARE(x)    ((x) * (x))
#define STRINGIFY(x) #x
#define GLUE(a, b)   a##b
#define VERSION      2

#if VERSION >= 2
#define BUILD "v2"
#else
#define BUILD "v1"
#endif

#ifdef LIMIT
#define HAVE_LIMIT 1
#endif

int GLUE(pasted, _name)(void) { return 77; }

//===----------------------------------------------------------------------===//
// Declarations: typedef, enum, struct, union, bitfields, const, volatile
//===----------------------------------------------------------------------===//

typedef unsigned char  byte;
typedef unsigned short word;
typedef int (*unary_fn)(int);

enum Colour { RED, GREEN = 5, BLUE };

struct Point { int x, y; };

struct Packed {
    byte  ready : 1;
    byte  mode  : 3;
    byte  spare : 4;
};

union Punned {
    word  whole;
    byte  half[2];
};

static const char *const NAMES[] = { "red", "green", "blue" };

_Static_assert(sizeof(char) == 1, "a char is one byte everywhere");
_Static_assert(GREEN == 5, "enumerator with an explicit value");

//===----------------------------------------------------------------------===//
// Functions: recursion, mutual recursion, variadic, struct by value, inline
//===----------------------------------------------------------------------===//

static int factorial(int n) { return n <= 1 ? 1 : n * factorial(n - 1); }

static int is_odd(int n);
static int is_even(int n) { return n == 0 ? 1 : is_odd(n - 1); }
static int is_odd(int n)  { return n == 0 ? 0 : is_even(n - 1); }

static inline int triple(int x) { return x * 3; }

static int sum_all(int count, ...)
{
    va_list ap;
    int total = 0;

    va_start(ap, count);
    for (int i = 0; i < count; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

static int point_sum(struct Point p) { return p.x + p.y; }

static struct Point point_make(int x, int y)
{
    struct Point p = { x, y };
    return p;
}

static int doubler(int x) { return x * 2; }
static int negate(int x)  { return -x; }
static int apply(unary_fn fn, int v) { return fn(v); }

//===----------------------------------------------------------------------===//

int main(void)
{
    /* --- storage classes and qualifiers ------------------------------- */
    auto int automatic = 1;
    static int persistent = 2;
    register int fast = 3;
    const int constant = 4;
    volatile int observed = 5;

    printf("storage: %d %d %d %d %d\n",
           automatic, persistent, fast, constant, observed);

    /* --- the null statement, and a compound statement ----------------- */
    ;
    {
        int shadowed = 100;
        printf("block: %d\n", shadowed);
    }

    /* --- if / else if / else ------------------------------------------ */
    int grade = 75;
    if (grade >= 90)        printf("if: A\n");
    else if (grade >= 70)   printf("if: B\n");
    else                    printf("if: C\n");

    /* --- switch, with a deliberate fallthrough and a default ---------- */
    for (int k = 0; k < 4; k++) {
        printf("switch %d: ", k);
        switch (k) {
        case 0:
            printf("zero ");
            /* falls through */
        case 1:
            printf("low\n");
            break;
        case 2:
            printf("two\n");
            break;
        default:
            printf("other\n");
            break;
        }
    }

    /* --- while, do-while, for (three shapes), break, continue --------- */
    int n = 0, guard = 0;
    while (n < LIMIT) n++;
    do { guard++; } while (guard < 3);

    int loop_total = 0;
    for (int i = 0, j = LIMIT; i < j; i++, j--)   /* comma operator */
        loop_total += i;

    int skipped = 0;
    for (int i = 0; i < LIMIT; i++) {
        if (i % 2 == 0) continue;
        if (i > 7) break;
        skipped += i;
    }

    int endless = 0;
    for (;;) { if (++endless == 4) break; }       /* empty for */

    printf("loops: %d %d %d %d %d\n",
           n, guard, loop_total, skipped, endless);

    /* --- goto and a label --------------------------------------------- */
    int attempts = 0;
retry:
    attempts++;
    if (attempts < 3)
        goto retry;
    printf("goto: %d\n", attempts);

    /* --- operators ---------------------------------------------------- */
    int a = 12, b = 5;
    printf("arith: %d %d %d %d %d\n", a + b, a - b, a * b, a / b, a % b);
    /* The complement is printed as a word: (int)(word)~x would be 65523 where
       int is 32 bits and -13 where it is 16, and this file has to read the
       same on both. */
    printf("bitwise: %d %d %d %u %d %d\n",
           a & b, a | b, a ^ b, (unsigned)(word)~(word)a, a << 2, a >> 2);
    printf("logical: %d %d %d\n", a && b, a || 0, !a);
    printf("compare: %d %d %d %d %d %d\n",
           a > b, a < b, a >= b, a <= b, a == b, a != b);

    /* Each of these is its own statement on purpose.  Putting c++, ++c and
       --c in one argument list is unsequenced modification -- undefined
       behaviour -- and the two compilers would be free to disagree. */
    int c = 10;
    int post_inc = c++;
    int pre_inc  = ++c;
    int pre_dec  = --c;
    int post_dec = c--;
    printf("incdec: %d %d %d %d %d\n", post_inc, pre_inc, pre_dec, post_dec, c);

    int assign = 8;
    assign += 2; assign -= 1; assign *= 3; assign /= 2;
    assign %= 10; assign <<= 2; assign >>= 1; assign |= 1;
    assign &= 0xF; assign ^= 2;
    printf("compound: %d\n", assign);

    printf("ternary: %d\n", a > b ? a : b);
    int comma_result = (assign = b, a + b);   /* left operand has an effect */
    printf("comma: %d\n", comma_result);

    /* --- arrays, 2-D arrays, pointers, pointer arithmetic ------------- */
    int values[LIMIT];
    for (int i = 0; i < LIMIT; i++)
        values[i] = SQUARE(i);

    int grid[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            grid[i][j] = i * 3 + j;

    int *p = values;
    int *q = &values[4];
    int **pp = &p;

    printf("array: %d %d %d\n", values[3], grid[1][2], grid[2][2]);
    printf("pointer: %d %d %d %d\n", *p, *(p + 4), (int)(q - p), **pp);

    /* --- variable-length arrays: NOT exercised here --------------------
     *
     * A VLA on its own works, but a function containing both a VLA and a
     * fixed-size local returns into the stack -- the epilogue does not undo
     * the dynamic SP adjustment, because the frame lowering has no frame
     * pointer.  Reproducer:
     *
     *   int main(void){ int f[10]; for(int i=0;i<10;i++) f[i]=i;
     *                   int n=4; int a[n]; ...  }
     *   -> prints the right values, then "unimplemented instruction" at 0xfe28
     *
     * alloca() has the same shape and the same problem.  Left out until the
     * frame lowering grows a frame pointer.
     */

    /* --- struct, union, bitfield, enum, designated init, compound lit - */
    struct Point origin = { 0, 0 };
    struct Point corner = { .y = 7, .x = 3 };          /* designated */
    struct Point temp   = (struct Point){ 9, 1 };      /* compound literal */
    struct Point *sp    = &corner;

    union Punned u;
    u.whole = 0xBEEF;

    struct Packed flags = { .ready = 1, .mode = 5, .spare = 0 };

    enum Colour colour = BLUE;

    printf("struct: %d %d %d %d\n", origin.x, corner.x, sp->y, temp.x + temp.y);
    printf("union: %04X %02X %02X\n", u.whole, u.half[1], u.half[0]);
    printf("bitfield: %d %d\n", flags.ready, flags.mode);
    /* BLUE is 6, not 2, because GREEN was given an explicit value -- so the
       name lookup has to be mapped, not indexed directly. */
    int name_index = (colour == BLUE) ? 2 : (colour == GREEN) ? 1 : 0;
    printf("enum: %d %d %s\n", RED, colour, NAMES[name_index]);

    /* --- functions ----------------------------------------------------- */
    printf("call: %d %d %d\n", factorial(7), triple(5), pasted_name());
    printf("mutual: %d %d\n", is_even(8), is_odd(8));
    printf("varargs: %d %d\n", sum_all(3, 10, 20, 30), sum_all(1, 5));
    printf("struct arg: %d %d\n", point_sum(corner), point_sum(point_make(2, 3)));
    printf("fnptr: %d %d\n", apply(doubler, 21), apply(negate, 21));

    /* --- library ------------------------------------------------------- */
    char buffer[24];
    strcpy(buffer, "uG24");
    strcat(buffer, "-ok");
    snprintf(buffer + strlen(buffer), sizeof buffer - strlen(buffer), "-%d", 9);

    char *heap = malloc(8);
    memset(heap, 'z', 7);
    heap[7] = '\0';

    printf("string: %s %u\n", buffer, (unsigned)strlen(buffer));
    printf("heap: %s\n", heap);
    free(heap);

    /* --- preprocessor results ------------------------------------------ */
    printf("macro: %d %s %s %d\n",
           SQUARE(6), STRINGIFY(uG24), BUILD, HAVE_LIMIT);

    return 0;
}
