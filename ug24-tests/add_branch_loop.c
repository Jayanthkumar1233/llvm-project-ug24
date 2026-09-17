//===-- add_branch_loop.c - Addition with branches and loops on the uG24 --===//
//
// Adds two numbers three different ways and cross-checks the answers, so the
// program exercises the three things the backend has to get right together:
//
//   addition   ADD on the low byte, ADC on the high byte (no 16-bit ALU)
//   branches   CMP followed by BEQ / BNE / BLT / BGE, high byte then low
//   loops      a back edge that survives register allocation
//
// The two operands are volatile, so nothing here is folded at compile time --
// the adds, the compares and the loops all survive to the object file.
//
//   Build and run:   ug24-tests/ug24-run.sh ug24-tests/add_branch_loop.c
//   Other levels:    OPT=-O0 ug24-tests/ug24-run.sh ug24-tests/add_branch_loop.c
//
//===----------------------------------------------------------------------===//

#include <ug24.h>

// Volatile so the compiler must load them, not constant-fold the program away.
volatile ug24_u16 operand_a = 1234;
volatile ug24_u16 operand_b = 5678;

static int failures = 0;

//===----------------------------------------------------------------------===//
// 1.  Straight addition -- one ADD and one ADC
//===----------------------------------------------------------------------===//

ug24_u16 add_direct(ug24_u16 a, ug24_u16 b) {
    return (ug24_u16)(a + b);
}

//===----------------------------------------------------------------------===//
// 2.  The same sum built by a loop -- a back edge and a counted compare
//===----------------------------------------------------------------------===//

ug24_u16 add_by_loop(ug24_u16 a, ug24_u16 b) {
    ug24_u16 total = a;
    for (ug24_u16 i = 0; i < b; i++)   // BNE on the loop counter
        total = (ug24_u16)(total + 1);
    return total;
}

//===----------------------------------------------------------------------===//
// 3.  The same sum built a byte at a time -- a while loop with a branch inside
//===----------------------------------------------------------------------===//

ug24_u16 add_by_chunks(ug24_u16 a, ug24_u16 b) {
    ug24_u16 total = a;
    ug24_u16 left  = b;

    while (left != 0) {                // BEQ to leave the loop
        if (left >= 100) {             // BLT / BGE inside the loop body
            total = (ug24_u16)(total + 100);
            left  = (ug24_u16)(left - 100);
        } else {
            total = (ug24_u16)(total + left);
            left  = 0;
        }
    }
    return total;
}

//===----------------------------------------------------------------------===//
// 4.  Signed addition selected by a branch -- add or subtract by sign
//===----------------------------------------------------------------------===//

ug24_s16 add_signed(ug24_s16 a, ug24_s16 b) {
    if (b < 0)                          // signed compare: BLT
        return (ug24_s16)(a - (ug24_s16)(-b));
    return (ug24_s16)(a + b);
}

//===----------------------------------------------------------------------===//
// 5.  A do-while that keeps adding until it crosses a limit
//===----------------------------------------------------------------------===//

ug24_u16 add_until(ug24_u16 start, ug24_u16 step, ug24_u16 limit) {
    ug24_u16 value = start;
    ug24_u16 rounds = 0;
    do {
        value = (ug24_u16)(value + step);
        rounds++;
    } while (value < limit);            // BLT on the back edge
    return rounds;
}

//===----------------------------------------------------------------------===//
// Reporting
//===----------------------------------------------------------------------===//

static void check(const char *label, ug24_u16 got, ug24_u16 want) {
    ug24_puts("  ");
    ug24_puts(label);
    ug24_puts(" = ");
    ug24_put_u16(got);
    if (got != want) {                  // a branch on the result itself
        ug24_puts("   WRONG, expected ");
        ug24_put_u16(want);
        failures++;
    }
    ug24_putchar('\n');
}

int main(void) {
    ug24_u16 a = operand_a;
    ug24_u16 b = operand_b;

    ug24_puts("adding ");
    ug24_put_u16(a);
    ug24_puts(" and ");
    ug24_put_u16(b);
    ug24_println("");

    ug24_println(" three ways to the same answer");
    ug24_u16 direct = add_direct(a, b);
    ug24_u16 looped = add_by_loop(a, b);
    ug24_u16 chunks = add_by_chunks(a, b);

    check("a + b   (one ADD/ADC)", direct, 6912);
    check("a + b   (loop of 1s)",  looped, 6912);
    check("a + b   (loop of 100s)", chunks, 6912);

    // Compare the three results against each other with real branches.
    ug24_puts("  all three agree? ");
    if (direct == looped && looped == chunks) {
        ug24_println("yes");
    } else {
        ug24_println("NO");
        failures++;
    }

    ug24_println(" branch chooses the operation");
    check("100 + 25",  (ug24_u16)add_signed(100,  25),  125);
    check("100 + -25", (ug24_u16)add_signed(100, -25),   75);

    ug24_println(" loop until a limit is crossed");
    check("steps of 7 to reach 100",  add_until(0, 7, 100),  15);
    check("steps of 250 to reach 1000", add_until(0, 250, 1000), 4);

    ug24_println(" carry out of the low byte");
    check("255 + 1",       add_direct(255, 1),       256);
    check("65535 + 1",     add_direct(65535, 1),       0);

    ug24_putchar('\n');
    if (failures == 0)
        ug24_println("PASS - 9 checks, all correct");
    else {
        ug24_puts("FAIL - wrong answers: ");
        ug24_put_u16((ug24_u16)failures);
        ug24_putchar('\n');
    }
    return failures;
}
