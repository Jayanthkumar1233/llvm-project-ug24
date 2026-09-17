//===-- add.c - Addition test for the uG24 cross compiler -----------------===//
//
// Exercises the paths the backend has to get right for addition on a machine
// with no 16-bit ALU:
//
//   * 8-bit  add          -> a single ADD
//   * 16-bit add          -> ADD on the low byte, ADC on the high byte
//   * add of a constant   -> ADI, or MVI + ADD when the constant is wide
//   * carry out of bit 7  -> the case that fails if ADC is missing or if the
//                            expansion pass gets the byte order wrong
//   * signed negatives    -> two's complement across the pair
//   * addition in a loop  -> the accumulator must survive register allocation
//
// Every result is printed AND checked, so a wrong answer changes the exit
// status as well as the console output.
//
//   Build and run:  ug24-tests/ug24-run.sh ug24-tests/add.c
//
//===----------------------------------------------------------------------===//

#include <ug24.h>

static int failures = 0;

/// Print "label = value" and record a failure if it is not what we expect.
static void check_u16(const char *label, ug24_u16 got, ug24_u16 want) {
    ug24_puts("  ");
    ug24_puts(label);
    ug24_puts(" = ");
    ug24_put_u16(got);
    if (got != want) {
        ug24_puts("   WRONG, expected ");
        ug24_put_u16(want);
        failures++;
    }
    ug24_putchar('\n');
}

static void check_s16(const char *label, ug24_s16 got, ug24_s16 want) {
    ug24_puts("  ");
    ug24_puts(label);
    ug24_puts(" = ");
    ug24_put_s16(got);
    if (got != want) {
        ug24_puts("   WRONG, expected ");
        ug24_put_s16(want);
        failures++;
    }
    ug24_putchar('\n');
}

//===----------------------------------------------------------------------===//
// The functions under test.  They are deliberately not static and not inline:
// keeping them as real calls forces the compiler to pass the operands through
// the calling convention instead of folding the whole thing at compile time.
//===----------------------------------------------------------------------===//

ug24_u8  add_u8 (ug24_u8  a, ug24_u8  b) { return (ug24_u8)(a + b); }
ug24_u16 add_u16(ug24_u16 a, ug24_u16 b) { return (ug24_u16)(a + b); }
ug24_s16 add_s16(ug24_s16 a, ug24_s16 b) { return (ug24_s16)(a + b); }
ug24_u16 add_const(ug24_u16 a)           { return (ug24_u16)(a + 4660); }
ug24_u16 add_small(ug24_u16 a)           { return (ug24_u16)(a + 7); }

// `total` is volatile on purpose.  Without it the optimiser recognises the
// triangular sum and replaces the whole loop with n*(n+1)/2 — a 32-bit
// multiply, which calls __mulsi3, which libug24.a does not provide.  Marking
// it volatile keeps the loop as a loop, which is what this test is for.
ug24_u16 sum_to(ug24_u16 n) {
    volatile ug24_u16 total = 0;
    for (ug24_u16 i = 1; i <= n; i++)
        total = (ug24_u16)(total + i);
    return total;
}

int main(void) {
    ug24_println("uG24 addition test");

    ug24_println(" 8-bit");
    check_u16("2 + 3",            add_u8(2, 3),        5);
    check_u16("200 + 55",         add_u8(200, 55),     255);   // fills the byte
    check_u16("200 + 56",         add_u8(200, 56),     0);     // wraps to zero

    ug24_println(" 16-bit, carry between the bytes");
    check_u16("255 + 1",          add_u16(255, 1),     256);   // needs ADC
    check_u16("1000 + 2000",      add_u16(1000, 2000), 3000);
    check_u16("65535 + 1",        add_u16(65535, 1),   0);     // wraps
    check_u16("32768 + 32767",    add_u16(32768, 32767), 65535);

    ug24_println(" constants");
    check_u16("100 + 7",          add_small(100),      107);   // fits ADI
    check_u16("100 + 4660",       add_const(100),      4760);  // needs MVI+ADD

    ug24_println(" signed");
    check_s16("-5 + 3",           add_s16(-5, 3),      -2);
    check_s16("-1000 + 1000",     add_s16(-1000, 1000), 0);
    check_s16("-30000 + -2000",   add_s16(-30000, -2000), -32000);

    ug24_println(" accumulated in a loop");
    check_u16("sum 1..10",        sum_to(10),          55);
    check_u16("sum 1..100",       sum_to(100),         5050);
    check_u16("sum 1..300",       sum_to(300),         45150);

    ug24_putchar('\n');
    if (failures == 0) {
        ug24_println("PASS - all 15 additions correct");
    } else {
        ug24_puts("FAIL - wrong answers: ");
        ug24_put_u16((ug24_u16)failures);
        ug24_putchar('\n');
    }
    return failures;
}
