;===-- ug24_setjmp.s - setjmp/longjmp for the uG24 ----------------------===;
;
; Written in assembly because a C function cannot see its own return address:
; the uG24 leaves it in RA rather than on the stack, and a prologue would
; already have moved the stack pointer by the time C code ran.
;
; The buffer holds everything the ABI says survives a call, which is what the
; longjmp target will expect to find in place:
;
;   [0..4]  R4, R5, R6, R7, R10   the callee-saved registers
;   [5..6]  SP                     the stack pointer at the call to setjmp
;   [7..8]  RA                     where setjmp was going to return to
;
; DPTR0 is not saved: it is the memory base register and every access
; reloads it, so no caller expects it to survive a call.
;
;===--------------------------------------------------------------------===;

	.text

	.globl	setjmp
	.type	setjmp,@function
setjmp:
	; The buffer pointer arrives in W; LD and ST address through DPTR0.
	mov	r14, r8
	mov	r15, r9
	clrf	8

	st	r4,  [0]
	st	r5,  [1]
	st	r6,  [2]
	st	r7,  [3]
	st	r10, [4]

	; SP and RA are special function registers, so each goes through a
	; pair first.  DPTR1 is free: it is an argument register and setjmp
	; takes only one argument.
	mov	dptr1, sp
	st	r12, [5]
	st	r13, [6]
	mov	dptr1, ra
	st	r12, [7]
	st	r13, [8]

	; Return 0.  This is the direct call; longjmp arranges for the other
	; return to be non-zero.
	mvi	r8, 0
	mvi	r9, 0
	ret
	.size	setjmp, .-setjmp

	.globl	longjmp
	.type	longjmp,@function
longjmp:
	; Buffer in W, value in DPTR1.  The value has to move out of DPTR1
	; before DPTR1 is used to reload the special function registers.
	mov	r0, r12
	mov	r1, r13

	mov	r14, r8
	mov	r15, r9
	clrf	8

	ld	r4,  [0]
	ld	r5,  [1]
	ld	r6,  [2]
	ld	r7,  [3]
	ld	r10, [4]

	; RA before SP: both go through W, and once SP has moved this code is
	; running on the target's stack frame.
	ld	r8,  [7]
	ld	r9,  [8]
	mov	ra, w
	ld	r8,  [5]
	ld	r9,  [6]
	mov	sp, w

	; setjmp returns the value, except that longjmp(env, 0) must still
	; return 1 so that the two returns stay distinguishable.
	mov	r8, r0
	mov	r9, r1
	or	r0, r1
	bnz	.Lnonzero
	mvi	r8, 1
	mvi	r9, 0
.Lnonzero:
	ret
	.size	longjmp, .-longjmp
