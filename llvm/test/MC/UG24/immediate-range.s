; RUN: not llvm-mc -triple=ug24-unknown-none-eabi -filetype=obj %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s
;
; Out-of-range immediates are diagnosed rather than truncated.  The compiler
; never produces one -- it materialises a wide constant properly -- so this is
; about hand-written assembly, which is where crt0.s and any interrupt service
; routine live.

        .text
; CHECK: error: immediate must be an integer in the range [-128, 255]
; CHECK-NEXT: adi r0, 4660
        adi     r0, 4660
; CHECK: error: immediate must be an integer in the range [-128, 255]
; CHECK-NEXT: mvi r1, 300
        mvi     r1, 300
; CHECK: error: immediate must be an integer in the range [1, 8]
; CHECK-NEXT: lsl r3, 9
        lsl     r3, 9
; CHECK: error: immediate must be an integer in the range [1, 16]
; CHECK-NEXT: inc r2, 0
        inc     r2, 0
; CHECK: error: immediate must be an integer in the range [1, 16]
; CHECK-NEXT: dec r2, 17
        dec     r2, 17
; CHECK: error: immediate must be an integer in the range [0, 15]
; CHECK-NEXT: clrf 16
        clrf    16
; CHECK: error: immediate must be an integer in the range [0, 255]
; CHECK-NEXT: ld r0, [300]
        ld      r0, [300]
