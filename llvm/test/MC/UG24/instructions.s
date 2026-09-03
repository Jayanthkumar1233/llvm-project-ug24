; RUN: llvm-mc -triple=ug24-unknown-none-eabi -show-encoding %s | FileCheck %s
;
; Encodings are taken from "Copy of uG24xx1616uP_ISA.xlsx".

; Format I - 8-bit immediate, Inst{0} = 1
; CHECK: ld  r1, [4]           ; encoding: [0x11,0x04]
        ld      r1, [4]
; CHECK: mvi r0, 18            ; encoding: [0x03,0x12]
        mvi     r0, 0x12
; CHECK: st  r2, [8]           ; encoding: [0x85,0x20]
        st      r2, [8]
; CHECK: andi r3, 15           ; encoding: [0x37,0x0f]
        andi    r3, 0x0f
; CHECK: ori r4, 240           ; encoding: [0x49,0xf0]
        ori     r4, 0xf0
; CHECK: xori r5, 170          ; encoding: [0x5b,0xaa]
        xori    r5, 0xaa
; CHECK: adi r6, 1             ; encoding: [0x6d,0x01]
        adi     r6, 1
; CHECK: sbi r7, 2             ; encoding: [0x7f,0x02]
        sbi     r7, 2

; Format A - arithmetic, Inst{3-2} = 10
; CHECK: add r0, r1            ; encoding: [0x08,0x10]
        add     r0, r1
; CHECK: adc r0, r1            ; encoding: [0x08,0x11]
        adc     r0, r1
; CHECK: sub r2, r3            ; encoding: [0x28,0x34]
        sub     r2, r3
; CHECK: sbb r2, r3            ; encoding: [0x28,0x35]
        sbb     r2, r3
; CHECK: inc r4, 1             ; encoding: [0x48,0x08]
        inc     r4, 1
; CHECK: dec r5, 16            ; encoding: [0x58,0xf9]
        dec     r5, 16

; Format L - logical and shifts, Inst{3-2} = 11
; CHECK: and r6, r7            ; encoding: [0x6c,0x70]
        and     r6, r7
; CHECK: or  r8, r9            ; encoding: [0x8c,0x92]
        or      r8, r9
; CHECK: xor r10, r11          ; encoding: [0xac,0xb4]
        xor     r10, r11
; CHECK: not r12               ; encoding: [0xcc,0x06]
        not     r12
; CHECK: lsl r0, 1             ; encoding: [0x0c,0x08]
        lsl     r0, 1
; CHECK: lsr r0, 8             ; encoding: [0x0c,0x79]
        lsr     r0, 8
; CHECK: asr r0, 3             ; encoding: [0x0c,0x2c]
        asr     r0, 3
; CHECK: rsl r0, 2             ; encoding: [0x0c,0x1a]
        rsl     r0, 2
; CHECK: rsr r0, 4             ; encoding: [0x0c,0x3b]
        rsr     r0, 4
; CHECK: clrf 8                ; encoding: [0x0c,0x8e]
        clrf    8
; CHECK: invf 0                ; encoding: [0x0c,0x0f]
        invf    0

; Format P - two implied operands, Inst{3-0} = 0000
; CHECK: swap r1, r2           ; encoding: [0x20,0x12]
        swap    r1, r2
; CHECK: mul r3, r4            ; encoding: [0x40,0x34]
        mul     r3, r4
; CHECK: div r5, r6            ; encoding: [0x50,0x56]
        div     r5, r6
; CHECK: cmp r7, r8            ; encoding: [0x60,0x78]
        cmp     r7, r8
; CHECK: nop                   ; encoding: [0x00,0x00]
        nop
; CHECK: ret                   ; encoding: [0x00,0x01]
        ret
; CHECK: fncb                  ; encoding: [0x00,0x02]
        fncb
; CHECK: fnca                  ; encoding: [0x00,0x03]
        fnca
; CHECK: wfi                   ; encoding: [0x00,0x80]
        wfi

; Format S - stack and register transfer, Inst{3-0} = 0100
; CHECK: mov r0, r1            ; encoding: [0x04,0x10]
        mov     r0, r1
; CHECK: push r2               ; encoding: [0x04,0x28]
        push    r2
; CHECK: pop r3                ; encoding: [0x34,0x0a]
        pop     r3
; CHECK: push pc               ; encoding: [0x04,0x09]
        push    pc
; CHECK: push ra               ; encoding: [0x04,0x19]
        push    ra
; CHECK: push psw              ; encoding: [0x04,0x29]
        push    psw
; CHECK: pop pc                ; encoding: [0x04,0x0b]
        pop     pc
; CHECK: pop ra                ; encoding: [0x14,0x0b]
        pop     ra
; CHECK: pop psw               ; encoding: [0x24,0x0b]
        pop     psw
; CHECK: mov w, pc             ; encoding: [0x44,0x01]
        mov     w, pc
; CHECK: mov dptr1, ra         ; encoding: [0x64,0x11]
        mov     dptr1, ra
; CHECK: mov dptr0, psw        ; encoding: [0x74,0x21]
        mov     dptr0, psw
; CHECK: mov w, sp             ; encoding: [0x44,0x31]
        mov     w, sp
; CHECK: mov ra, w             ; encoding: [0x14,0x42]
        mov     ra, w
; CHECK: mov sp, dptr0         ; encoding: [0x34,0x72]
        mov     sp, dptr0
; CHECK: swap w, ra            ; encoding: [0x30,0x41]
        swap    w, ra
; CHECK: swap dptr1, sp        ; encoding: [0x30,0x63]
        swap    dptr1, sp
