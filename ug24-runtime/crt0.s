;===-- crt0.s - uG24 C runtime startup ----------------------------------===;
;
; Sets up the stack pointer, clears .bss, copies .data into place, calls main
; and then halts.  The reset PC and SP of a real uG24 are strapped inputs, so
; this file assumes the core starts executing at the base of .text.
;
;===--------------------------------------------------------------------===;

	.text
	.globl	_start
	.type	_start,@function
_start:
	; SP <- __stack_top.  SP is a special function register, so the value is
	; built in DPTR0 first and then moved across.
	mvi	r14, lo8(__stack_top)
	mvi	r15, hi8(__stack_top)
	mov	sp, dptr0

	; PSW.DP = 0 so that LD/ST use DPTR0, which the compiler reserves as the
	; memory base register.
	clrf	8

	; Zero .bss, one byte at a time through DPTR0.
	mvi	r14, lo8(__bss_start)
	mvi	r15, hi8(__bss_start)
	mvi	r0, 0
.Lbss_loop:
	mvi	r1, lo8(__bss_end)
	cmp	r14, r1
	bne	.Lbss_store
	mvi	r1, hi8(__bss_end)
	cmp	r15, r1
	beq	.Lbss_done
.Lbss_store:
	st	r0, [0]
	; ADI is used rather than INC because INC leaves the flags alone and the
	; carry out of the low byte is what drives the high byte here.
	adi	r14, 1
	bnc	.Lbss_loop
	adi	r15, 1
	jr	.Lbss_loop
.Lbss_done:

	; Hand over to the application.
	lja	main

	; main returned: stop the core.
.Lhalt:
	wfi
	jr	.Lhalt
	.size	_start, .-_start
