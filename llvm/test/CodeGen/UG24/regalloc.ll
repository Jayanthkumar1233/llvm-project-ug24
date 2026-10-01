; RUN: llc -mtriple=ug24-unknown-elf < %s | FileCheck %s
;
; Every even-aligned register pair can hold a 16-bit value, not just the three
; the instruction set names for its extended-register field.

; Three live 16-bit values fit in the caller-saved pairs P0-P2, so a leaf
; function this size touches the stack not at all.
define i16 @three_live_values(i16 %a, i16 %b) {
; CHECK-LABEL: three_live_values:
; CHECK-NOT: Folded Spill
; CHECK-NOT: push
  %p = add i16 %a, 1
  %t = add i16 %p, %b
  ret i16 %t
}

; Four live values need a fourth pair, and the ABI leaves only three
; caller-saved ones (R0-R5), so the allocator borrows a callee-saved pair and
; gives it back.  What this checks is that it *can*: P3 = R6:R7 is allocatable,
; which is the whole point of declaring all eight pairs rather than the three
; the extended-register field can name.  The values are computed rather than
; passed, because only four bytes of arguments arrive in registers.
define i16 @four_live_values(i16 %a, i16 %b) {
; CHECK-LABEL: four_live_values:
; CHECK: r6
  %p = add i16 %a, 1
  %q = add i16 %b, 2
  %r = xor i16 %a, 3
  %s = xor i16 %b, 4
  %t = add i16 %p, %q
  %u = add i16 %r, %s
  %v = xor i16 %t, %u
  ret i16 %v
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
