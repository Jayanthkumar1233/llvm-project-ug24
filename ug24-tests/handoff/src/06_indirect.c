/* 06_indirect - the one program in this kit whose output answers a question we
 * cannot answer ourselves.
 *
 * The uG24 has no call-indirect instruction, so a call through a function
 * pointer is built by hand: read PC, add the length of the sequence, put that in
 * RA as the return address, push the target and POP PC to it.  The constant
 * added to PC depends on something the specification does not say -- whether
 * `MOV Xd, PC` yields the address of the MOV itself or the address of the next
 * instruction.  We assume the former, so we add 16; if it is the latter the
 * right constant is 14.
 *
 * Our own simulator shares that assumption, so our tests cannot tell the
 * difference.  An independently written one can.  Run this and compare:
 *
 *   twice(21)  = 42  and  square(7)  = 49   -> PC reads the address of the MOV,
 *                                             which is what we assume
 *   anything else                           -> PC reads something else; tell us
 *                                             what you get and what your core
 *                                             does, because our compiler is
 *                                             then emitting the wrong constant
 *
 * On a simulator that reads PC+2 this prints 0 and 0 and then exits normally,
 * reporting no error at all -- which is why it is worth checking deliberately
 * rather than waiting for it to show up as a bug.
 *
 * `chosen` is volatile so that the optimiser cannot see which function it holds.
 * Without that it devirtualises the call into a direct one and the sequence this
 * program exists to exercise never gets emitted.
 */
#include <stdio.h>

static unsigned char twice(unsigned char x)  { return (unsigned char)(x * 2); }
static unsigned char square(unsigned char x) { return (unsigned char)(x * x); }

static unsigned char (*volatile chosen)(unsigned char);

int main(void)
{
    printf("uG24 sample 6: calls through a function pointer\n");

    chosen = twice;
    printf("  twice(21)        = %u   (expected 42)\n", chosen(21));

    chosen = square;
    printf("  square(7)        = %u   (expected 49)\n", chosen(7));

    return 0;
}
