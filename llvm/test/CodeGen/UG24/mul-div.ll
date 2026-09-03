; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s
;
; The hardware MUL is an 8x8 -> 16 multiply landing in W, and DIV puts the
; quotient in W's low half and the remainder in its high half.  None of these
; may fall back to a runtime call.

define i8 @mul8(i8 %a, i8 %b) {
; CHECK-LABEL: mul8:
; CHECK-NOT: lja
; CHECK: mul r
  %r = mul i8 %a, %b
  ret i8 %r
}

define i16 @widening_mul(i8 %a, i8 %b) {
; CHECK-LABEL: widening_mul:
; CHECK-NOT: lja
; CHECK: mul r
  %x = zext i8 %a to i16
  %y = zext i8 %b to i16
  %r = mul i16 %x, %y
  ret i16 %r
}

; Only the low byte of this product is used, so it narrows to the 8-bit form.
define i8 @truncated_mul(i16 %a, i16 %b) {
; CHECK-LABEL: truncated_mul:
; CHECK-NOT: lja
; CHECK: mul r
  %m = mul i16 %a, %b
  %r = trunc i16 %m to i8
  ret i8 %r
}

; Same narrowing when the result goes straight to memory as a byte.
define void @truncated_mul_store(i16 %a, i16 %b, ptr %p) {
; CHECK-LABEL: truncated_mul_store:
; CHECK-NOT: lja
; CHECK: mul r
  %m = mul i16 %a, %b
  %t = trunc i16 %m to i8
  store i8 %t, ptr %p
  ret void
}

define i8 @udiv8(i8 %a, i8 %b) {
; CHECK-LABEL: udiv8:
; CHECK-NOT: lja
; CHECK: div r
  %r = udiv i8 %a, %b
  ret i8 %r
}

define i8 @urem8(i8 %a, i8 %b) {
; CHECK-LABEL: urem8:
; CHECK-NOT: lja
; CHECK: div r
  %r = urem i8 %a, %b
  ret i8 %r
}

; A genuinely 16-bit multiply has no hardware support and must call the helper.
define i16 @mul16(i16 %a, i16 %b) {
; CHECK-LABEL: mul16:
; CHECK: lja __mulhi3
  %r = mul i16 %a, %b
  ret i16 %r
}
