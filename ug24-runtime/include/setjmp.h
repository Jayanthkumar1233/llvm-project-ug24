//===-- setjmp.h - Non-local jumps for the uG24 ---------------------------===//
//
// The buffer holds the five callee-saved registers, the stack pointer and the
// return address: nine bytes, rounded up to ten.  See ug24-runtime/
// ug24_setjmp.s for the layout.
//
//===----------------------------------------------------------------------===//

#ifndef _SETJMP_H
#define _SETJMP_H

typedef unsigned char jmp_buf[10];

/// Record the current execution context in \p env and return 0.  Returns
/// again, with the value passed to longjmp(), each time that longjmp() is
/// called on the same buffer.
///
/// returns_twice is what stops the optimiser from assuming that the code
/// after a setjmp() runs only once, which is the usual way a soft-float or
/// register-allocated value goes missing across a longjmp().
__attribute__((returns_twice)) int setjmp(jmp_buf env);

/// Resume the context recorded in \p env, making the matching setjmp()
/// return \p value -- or 1 when \p value is zero.  Never returns.
///
/// The function that called setjmp() must not have returned yet.  Nothing
/// checks this: its stack frame has already been reused.
__attribute__((noreturn)) void longjmp(jmp_buf env, int value);

#endif // _SETJMP_H
