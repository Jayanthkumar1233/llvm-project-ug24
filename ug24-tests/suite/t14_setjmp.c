/* setjmp / longjmp.
 *
 * The buffer holds the five callee-saved registers, SP and RA, and nothing
 * else: DPTR0 is the memory base register and is reloaded at every access, so
 * no caller expects it back.  The cases below check that in turn -- a jump
 * out of a deep call chain, a jump that has to unwind a variable-length
 * array's stack adjustment, and callee-saved values that must survive.
 */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;
static int depth;

static void innermost(void) {
    depth++;
    longjmp(env, 42);
}

static void middle(void) {
    depth++;
    innermost();
    printf("FAIL: middle returned\n");
}

static void outer(void) {
    depth++;
    middle();
    printf("FAIL: outer returned\n");
}

/* A frame with a variable-length array moves SP behind the prologue's back,
 * so returning here through longjmp is what proves SP is restored and not
 * merely unwound one frame at a time. */
static void with_vla(int n) {
    int vla[n];
    int i;
    for (i = 0; i < n; i++)
        vla[i] = i;
    if (vla[n - 1] != n - 1)
        printf("FAIL: vla\n");
    longjmp(env, 7);
}

int main(void) {
    volatile int callee_saved_a = 11, callee_saved_b = 22;
    int value;

    value = setjmp(env);
    if (value == 0) {
        outer();
        printf("FAIL: outer did not jump\n");
    } else {
        printf("returned %d after %d calls\n", value, depth);
    }

    /* longjmp(env, 0) must still make setjmp return 1. */
    if ((value = setjmp(env)) == 0)
        longjmp(env, 0);
    printf("zero becomes %d\n", value);

    if ((value = setjmp(env)) == 0)
        with_vla(6);
    printf("vla frame unwound, value %d\n", value);

    /* The locals either side of the jumps have to be intact.  volatile
     * because a non-volatile local whose value changes between setjmp and
     * longjmp is undefined in C, and this test is about the register state,
     * not about that rule. */
    printf("locals %d %d\n", callee_saved_a, callee_saved_b);

    printf("PASS - setjmp\n");
    return 0;
}
