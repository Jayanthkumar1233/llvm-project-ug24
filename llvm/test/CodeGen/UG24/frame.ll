; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s

declare void @callee(ptr)

; A non-leaf function must preserve RA, which the next call would overwrite.
define i16 @has_call(i16 %a) {
; CHECK-LABEL: has_call:
; CHECK: push ra
; CHECK: lja callee
; CHECK: pop ra
; CHECK: ret
  %slot = alloca i16
  store i16 %a, ptr %slot
  call void @callee(ptr %slot)
  %v = load i16, ptr %slot
  ret i16 %v
}

; A leaf needs no return-address save and no frame at all.
define i8 @leaf(i8 %a, i8 %b) {
; CHECK-LABEL: leaf:
; CHECK-NOT: push ra
; CHECK: ret
  %r = add i8 %a, %b
  ret i8 %r
}

; The stack pointer is a special function register, so adjusting it goes
; through DPTR0.
define i8 @big_frame() {
; CHECK-LABEL: big_frame:
; CHECK: mov dptr0, sp
; CHECK: mov sp, dptr0
  %buf = alloca [40 x i8]
  call void @callee(ptr %buf)
  %p = getelementptr [40 x i8], ptr %buf, i16 0, i16 3
  %v = load i8, ptr %p
  ret i8 %v
}
