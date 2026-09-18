; RUN: llvm-mc -triple=ug24-unknown-none-eabi -show-encoding %s | FileCheck %s
; RUN: not llvm-mc -triple=ug24-unknown-none-eabi -mattr=-mul,-div -filetype=obj \
; RUN:   %s -o /dev/null 2>&1 | FileCheck --check-prefix=BASE %s
;
; The multiplier and the divider are optional blocks in the SoC
; configuration, so the assembler refuses their instructions on a part built
; without them rather than encoding something that cannot execute.

        .text
; CHECK: mul r1, r0
; BASE: error: instruction requires an optional block that this -mcpu or -mattr leaves out
        mul     r1, r0
; CHECK: div r0, r1
; BASE: error: instruction requires an optional block that this -mcpu or -mattr leaves out
        div     r0, r1
