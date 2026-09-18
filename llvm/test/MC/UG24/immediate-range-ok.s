; RUN: llvm-mc -triple=ug24-unknown-none-eabi -filetype=obj %s -o /dev/null
;
; The other side of MC/UG24/immediate-range.s: everything at the edge of its
; range still assembles, and a symbolic expression is not range-checked in the
; parser at all -- its value is not known until the fixup is applied, and
; UG24AsmBackend checks it there.

        .text
        adi     r0, -128
        adi     r0, 255
        sbi     r0, -128
        mvi     r1, 255
        lsl     r3, 1
        lsl     r3, 8
        rsr     r3, 8
        inc     r2, 1
        dec     r2, 16
        clrf    0
        clrf    15
        invf    15
        ld      r0, [0]
        st      r0, [255]
        mvi     r1, lo8(start)
        mvi     r1, hi8(start)
start:
