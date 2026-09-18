/* Interrupts.
 *
 * The model this exercises is an assumption -- see "Interrupts" in
 * docs/uG24-assumptions.md -- but everything below it is real: the attribute
 * makes the handler preserve every register it writes and return through PSW
 * and PC, and the simulator delivers the interrupt between two instructions
 * of the loop in main.
 *
 * There is no host equivalent, so .expected is written from what the program
 * must produce rather than generated.  Every number in it is exact: the
 * software source is raised a known number of times, and the timer runs
 * until a known count is reached.
 */
#include <stdio.h>
#include <ug24.h>

static volatile unsigned software_count;
static volatile unsigned timer_count;

/* Written so the handler needs real registers of its own: the multiply and
 * the memory traffic give the prologue something to preserve. */
__attribute__((interrupt)) void __ug24_irq(void) {
    unsigned char pending = UG24_IRQ_STATUS;

    if (pending & UG24_IRQ_SW)
        software_count = software_count * 3 + 1;
    if (pending & UG24_IRQ_TIMER)
        timer_count++;

    UG24_IRQ_STATUS = pending;      /* write 1s back to clear */
}

int main(void) {
    unsigned i;
    unsigned accumulator = 0;       /* not volatile: lives in registers
                                       across the deliveries, which is the
                                       point of saving them */

    /* Software source: exactly one delivery per raise. */
    UG24_IRQ_ENABLE = UG24_IRQ_SW;
    ug24_enable_interrupts();
    for (i = 0; i < 5; i++) {
        UG24_IRQ_RAISE = 1;         /* bit 1 = software */
        accumulator = accumulator * 2 + i;
    }
    ug24_disable_interrupts();

    printf("software %u\n", software_count);
    printf("accumulator %u\n", accumulator);

    /* Timer source: runs until the handler has counted far enough.  How many
     * instructions that takes depends on the optimisation level; how many
     * interrupts it takes does not. */
    timer_count = 0;
    /* Long enough that the handler finishes well inside one period.  Set it
     * to 40 and the handler takes longer than the interval at -O0: the core
     * spends its time re-entering the handler, main advances an instruction
     * at a time, and the count it finally reads is whatever the storm got
     * to.  That is real behaviour, not a simulator artefact, but it is not
     * what this test is measuring. */
    UG24_TIMER_LOAD = 255;
    UG24_IRQ_ENABLE = UG24_IRQ_TIMER;
    ug24_enable_interrupts();
    while (timer_count < 4)
        ;
    ug24_disable_interrupts();
    UG24_TIMER_LOAD = 0;
    UG24_IRQ_ENABLE = 0;

    printf("timer %u\n", timer_count);

    /* With interrupts off, a raised source stays pending and is not taken. */
    UG24_IRQ_ENABLE = UG24_IRQ_SW;
    UG24_IRQ_RAISE = 1;
    printf("masked %u pending %u\n", software_count,
           (unsigned)(UG24_IRQ_STATUS & UG24_IRQ_SW));
    UG24_IRQ_STATUS = UG24_IRQ_SW;
    UG24_IRQ_ENABLE = 0;

    printf("PASS - interrupts\n");
    return 0;
}
