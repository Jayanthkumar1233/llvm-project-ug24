; RUN: llc -mtriple=ug24-unknown-none-eabi < %s | FileCheck %s
;
; An interrupt handler is entered by the hardware, not by a call, so the
; caller-saved/callee-saved split does not apply to it: every register it
; writes has to come back, and that includes R11 and DPTR0, which are
; reserved and so invisible to the generic callee-saved machinery.  It
; returns by undoing what the hardware pushed rather than through RA.

@count = internal global i8 0
@flag = internal global i8 0

define void @handler() #0 {
; CHECK-LABEL: handler:
; The scratch registers the body uses are preserved...
; CHECK:      push r0
; CHECK:      push r14
; CHECK:      push r15
; ... and restored in the opposite order, before the return.
; CHECK:      pop r15
; CHECK-NEXT: pop r14
; CHECK:      pop r0
; CHECK-NEXT: pop psw
; CHECK-NEXT: pop pc
; CHECK-NOT:  ret
  %v = load volatile i8, ptr @flag
  %c = load volatile i8, ptr @count
  %s = add i8 %c, %v
  store volatile i8 %s, ptr @count
  ret void
}

; An ordinary function with the same body saves nothing and returns with RET.
define void @ordinary() {
; CHECK-LABEL: ordinary:
; CHECK-NOT:  push r0
; CHECK:      ret
  %v = load volatile i8, ptr @flag
  %c = load volatile i8, ptr @count
  %s = add i8 %c, %v
  store volatile i8 %s, ptr @count
  ret void
}

attributes #0 = { "interrupt" }
