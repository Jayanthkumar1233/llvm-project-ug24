// RUN: %clang_cc1 -triple ug24-unknown-none-eabi -verify -fsyntax-only %s
//
// The handler is wired to a vector by its name, through the weak symbols in
// crt0.s, so the attribute takes no arguments -- unlike the MSP430 and ARM
// spellings it shares a name with.

__attribute__((interrupt)) void ok(void) {}

__attribute__((interrupt(1))) void numbered(void); // expected-error {{'interrupt' attribute takes no arguments}}

struct S { int x; } __attribute__((interrupt)); // expected-warning {{'interrupt' attribute only applies to functions}}

int variable __attribute__((interrupt)); // expected-warning {{'interrupt' attribute only applies to functions}}
