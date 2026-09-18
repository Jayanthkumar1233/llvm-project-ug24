// RUN: %clang_cc1 -triple ug24-unknown-none-eabi -emit-llvm -o - %s | FileCheck %s
//
// __attribute__((interrupt)) becomes the "interrupt" function attribute the
// backend looks for, plus noinline: nothing calls a handler, so inlining it
// into an ordinary function would silently drop the entry and exit code that
// makes it a handler.

// CHECK: define {{.*}}void @handler() #[[ATTRS:[0-9]+]]
__attribute__((interrupt)) void handler(void) {}

// CHECK: define {{.*}}void @plain() #[[PLAIN:[0-9]+]]
void plain(void) {}

// CHECK: attributes #[[ATTRS]] = {{{.*}}noinline{{.*}}"interrupt"
// CHECK-NOT: attributes #[[PLAIN]] = {{{.*}}"interrupt"
