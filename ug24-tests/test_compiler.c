// End-to-end check of the uG24 backend.  Each case leaves a byte in results[]
// that is 1 when the case passed; main returns the number of failures.
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;

volatile u8 results[48];
u8 ncases;

static void check(u8 ok) { results[ncases++] = ok; }

// Keep the optimiser from folding the whole test away.
static volatile u8  v8;
static volatile s8  vs8;
static volatile u16 v16;
static volatile s16 vs16;

static u16 fact(u8 n) { return n < 2 ? 1 : (u16)(n * fact(n - 1)); }
static u8 gcd(u8 a, u8 b) { while (b) { u8 t = a % b; a = b; b = t; } return a; }

struct point { u16 x; u8 y; };
static u16 point_sum(struct point p) { return (u16)(p.x + p.y); }

// More arguments than there are registers, so some arrive on the stack.
static u8 __attribute__((noinline))
many(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f) { return (u8)(a+b+c+d+e+f); }

static u16 __attribute__((noinline))
mixed(u16 a, u16 b, u16 c, u8 d) { return (u16)(a + b + c + d); }

// A deep chain: each level has to preserve its own return address.
static u8 __attribute__((noinline)) lvl3(u8 x) { return (u8)(x + 3); }
static u8 __attribute__((noinline)) lvl2(u8 x) { return (u8)(lvl3(x) * 2); }
static u8 __attribute__((noinline)) lvl1(u8 x) { return (u8)(lvl2(x) + 1); }

// Several values live across a call, forcing use of the callee-saved set.
static u8 __attribute__((noinline)) bump(u8 x) { return (u8)(x + 1); }
static u8 __attribute__((noinline)) live_across(u8 a, u8 b, u8 c) {
    u8 x = (u8)(a * 2), y = (u8)(b + 7), z = (u8)(c ^ 0x0f);
    u8 r = bump(a);
    return (u8)(x + y + z + r);
}

// Initialised data must survive the .data placement and .bss clear.
u8  g8  = 0xA5;
u16 g16 = 0xBEEF;
u8  garr[4] = {1, 2, 3, 4};
u8  gbss[4];

int main(void) {
    // 8-bit arithmetic and logic
    v8 = 200; check((u8)(v8 + 100) == 44);       // wraps
    v8 = 10;  check((u8)(v8 - 20) == 246);
    v8 = 0xF0; check((v8 & 0x3C) == 0x30);
    v8 = 0xF0; check((v8 | 0x0F) == 0xFF);
    v8 = 0xAA; check((v8 ^ 0xFF) == 0x55);
    v8 = 0x5A; check((u8)~v8 == 0xA5);

    // shifts
    v8 = 1;    check((u8)(v8 << 7) == 0x80);
    v8 = 0x80; check((u8)(v8 >> 7) == 1);
    vs8 = -8;  check((s8)(vs8 >> 2) == -2);

    // unsigned and signed comparison
    v8 = 200;  check(v8 > 100);
    vs8 = -1;  check(vs8 < 0);
    vs8 = -1;  check((u8)vs8 > 100);

    // 16-bit arithmetic
    v16 = 30000; check((u16)(v16 + 40000) == (u16)70000);
    v16 = 100;   check((u16)(v16 - 200) == (u16)-100);
    v16 = 0x1234; check((v16 >> 8) == 0x12);
    v16 = 0x1234; check((u16)(v16 << 4) == 0x2340);

    // 16-bit comparison, signed and unsigned
    vs16 = -1000; check(vs16 < 0);
    vs16 = -1000; check(vs16 < 5);
    v16 = 40000;  check(v16 > 30000);
    v16 = 0x0100; check(v16 > 0x00FF);
    v16 = 0x00FF; check(v16 < 0x0100);

    // multiply, divide, modulo via the runtime helpers
    v16 = 123; check((u16)(v16 * 3) == 369);
    v16 = 1000; check(v16 / 7 == 142);
    v16 = 1000; check(v16 % 7 == 6);
    vs16 = -100; check(vs16 / 7 == -14);
    vs16 = -100; check(vs16 % 7 == -2);

    // control flow and recursion
    check(fact(5) == 120);
    v8 = 48; check(gcd(v8, 18) == 6);

    // loops and arrays
    {
        u8 a[8], i, sum = 0;
        for (i = 0; i < 8; i++) a[i] = (u8)(i * i);
        for (i = 0; i < 8; i++) sum = (u8)(sum + a[i]);
        check(sum == 140);
    }

    // pointers
    {
        u8 x = 7, *p = &x;
        *p = (u8)(*p + 1);
        check(x == 8);
    }

    // structs passed by value
    {
        struct point p; p.x = 1000; p.y = 24;
        check(point_sum(p) == 1024);
    }

    // switch statement
    {
        u8 r = 0;
        v8 = 3;
        switch (v8) { case 1: r = 10; break; case 3: r = 30; break; default: r = 99; }
        check(r == 30);
    }

    // calls: stack arguments, deep chains, callee-saved registers
    check(many(1, 2, 3, 4, 5, 6) == 21);
    check(mixed(1000, 2000, 3000, 5) == 6005);
    check(lvl1(10) == 27);
    check(live_across(3, 4, 5) == 6 + 11 + 10 + 4);

    // static initialisation
    check(g8 == 0xA5);
    check(g16 == 0xBEEF);
    check(garr[0] == 1 && garr[3] == 4);
    check(gbss[0] == 0 && gbss[3] == 0);

    {
        u8 i, failures = 0;
        for (i = 0; i < ncases; i++) if (!results[i]) failures++;
        return failures;
    }
}
