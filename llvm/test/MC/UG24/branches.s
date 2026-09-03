; RUN: llvm-mc -triple=ug24-unknown-none-eabi -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --triple=ug24-unknown-none-eabi %t.o | FileCheck %s
; RUN: llvm-readobj --relocations %t.o | FileCheck --check-prefix=RELOC %s
;
; A conditional branch encodes a signed count of instruction words taken from
; the instruction after the branch, so the displacement to a label two
; instructions ahead is 1.

        .text
        .globl  start
start:
; CHECK: beq {{.*}}0x0006
        beq     target
; CHECK: bne {{.*}}0x0006
        bne     target
; CHECK: jr {{.*}}0x0006
        jr      target
target:
; CHECK: ja
        ja      extfunc
; CHECK: lja
        lja     extfunc
; CHECK: mvi r0, 0
        mvi     r0, lo8(gvar)
; CHECK: mvi r1, 0
        mvi     r1, hi8(gvar)

; RELOC: R_UG24_ABS16 extfunc
; RELOC: R_UG24_ABS16 extfunc
; RELOC: R_UG24_LO8 gvar
; RELOC: R_UG24_HI8 gvar

        .data
        .globl  gvar
gvar:   .short  0x1234
