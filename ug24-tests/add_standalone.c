//===-- add_standalone.c - Addition, branches and loops in plain C --------===//
//
// The same program as add_branch_loop.c, but with nothing outside this file:
// no #include, no ug24.h, no helper library.  Everything it needs -- the UART,
// the string output, the decimal conversion -- is written here in C.
//
// It also avoids '/' and '%' on purpose.  Division on an 8-bit machine with no
// 16-bit ALU becomes a call to __udivhi3 in libug24.a; doing the decimal
// conversion by repeated subtraction keeps the program free of every library
// call, so the only thing linked besides this file is the reset stub.
//
//   Build and run:   ug24-tests/ug24-run.sh ug24-tests/add_standalone.c
//   Other levels:    OPT=-O0 ug24-tests/ug24-run.sh ug24-tests/add_standalone.c
//   Freestanding:    clang --target=ug24-unknown-none-eabi -Os \
//                          add_standalone.c -o add_standalone.elf
//
//===----------------------------------------------------------------------===//

typedef unsigned char  u8;
typedef unsigned short u16;
typedef signed short   s16;

//===----------------------------------------------------------------------===//
// The machine.  These three addresses are the whole hardware interface: they
// live in the MMIO window that ug24.ld reserves at 0xFF00 and that the
// simulator implements.
//===----------------------------------------------------------------------===//

#define UART_TX     (*(volatile u8 *)0xFF00)
#define UART_STATUS (*(volatile u8 *)0xFF01)
#define UART_READY  0x01

static void put_char(char c) {
    while ((UART_STATUS & UART_READY) == 0)   // poll: a loop with a branch
        ;
    UART_TX = (u8)c;
}

static void put_str(const char *s) {
    while (*s != '\0')                        // loop over the string
        put_char(*s++);
}

static void put_line(const char *s) {
    put_str(s);
    put_char('\n');
}

//===----------------------------------------------------------------------===//
// Decimal output without '/' or '%'.
//
// Each digit is found by subtracting a power of ten while it still fits -- a
// loop and a compare per digit.  Slower than a divide, but it keeps the whole
// program inside this file.
//===----------------------------------------------------------------------===//

static void put_u16(u16 value) {
    static const u16 powers[5] = { 10000, 1000, 100, 10, 1 };
    u8 started = 0;

    for (u8 p = 0; p < 5; p++) {
        u16 unit  = powers[p];
        u8  digit = 0;

        while (value >= unit) {               // repeated subtraction
            value = (u16)(value - unit);
            digit++;
        }

        if (digit != 0 || started != 0 || p == 4) {
            put_char((char)('0' + digit));
            started = 1;
        }
    }
}

static void put_s16(s16 value) {
    if (value < 0) {                          // signed compare, then negate
        put_char('-');
        put_u16((u16)(0 - (u16)value));
    } else {
        put_u16((u16)value);
    }
}

//===----------------------------------------------------------------------===//
// The operands.  Volatile so the compiler must load them at run time instead
// of folding the entire program into a constant.
//===----------------------------------------------------------------------===//

volatile u16 operand_a = 1234;
volatile u16 operand_b = 5678;

static int failures = 0;

static void check(const char *label, u16 got, u16 want) {
    put_str("  ");
    put_str(label);
    put_str(" = ");
    put_u16(got);
    if (got != want) {
        put_str("   WRONG, expected ");
        put_u16(want);
        failures++;
    }
    put_char('\n');
}

//===----------------------------------------------------------------------===//
// Addition, three ways.  Not static, so they stay real calls and the operands
// travel through the calling convention.
//===----------------------------------------------------------------------===//

// One ADD on the low byte, one ADC on the high byte.
u16 add_direct(u16 a, u16 b) {
    return (u16)(a + b);
}

// The same sum reached by a counted loop: a back edge and a compare.
u16 add_by_loop(u16 a, u16 b) {
    u16 total = a;
    for (u16 i = 0; i < b; i++)
        total = (u16)(total + 1);
    return total;
}

// The same sum again, a hundred at a time, with a branch inside the loop.
u16 add_by_chunks(u16 a, u16 b) {
    u16 total = a;
    u16 left  = b;

    do {
        if (left >= 100) {
            total = (u16)(total + 100);
            left  = (u16)(left - 100);
        } else {
            total = (u16)(total + left);
            left  = 0;
        }
    } while (left != 0);

    return total;
}

// A branch chooses whether to add or subtract.
s16 add_signed(s16 a, s16 b) {
    if (b < 0)
        return (s16)(a - (s16)(0 - b));
    return (s16)(a + b);
}

// Count how many steps it takes to cross a limit.
u16 add_until(u16 start, u16 step, u16 limit) {
    u16 value  = start;
    u16 rounds = 0;
    do {
        value = (u16)(value + step);
        rounds++;
    } while (value < limit);
    return rounds;
}

//===----------------------------------------------------------------------===//

int main(void) {
    u16 a = operand_a;
    u16 b = operand_b;

    put_str("adding ");
    put_u16(a);
    put_str(" and ");
    put_u16(b);
    put_char('\n');

    put_line(" three ways to the same answer");
    u16 direct = add_direct(a, b);
    u16 looped = add_by_loop(a, b);
    u16 chunks = add_by_chunks(a, b);

    check("a + b   (one ADD/ADC)",   direct, 6912);
    check("a + b   (loop of 1s)",    looped, 6912);
    check("a + b   (loop of 100s)",  chunks, 6912);

    put_str("  all three agree? ");
    if (direct == looped && looped == chunks) {
        put_line("yes");
    } else {
        put_line("NO");
        failures++;
    }

    put_line(" branch chooses the operation");
    check("100 + 25",  (u16)add_signed(100,  25), 125);
    check("100 + -25", (u16)add_signed(100, -25),  75);

    put_line(" loop until a limit is crossed");
    check("steps of 7 to reach 100",    add_until(0,   7,  100), 15);
    check("steps of 250 to reach 1000", add_until(0, 250, 1000),  4);

    put_line(" carry out of the low byte");
    check("255 + 1",   add_direct(255, 1),   256);
    check("65535 + 1", add_direct(65535, 1),   0);

    put_str(" signed printing: ");
    put_s16(-32000);
    put_char('\n');

    put_char('\n');
    if (failures == 0)
        put_line("PASS - 9 checks, all correct");
    else {
        put_str("FAIL - wrong answers: ");
        put_u16((u16)failures);
        put_char('\n');
    }
    return failures;
}
