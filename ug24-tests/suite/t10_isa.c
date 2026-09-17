//===-- t10_isa.c - execute the instructions nothing had executed --------===//
//
// RSL, RSR, SWAP, CLRF, INVF, FNCA, FNCB, PUSH/POP of the special registers
// and the second data pointer all had encoding tests and a disassembler
// entry, and none of them had ever been run.  An encoding test proves the
// assembler and disassembler agree with each other; it says nothing about
// whether the instruction does what the specification claims.
//
// The helpers in t10_isa.s reach the ones no C construct can.  RSL is the
// exception: LLVM recognises the shift-or idiom, so it is checked from C as
// well as from assembly, and the two must agree.
//
//   Build and run:  ug24-tests/suite/run-suite.sh t10_isa
//
//===----------------------------------------------------------------------===//

#include <stdio.h>

unsigned char  asm_rsr3(unsigned char v);
unsigned char  asm_rsl3(unsigned char v);
unsigned short asm_swap(unsigned short v);
unsigned char  asm_flags(void);
unsigned char  asm_pushpop(unsigned char v);
void           asm_fence(void);
unsigned char  asm_load_via_dptr1(const unsigned char *p);

static int failures = 0;

static void check(const char *label, unsigned got, unsigned want)
{
    printf("  %-22s = %04X", label, got);
    if (got != want) {
        printf("   WRONG, expected %04X", want);
        failures++;
    }
    printf("\n");
}

// The compiler turns this into RSL on its own.
static unsigned char rotl3(unsigned char v)
{
    return (unsigned char)((v << 3) | (v >> 5));
}

volatile unsigned char table[4] = { 0x11, 0x22, 0x33, 0x44 };

int main(void)
{
    printf("rotates\n");
    // 0x81 = 1000 0001.  Left 3 -> 0000 1100 = 0x0C.  Right 3 -> 0011 0000 = 0x30.
    check("rsl 0x81 by 3", asm_rsl3(0x81), 0x0C);
    check("rsr 0x81 by 3", asm_rsr3(0x81), 0x30);
    check("rsl via C idiom", rotl3(0x81), 0x0C);
    check("rsl 0xFF by 3", asm_rsl3(0xFF), 0xFF);
    check("rsr 0x01 by 3", asm_rsr3(0x01), 0x20);

    printf("swap\n");
    check("swap 0xBEEF", asm_swap(0xBEEF), 0xEFBE);
    check("swap 0x00FF", asm_swap(0x00FF), 0xFF00);

    printf("flags\n");
    check("clrf/invf on PSW.DP", asm_flags(), 1);

    printf("stack\n");
    check("push/pop byte + psw", asm_pushpop(0x5A), 0x5A);
    check("push/pop 0xA5", asm_pushpop(0xA5), 0xA5);

    printf("fences\n");
    asm_fence();          // no observable effect; must not disturb anything
    check("fnca/fncb survived", asm_pushpop(0x3C), 0x3C);

    printf("second data pointer\n");
    check("load table[0] via dptr1", asm_load_via_dptr1((const unsigned char *)&table[0]), 0x11);
    check("load table[3] via dptr1", asm_load_via_dptr1((const unsigned char *)&table[3]), 0x44);
    // PSW.DP must be back to 0, or every later load through DPTR0 is wrong.
    check("dptr0 still works after", table[2], 0x33);

    printf("\n");
    if (failures == 0)
        printf("PASS - 14 checks, all correct\n");
    else
        printf("FAIL - wrong answers: %d\n", failures);
    return failures;
}
