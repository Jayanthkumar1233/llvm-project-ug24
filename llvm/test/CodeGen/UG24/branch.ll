; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s

; An 8-bit comparison is a CMP followed by the matching conditional branch.
define i8 @cmp_eq(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_eq:
; CHECK: cmp r
; CHECK: b{{eq|ne}}
  %c = icmp eq i8 %a, %b
  br i1 %c, label %t, label %f
t:
  ret i8 1
f:
  ret i8 0
}

; A signed comparison flips the sign bit so the unsigned ordering CMP records
; gives the signed answer.
define i8 @cmp_slt(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_slt:
; CHECK-DAG: xori r{{[0-9]+}}, 128
; CHECK: cmp r
  %c = icmp slt i8 %a, %b
  br i1 %c, label %t, label %f
t:
  ret i8 1
f:
  ret i8 0
}

; A 16-bit comparison compares the high bytes, then the low bytes.
define i8 @cmp16(i16 %a, i16 %b) {
; CHECK-LABEL: cmp16:
; CHECK: cmp r
; CHECK: cmp r
  %c = icmp ult i16 %a, %b
  br i1 %c, label %t, label %f
t:
  ret i8 1
f:
  ret i8 0
}
