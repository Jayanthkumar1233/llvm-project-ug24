; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s
; RUN: llc -mtriple=ug24-unknown-none-eabi -mcpu=ug24-base < %s \
; RUN:   | FileCheck --check-prefix=BASE %s
;
; The multiplier and the divider are optional blocks in the SoC
; configuration.  Without them the same operations become calls to the
; runtime helpers, which are shift-and-add loops and need no hardware of
; their own.

define i8 @mul8(i8 %a, i8 %b) {
; CHECK-LABEL: mul8:
; CHECK: mul r0, r1
; BASE-LABEL: mul8:
; BASE-NOT: mul r
; BASE: __mulqi3
  %r = mul i8 %a, %b
  ret i8 %r
}

define i8 @udiv8(i8 %a, i8 %b) {
; CHECK-LABEL: udiv8:
; CHECK: div r0, r1
; BASE-LABEL: udiv8:
; BASE-NOT: div r
; BASE: __udivqi3
  %r = udiv i8 %a, %b
  ret i8 %r
}

define i8 @urem8(i8 %a, i8 %b) {
; CHECK-LABEL: urem8:
; CHECK: div r0, r1
; BASE-LABEL: urem8:
; BASE-NOT: div r
; BASE: __umodqi3
  %r = urem i8 %a, %b
  ret i8 %r
}
