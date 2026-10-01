;===-- ug24_setjmp.s - setjmp/longjmp for the uG24 ----------------------===;
;
; Written in assembly because a C function cannot see its own return address:
; the uG24 leaves it in RA rather than on the stack, and a prologue would
; already have moved the stack pointer by the time C code ran.
;
; The buffer holds everything the confirmed ABI says survives a call, which is
; what the longjmp target will expect to find in place:
;
;   [0..5]  R6, R7, R8, R9, R10, R11   the callee-saved registers
;   [6..7]  SP                         the stack pointer at the call to setjmp
;   [8..9]  RA                         where setjmp was going to return to
;
; Neither data pointer is saved.  DPTR0 is the memory base register and every
; access reloads it; DPTR1 is reserved by the ABI and holds nothing a caller
; owns.  No caller expects either to survive a call.
;
;===--------------------------------------------------------------------===;

	.text

	.globl	setjmp
	.type	setjmp,@function
setjmp:
	; The buffer pointer arrives in X0 (R0:R1); LD and ST address through
	; DPTR0, and PSW.DP clear selects DPTR0 rather than DPTR1.
	mov	r14, r0
	mov	r15, r1
	clrf	8

	st	r6,  [0]
	st	r7,  [1]
	st	r8,  [2]
	st	r9,  [3]
	st	r10, [4]
	st	r11, [5]

	; SP and RA are special function registers, so each goes through a pair
	; first.  DPTR1 serves: it is reserved, so nothing live is in it.
	mov	dptr1, sp
	st	r12, [6]
	st	r13, [7]
	mov	dptr1, ra
	st	r12, [8]
	st	r13, [9]

	; Return 0 in X0.  This is the direct call; longjmp arranges for the
	; other return to be non-zero.
	mvi	r0, 0
	mvi	r1, 0
	ret
	.size	setjmp, .-setjmp

	.globl	longjmp
	.type	longjmp,@function
longjmp:
	; Buffer in X0 (R0:R1), value in X1 (R2:R3).  Nothing below writes R2 or
	; R3, so the value stays where it arrived until the end.
	mov	r14, r0
	mov	r15, r1
	clrf	8

	ld	r6,  [0]
	ld	r7,  [1]
	ld	r8,  [2]
	ld	r9,  [3]
	ld	r10, [4]
	ld	r11, [5]

	; RA before SP: both go through DPTR1, and once SP has moved this code
	; is running on the target's stack frame.
	ld	r12, [8]
	ld	r13, [9]
	mov	ra, dptr1
	ld	r12, [6]
	ld	r13, [7]
	mov	sp, dptr1

	; setjmp returns the value, except that longjmp(env, 0) must still
	; return 1 so that the two returns stay distinguishable.
	mov	r0, r2
	mov	r1, r3
	or	r2, r3
	bnz	.Lnonzero
	mvi	r0, 1
	mvi	r1, 0
.Lnonzero:
	ret
	.size	longjmp, .-longjmp
