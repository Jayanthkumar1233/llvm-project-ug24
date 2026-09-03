; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s
;
; Every even-aligned register pair can hold a 16-bit value, not just the three
; the instruction set names for its extended-register field.  With only those
; three, code like this would spill.

define i16 @four_live_values(i16 %a, i16 %b, i16 %c, i16 %d) {
; CHECK-LABEL: four_live_values:
; CHECK-NOT: Folded Spill
  %x = add i16 %a, %b
  %y = add i16 %c, %d
  %z = xor i16 %x, %y
  ret i16 %z
}

; A 16-bit branch compares the two halves directly rather than materialising a
; 0/1 byte and then testing that.
define i16 @loop(i16 %n) {
; CHECK-LABEL: loop:
; CHECK: cmp r
; CHECK: cmp r
entry:
  br label %body
body:
  %i = phi i16 [ 0, %entry ], [ %next, %body ]
  %acc = phi i16 [ 0, %entry ], [ %sum, %body ]
  %sum = add i16 %acc, %i
  %next = add i16 %i, 1
  %done = icmp eq i16 %next, %n
  br i1 %done, label %exit, label %body
exit:
  ret i16 %sum
}
