// RUN: %clang_cc1 -triple ug24-unknown-none-eabi -emit-llvm -o - %s | FileCheck %s
//
// The uG24 specification defines no C ABI, so this pins the one in
// docs/uG24-assumptions.md.  What Clang decides here is direct versus
// indirect; UG24CallingConv.td assigns the registers.

struct Small { char a, b; };
struct Big { int a, b, c, d; };
struct Empty {};

// A scalar of 32 bits or fewer goes in registers.
// CHECK-LABEL: define {{.*}}i16 @scalar16(i16 noundef signext %a)
short scalar16(short a) { return a; }

// CHECK-LABEL: define {{.*}}i32 @scalar32(i32 noundef %a)
long scalar32(long a) { return a; }

// 64 bits still fits: four 16-bit pairs, which is what a return value has.
// CHECK-LABEL: define {{.*}}i64 @scalar64(i64 noundef %a)
long long scalar64(long long a) { return a; }

// A narrow integer is promoted, as C requires, so that a callee compiled from
// a prototype-less declaration agrees with its caller.
// CHECK-LABEL: define {{.*}}signext i8 @narrow(i8 noundef signext %a)
char narrow(char a) { return a; }

// Aggregates are passed by reference with the caller owning the copy, and
// returned through a hidden pointer.  The uG24 has no block move, so pushing
// a struct a byte at a time at every call site costs more than the copy the
// callee would have made.
// CHECK-LABEL: define {{.*}}void @small(ptr {{.*}}sret(%struct.Small){{.*}}, ptr {{.*}}byval(%struct.Small)
struct Small small(struct Small s) { return s; }

// CHECK-LABEL: define {{.*}}void @big(ptr {{.*}}sret(%struct.Big){{.*}}, ptr {{.*}}byval(%struct.Big)
struct Big big(struct Big s) { return s; }

// An empty struct occupies nothing and is passed and returned in nothing.
// CHECK-LABEL: define {{.*}}void @empty()
struct Empty empty(struct Empty e) { return e; }

// double is IEEE single on this target, so it is one 32-bit value.
// CHECK-LABEL: define {{.*}}float @real(float noundef %a)
double real(double a) { return a; }
