; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s

define i8 @add8(i8 %a, i8 %b) {
; CHECK-LABEL: add8:
; CHECK: add r0, r1
  %r = add i8 %a, %b
  ret i8 %r
}

define i8 @and8(i8 %a, i8 %b) {
; CHECK-LABEL: and8:
; CHECK: and r0, r1
  %r = and i8 %a, %b
  ret i8 %r
}

define i8 @shl8(i8 %a) {
; CHECK-LABEL: shl8:
; CHECK: lsl r0, 3
  %r = shl i8 %a, 3
  ret i8 %r
}

define i8 @addimm8(i8 %a) {
; CHECK-LABEL: addimm8:
; CHECK: adi r0, 7
  %r = add i8 %a, 7
  ret i8 %r
}

; A 16-bit add is a byte add plus a carry-propagating add.
define i16 @add16(i16 %a, i16 %b) {
; CHECK-LABEL: add16:
; CHECK-DAG: add r
; CHECK-DAG: adc r
  %r = add i16 %a, %b
  ret i16 %r
}

; A 16-bit shift by a constant is expanded into byte shifts, never a libcall.
define i16 @shr16(i16 %a) {
; CHECK-LABEL: shr16:
; CHECK-NOT: lja
; CHECK: lsr
  %r = lshr i16 %a, 4
  ret i16 %r
}
