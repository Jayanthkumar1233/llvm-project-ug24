//===-- UG24RegisterInfo.td - UG24 Register defs -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Register file per "Copy of uG24xx1616uP_ISA.xlsx", sheet
// uG24081616uP_Registers.
//
// Sixteen 8-bit GPRs R0-R15 encoded in 4 bits.  Three of the eight adjacent
// pairs are also addressable as 16-bit "extended" registers through a 3-bit
// field: W = R9:R8 (code 100), DPTR1 = R13:R12 (code 110), DPTR0 = R15:R14
// (code 111).  The extended-register code is the GPR index shifted right by
// one, so it is *not* the same value as the 4-bit GPR encoding.
//
// Four 16-bit special function registers are selected by a 2-bit field:
// PC = 00, RA = 01, PSW = 10, SP = 11.
//
//===----------------------------------------------------------------------===//

class UG24Reg<bits<16> Enc, string n, list<Register> subregs = []>
    : Register<n> {
  let HWEncoding = Enc;
  let Namespace = "UG24";
  let SubRegs = subregs;
}

// Sub-register indices for the 16-bit pairs.  The low byte lives in the
// even-numbered GPR, matching the "R9:R8" (high:low) notation in the spec.
def sub_lo : SubRegIndex<8>;
def sub_hi : SubRegIndex<8, 8>;

//===----------------------------------------------------------------------===//
// 8-bit general purpose registers
//===----------------------------------------------------------------------===//

foreach i = 0-15 in {
  def R#i : UG24Reg<i, "r"#i>, DwarfRegNum<[i]>;
}

//===----------------------------------------------------------------------===//
// 16-bit register pairs
//===----------------------------------------------------------------------===//
//
// The instruction set only names three of the eight adjacent pairs - W,
// DPTR1 and DPTR0 - because only those can appear in the 3-bit "extended
// register" field of MOV Xd,SFR, SWAP Xs,SFR and JI/LJI.
//
// Ordinary 16-bit arithmetic never uses that field: it is synthesised from
// byte operations with ADD/ADC on the two halves, which works on any pair.
// So every even-aligned pair is declared here and made available to the
// register allocator, and only the instructions that really encode an
// extended register are restricted to XReg below.  Without this there would
// be two allocatable 16-bit registers, and since int is 16 bits on this
// target almost everything would spill.

let SubRegIndices = [sub_lo, sub_hi], CoveredBySubRegs = 1 in {
  def P0    : UG24Reg<0, "p0",    [R0,  R1]>,  DwarfRegNum<[16]>;
  def P1    : UG24Reg<1, "p1",    [R2,  R3]>,  DwarfRegNum<[17]>;
  def P2    : UG24Reg<2, "p2",    [R4,  R5]>,  DwarfRegNum<[18]>;
  def P3    : UG24Reg<3, "p3",    [R6,  R7]>,  DwarfRegNum<[19]>;
  def W     : UG24Reg<4, "w",     [R8,  R9]>,  DwarfRegNum<[20]>;
  def P5    : UG24Reg<5, "p5",    [R10, R11]>, DwarfRegNum<[21]>;
  def DPTR1 : UG24Reg<6, "dptr1", [R12, R13]>, DwarfRegNum<[22]>;
  def DPTR0 : UG24Reg<7, "dptr0", [R14, R15]>, DwarfRegNum<[23]>;
}

//===----------------------------------------------------------------------===//
// 16-bit special function registers (2-bit encoding)
//===----------------------------------------------------------------------===//

def PC  : UG24Reg<0, "pc">,  DwarfRegNum<[24]>;
def RA  : UG24Reg<1, "ra">,  DwarfRegNum<[25]>;
def PSW : UG24Reg<2, "psw">, DwarfRegNum<[26]>;
def SP  : UG24Reg<3, "sp">,  DwarfRegNum<[27]>;

//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

// Two registers are reserved and never allocated:
//
//   R11        - scratch byte used by the frame and pseudo expansions, which
//                need a temporary to hold the high byte of a 16-bit constant
//                for ADC/SBB.
//   R15:R14    - DPTR0, the base address register implicitly used by every
//                LD and ST.  Dedicating it removes the need to shuffle a
//                pointer into place around each memory access.
//
// R12/R13 (DPTR1) stay allocatable; PSW.DP is kept clear so that DPTR0 is the
// pointer the hardware selects.
def GPR : RegisterClass<"UG24", [i8], 8, (add
  R0, R1, R2, R3,       // argument / return / scratch
  R8, R9,               // W pair
  R12, R13,             // DPTR1 pair
  R4, R5, R6, R7, R10,  // callee saved
  R11,                  // reserved: expansion temporary
  R14, R15              // reserved: DPTR0
)>;

// Every pair that is free to hold a 16-bit value.  P5 is absent because it
// contains the reserved temporary R11, and DPTR0 is listed but reserved so
// that the frame code and the assembler can still name it.
def GPR16 : RegisterClass<"UG24", [i16], 8, (add
  P0, P1,          // argument / scratch pairs
  W,               // 16-bit return value
  DPTR1,           // second 16-bit argument
  P2, P3,          // callee saved
  DPTR0            // reserved: memory base register
)> {
  let Size = 16;
}

// The three pairs the hardware can name in a 3-bit extended-register field.
def XReg : RegisterClass<"UG24", [i16], 8, (add W, DPTR1, DPTR0)> {
  let Size = 16;
}

def SFR16 : RegisterClass<"UG24", [i16], 8, (add PC, RA, PSW, SP)> {
  let Size = 16;
  let isAllocatable = 0;
}

// PSW alone, so instructions can express their flag clobber precisely.
def FLAGS : RegisterClass<"UG24", [i16], 8, (add PSW)> {
  let Size = 16;
  let isAllocatable = 0;
  let CopyCost = -1;
}
