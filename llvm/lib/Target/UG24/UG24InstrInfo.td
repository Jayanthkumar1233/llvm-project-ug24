//===-- UG24InstrInfo.td - UG24 Instruction defs -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 instruction set, transcribed from "Copy of uG24xx1616uP_ISA.xlsx".
// Opcode values in the comments are the OPCODES / bit-field columns of that
// sheet.
//
//===----------------------------------------------------------------------===//

include "UG24InstrFormats.td"

//===----------------------------------------------------------------------===//
// Target-specific DAG nodes
//===----------------------------------------------------------------------===//

def SDT_UG24Call     : SDTypeProfile<0, -1, [SDTCisVT<0, i16>]>;
def SDT_UG24CallSeq  : SDCallSeqStart<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_UG24CallSeqE : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_UG24Cmp      : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>, SDTCisVT<0, i8>]>;
def SDT_UG24MulW     : SDTypeProfile<1, 2, [SDTCisVT<0, i16>, SDTCisVT<1, i8>,
                                           SDTCisSameAs<1, 2>]>;
def SDT_UG24BrCC     : SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>,
                                            SDTCisVT<1, i8>]>;
def SDT_UG24Wrapper  : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                                            SDTCisPtrTy<0>]>;

def UG24retglue : SDNode<"UG24ISD::RET_GLUE", SDTNone,
                         [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def UG24mulw    : SDNode<"UG24ISD::MULW", SDT_UG24MulW>;

def UG24call    : SDNode<"UG24ISD::CALL", SDT_UG24Call,
                         [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                          SDNPVariadic]>;

def UG24callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_UG24CallSeq,
                               [SDNPHasChain, SDNPOutGlue]>;
def UG24callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_UG24CallSeqE,
                               [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

// CMP sets PSW; BRCC consumes it.
def UG24cmp   : SDNode<"UG24ISD::CMP", SDT_UG24Cmp, [SDNPOutGlue]>;
def UG24brcc  : SDNode<"UG24ISD::BR_CC", SDT_UG24BrCC,
                       [SDNPHasChain, SDNPInGlue]>;

def UG24wrapper : SDNode<"UG24ISD::WRAPPER", SDT_UG24Wrapper>;

// Naming the halves of a 16-bit register, and building one from two bytes.
// These let the 16-bit shift expansion work in bytes without going back
// through a 16-bit shift to get at the high half.
def SDT_UG24Half : SDTypeProfile<1, 1, [SDTCisVT<0, i8>, SDTCisVT<1, i16>]>;
def SDT_UG24Pair : SDTypeProfile<1, 2, [SDTCisVT<0, i16>, SDTCisVT<1, i8>,
                                        SDTCisVT<2, i8>]>;
def UG24lo8  : SDNode<"UG24ISD::LO8",  SDT_UG24Half>;
def UG24hi8  : SDNode<"UG24ISD::HI8",  SDT_UG24Half>;
def UG24pair : SDNode<"UG24ISD::PAIR", SDT_UG24Pair>;

// Conditional select on the flags left by a preceding CMP.
def SDT_UG24SelectCC : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                            SDTCisSameAs<1, 2>,
                                            SDTCisVT<3, i8>]>;
def UG24selectcc : SDNode<"UG24ISD::SELECT_CC", SDT_UG24SelectCC, [SDNPInGlue]>;

// A 16-bit conditional branch, taken apart into byte compares by a custom
// inserter.  Going straight to a branch avoids materialising a 0/1 byte and
// then comparing that against zero.
def SDT_UG24BrCC16 : SDTypeProfile<0, 4, [SDTCisVT<0, OtherVT>,
                                          SDTCisVT<1, i16>, SDTCisVT<2, i16>,
                                          SDTCisVT<3, i8>]>;
def UG24brcc16 : SDNode<"UG24ISD::BR_CC16", SDT_UG24BrCC16, [SDNPHasChain]>;

// Boolean result of a 16-bit comparison, materialised by a custom inserter.
def SDT_UG24SetCC16 : SDTypeProfile<1, 3, [SDTCisVT<0, i8>, SDTCisVT<1, i16>,
                                           SDTCisVT<2, i16>, SDTCisVT<3, i8>]>;
def UG24setcc16 : SDNode<"UG24ISD::SETCC16", SDT_UG24SetCC16>;

//===----------------------------------------------------------------------===//
// Operands and immediate predicates
//===----------------------------------------------------------------------===//

// Immediate range checking for hand-written assembly.
//
// The instruction selector only ever produces immediates that fit, because the
// ImmLeaf predicates below gate every pattern.  An assembly file has nothing
// gating it, so without a parser match class the matcher accepts any integer
// and the encoder quietly drops the bits that do not fit -- "adi r0, 4660"
// assembling as "adi r0, 52".  Each class names a predicate on UG24Operand and
// a diagnostic that MatchAndEmitInstruction turns into a range message.
//
// A symbolic expression passes every one of these: its value is not known
// until the fixup is applied, and the asm backend range-checks it there.
class UG24ImmAsmOperand<string name, string diag> : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = diag;
}

def Imm8AsmOperand      : UG24ImmAsmOperand<"Imm8",      "InvalidImm8">;
def UImm8AsmOperand     : UG24ImmAsmOperand<"UImm8",     "InvalidUImm8">;
def UImm7AsmOperand     : UG24ImmAsmOperand<"UImm7",     "InvalidUImm7">;
def UImm4AsmOperand     : UG24ImmAsmOperand<"UImm4",     "InvalidUImm4">;
def IncImmAsmOperand    : UG24ImmAsmOperand<"IncImm",    "InvalidIncImm">;
def ShiftImmAsmOperand  : UG24ImmAsmOperand<"ShiftImm",  "InvalidShiftImm">;

// Unsigned 8-bit immediate, as carried by every Format I instruction.
def imm8 : Operand<i8>, ImmLeaf<i8, [{ return isUInt<8>(Imm & 0xff); }]> {
  let PrintMethod = "printImm8";
  let ParserMatchClass = Imm8AsmOperand;
}

// INC/DEC encode "amount - 1" in four bits, so the assembly-level range is
// 1..16.
def inc_imm : Operand<i8>, ImmLeaf<i8, [{
  return Imm >= 1 && Imm <= 16;
}]> {
  let EncoderMethod = "getImmMinus1OpValue";
  let DecoderMethod = "decodeImmPlus1Operand<4>";
  let PrintMethod = "printImmOperand";
  let ParserMatchClass = IncImmAsmOperand;
}

// Shifts encode "amount - 1" in three bits: assembly range 1..8.
def shift_imm : Operand<i8>, ImmLeaf<i8, [{
  return Imm >= 1 && Imm <= 8;
}]> {
  let EncoderMethod = "getImmMinus1OpValue";
  let DecoderMethod = "decodeImmPlus1Operand<3>";
  let PrintMethod = "printImmOperand";
  let ParserMatchClass = ShiftImmAsmOperand;
}

// PSW bit index for CLRF / INVF.
def flag_imm : Operand<i8>, ImmLeaf<i8, [{ return isUInt<4>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let ParserMatchClass = UImm4AsmOperand;
}

// 7-bit immediate for the register-indirect jumps.
def imm7 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<7>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let ParserMatchClass = UImm7AsmOperand;
}

// PC-relative branch target.  The encoded field is a signed count of
// instruction words relative to the instruction that follows the branch.
def brtarget : Operand<OtherVT> {
  let EncoderMethod = "getBranchTargetOpValue";
  let DecoderMethod = "decodeBranchTarget";
  let OperandType = "OPERAND_PCREL";
  let PrintMethod = "printBranchTarget";
}

// 16-bit absolute call/jump target.
def calltarget : Operand<i16> {
  let EncoderMethod = "getCallTargetOpValue";
  let PrintMethod = "printCallTarget";
}

// The 8-bit displacement of an LD/ST, printed as "[imm]".
def memdisp : Operand<i8> {
  let PrintMethod = "printMemDisp";
  let ParserMatchClass = UImm8AsmOperand;
}

// Addressing modes.  A frame slot is reached through the SP-relative pseudo;
// anything else goes through DPTR0 with an 8-bit displacement.
def addr_fi  : ComplexPattern<i16, 2, "SelectAddrFI",  [frameindex], []>;
def addr_reg : ComplexPattern<i16, 2, "SelectAddrReg", [], []>;

// Compound operands pairing a base with its displacement, so a ComplexPattern
// result can be handed to an instruction as a single operand.
def memfi : Operand<i16> {
  let MIOperandInfo = (ops i16imm, i16imm);
  let PrintMethod = "printFrameSlot";
}

def memri : Operand<i16> {
  let MIOperandInfo = (ops GPR16, memdisp);
  let PrintMethod = "printMemRI";
}

//===----------------------------------------------------------------------===//
// Format I - immediate data transfer, arithmetic and logic
//   func 000 = LD, 001 = MVI, 010 = ST, 011 = ANDI,
//        100 = ORI, 101 = XORI, 110 = ADI, 111 = SBI
//===----------------------------------------------------------------------===//

// LD Rd, i8 : Rd <- [DPTR + i8], DPTR selected by PSW.DP
let mayLoad = 1, Uses = [DPTR0] in
def LD : FI_RI<0b000, (outs GPR:$rd), (ins memdisp:$imm),
               "ld\t$rd, $imm", []>;

// MVI Rd, i8 : Rd <- i8
let isMoveImm = 1, isReMaterializable = 1, isAsCheapAsAMove = 1 in
def MVI : FI_RI<0b001, (outs GPR:$rd), (ins imm8:$imm),
                "mvi\t$rd, $imm",
                [(set GPR:$rd, imm:$imm)]>;

// ST Rs, i8 : Rs -> [DPTR + i8]
let mayStore = 1, Uses = [DPTR0] in
def ST : FI_ST<0b010, (outs), (ins GPR:$rs, memdisp:$imm),
               "st\t$rs, $imm", []>;

let Constraints = "$src = $rd", Defs = [PSW] in {
def ANDI : FI_RI<0b011, (outs GPR:$rd), (ins GPR:$src, imm8:$imm),
                 "andi\t$rd, $imm",
                 [(set GPR:$rd, (and GPR:$src, imm:$imm))]>;

def ORI  : FI_RI<0b100, (outs GPR:$rd), (ins GPR:$src, imm8:$imm),
                 "ori\t$rd, $imm",
                 [(set GPR:$rd, (or GPR:$src, imm:$imm))]>;

def XORI : FI_RI<0b101, (outs GPR:$rd), (ins GPR:$src, imm8:$imm),
                 "xori\t$rd, $imm",
                 [(set GPR:$rd, (xor GPR:$src, imm:$imm))]>;

def ADI  : FI_RI<0b110, (outs GPR:$rd), (ins GPR:$src, imm8:$imm),
                 "adi\t$rd, $imm",
                 [(set GPR:$rd, (add GPR:$src, imm:$imm))]>;

def SBI  : FI_RI<0b111, (outs GPR:$rd), (ins GPR:$src, imm8:$imm),
                 "sbi\t$rd, $imm",
                 [(set GPR:$rd, (sub GPR:$src, imm:$imm))]>;
}

//===----------------------------------------------------------------------===//
// Format A - arithmetic, Inst{3-2} = 10
//   func 0000 = ADD, 0001 = ADC, 0100 = SUB, 0101 = SBB,
//        1000 = INC, 1001 = DEC
//===----------------------------------------------------------------------===//

let Constraints = "$src = $rd", Defs = [PSW] in {
def ADD : FALU_RR<0b10, 0b0000, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "add\t$rd, $rs",
                  [(set GPR:$rd, (add GPR:$src, GPR:$rs))]>;

def SUB : FALU_RR<0b10, 0b0100, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "sub\t$rd, $rs",
                  [(set GPR:$rd, (sub GPR:$src, GPR:$rs))]>;

// Carry-propagating forms, used to synthesise 16-bit arithmetic.
let Uses = [PSW] in {
def ADC : FALU_RR<0b10, 0b0001, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "adc\t$rd, $rs", []>;

def SBB : FALU_RR<0b10, 0b0101, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "sbb\t$rd, $rs", []>;
}
}

// INC/DEC do not touch the flags, so they are cheaper than ADI/SBI when the
// amount fits.  They encode (amount - 1).
let Constraints = "$src = $rd" in {
def INC : FALU_RI4<0b10, 0b1000, (outs GPR:$rd), (ins GPR:$src, inc_imm:$imm),
                   "inc\t$rd, $imm", []>;

def DEC : FALU_RI4<0b10, 0b1001, (outs GPR:$rd), (ins GPR:$src, inc_imm:$imm),
                   "dec\t$rd, $imm", []>;
}

//===----------------------------------------------------------------------===//
// Format L - logical and shifts, Inst{3-2} = 11
//   func 0000 = AND, 0010 = OR, 0100 = XOR, 0110 = NOT,
//        1000 = LSL, 1001 = LSR, 1010 = RSL, 1011 = RSR, 1100 = ASR,
//        1110 = CLRF, 1111 = INVF
//===----------------------------------------------------------------------===//

let Constraints = "$src = $rd", Defs = [PSW] in {
def AND : FALU_RR<0b11, 0b0000, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "and\t$rd, $rs",
                  [(set GPR:$rd, (and GPR:$src, GPR:$rs))]>;

def OR  : FALU_RR<0b11, 0b0010, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "or\t$rd, $rs",
                  [(set GPR:$rd, (or GPR:$src, GPR:$rs))]>;

def XOR : FALU_RR<0b11, 0b0100, (outs GPR:$rd), (ins GPR:$src, GPR:$rs),
                  "xor\t$rd, $rs",
                  [(set GPR:$rd, (xor GPR:$src, GPR:$rs))]>;

def NOT : FALU_R<0b11, 0b0110, (outs GPR:$rd), (ins GPR:$src),
                 "not\t$rd",
                 [(set GPR:$rd, (not GPR:$src))]>;

// Shift amounts are encoded as (amount - 1); a shift by zero has no encoding
// and is folded away before instruction selection.
def LSL : FALU_RI3<0b11, 0b1000, (outs GPR:$rd), (ins GPR:$src, shift_imm:$imm),
                   "lsl\t$rd, $imm",
                   [(set GPR:$rd, (shl GPR:$src, shift_imm:$imm))]>;

def LSR : FALU_RI3<0b11, 0b1001, (outs GPR:$rd), (ins GPR:$src, shift_imm:$imm),
                   "lsr\t$rd, $imm",
                   [(set GPR:$rd, (srl GPR:$src, shift_imm:$imm))]>;

def ASR : FALU_RI3<0b11, 0b1100, (outs GPR:$rd), (ins GPR:$src, shift_imm:$imm),
                   "asr\t$rd, $imm",
                   [(set GPR:$rd, (sra GPR:$src, shift_imm:$imm))]>;

def RSL : FALU_RI3<0b11, 0b1010, (outs GPR:$rd), (ins GPR:$src, shift_imm:$imm),
                   "rsl\t$rd, $imm",
                   [(set GPR:$rd, (rotl GPR:$src, shift_imm:$imm))]>;

def RSR : FALU_RI3<0b11, 0b1011, (outs GPR:$rd), (ins GPR:$src, shift_imm:$imm),
                   "rsr\t$rd, $imm",
                   [(set GPR:$rd, (rotr GPR:$src, shift_imm:$imm))]>;
}

let Defs = [PSW] in {
def CLRF : FALU_Flag<0b1110, (outs), (ins flag_imm:$imm), "clrf\t$imm", []>;
def INVF : FALU_Flag<0b1111, (outs), (ins flag_imm:$imm), "invf\t$imm", []>;
}

//===----------------------------------------------------------------------===//
// Format P - two implied operands, Inst{3-0} = 0000
//   func 0010 = SWAP, 0011 = SWAP Xs/SFR, 0100 = MUL, 0101 = DIV,
//        0110 = CMP, 1000 = JA, 1001 = LJA
//===----------------------------------------------------------------------===//

def SWAP : FP_RR<0b0010, (outs), (ins GPR:$rs1, GPR:$rs2),
                 "swap\t$rs1, $rs2", []>;

// MUL writes the 16-bit product to W; DIV writes quotient to W.lo and
// remainder to W.hi.  Neither result is an encoded operand.
// Both are optional blocks in the SoC configuration, so each carries the
// predicate for its feature: -mattr=-mul makes "mul" an unrecognised
// instruction in assembly as well as unavailable to the selector.
let Defs = [W, PSW] in {
let Predicates = [HasMul] in
def MUL : FP_RR<0b0100, (outs), (ins GPR:$rs1, GPR:$rs2),
                "mul\t$rs1, $rs2", []>;

let Predicates = [HasDiv] in
def DIV : FP_RR<0b0101, (outs), (ins GPR:$rs1, GPR:$rs2),
                "div\t$rs1, $rs2", []>;
}

let Defs = [PSW], isCompare = 1 in
def CMP : FP_RR<0b0110, (outs), (ins GPR:$rs1, GPR:$rs2),
                "cmp\t$rs1, $rs2",
                [(UG24cmp GPR:$rs1, GPR:$rs2)]>;

// Machine control.  These are fully-specified 16-bit opcodes.
def NOP  : FP_Ctrl<0b00000000, (outs), (ins), "nop",  []> {
  let hasSideEffects = 0;
}
let isReturn = 1, isTerminator = 1, isBarrier = 1, Uses = [RA] in
def RET  : FP_Ctrl<0b00000001, (outs), (ins), "ret",  [(UG24retglue)]>;
def FNCB : FP_Ctrl<0b00000010, (outs), (ins), "fncb", []>;
def FNCA : FP_Ctrl<0b00000011, (outs), (ins), "fnca", []>;
def WFI  : FP_Ctrl<0b10000000, (outs), (ins), "wfi",  []>;

// SWAP Xs, RA / SWAP Xs, SP
def SWAPXRA : FP_XSfr<0b0011, 0b01, (outs), (ins XReg:$xs),
                      "swap\t$xs, ra", []>;
def SWAPXSP : FP_XSfr<0b0011, 0b11, (outs), (ins XReg:$xs),
                      "swap\t$xs, sp", []>;

// 2-fetch absolute jumps.
let isBranch = 1, isTerminator = 1, isBarrier = 1 in
def JA : FP_Abs<0b1000, (outs), (ins calltarget:$addr), "ja\t$addr", []>;

let isCall = 1, Defs = [RA, PSW], Uses = [SP] in
def LJA : FP_Abs<0b1001, (outs), (ins calltarget:$addr), "lja\t$addr", []>;

//===----------------------------------------------------------------------===//
// Format S - stack and register transfers, Inst{3-0} = 0100
//   func 0000 = MOV Rd/Rs, 0001 = MOV Xd,SFR, 0010 = MOV SFR,Xs,
//        1000 = PUSH Rs, 1001 = PUSH SFR, 1010 = POP Rd, 1011 = POP SFR
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, isMoveReg = 1 in
def MOV : FS_RR<0b0000, (outs GPR:$dst), (ins GPR:$src), "mov\t$dst, $src", []>;

let mayStore = 1, Defs = [SP], Uses = [SP] in {
def PUSH : FS_RR<0b1000, (outs), (ins GPR:$src), "push\t$src", []> {
  let dst = 0b0000;
}
def PUSHPC  : FS_PushSfr<0b00, (outs), (ins), "push\tpc",  []>;
def PUSHRA  : FS_PushSfr<0b01, (outs), (ins), "push\tra",  []>;
def PUSHPSW : FS_PushSfr<0b10, (outs), (ins), "push\tpsw", []>;
}

let mayLoad = 1, Defs = [SP], Uses = [SP] in {
def POP : FS_RR<0b1010, (outs GPR:$dst), (ins), "pop\t$dst", []> {
  let src = 0b0000;
}
def POPPC  : FS_PopSfr<0b00, (outs), (ins), "pop\tpc",  []>;
def POPRA  : FS_PopSfr<0b01, (outs), (ins), "pop\tra",  []>;
def POPPSW : FS_PopSfr<0b10, (outs), (ins), "pop\tpsw", []>;
}

// MOV Xd, SFR
let hasSideEffects = 0 in {
def MOVXPC  : FS_XFromSfr<0b00, (outs XReg:$xd), (ins), "mov\t$xd, pc",  []>;
def MOVXRA  : FS_XFromSfr<0b01, (outs XReg:$xd), (ins), "mov\t$xd, ra",  []>;
def MOVXPSW : FS_XFromSfr<0b10, (outs XReg:$xd), (ins), "mov\t$xd, psw", []>;
def MOVXSP  : FS_XFromSfr<0b11, (outs XReg:$xd), (ins), "mov\t$xd, sp",  []>;

// MOV SFR, Xs - only RA and SP are writable this way.
def MOVRAX : FS_SfrFromX<0b01, (outs), (ins XReg:$xs), "mov\tra, $xs", []>;
def MOVSPX : FS_SfrFromX<0b11, (outs), (ins XReg:$xs), "mov\tsp, $xs", []>;
}

//===----------------------------------------------------------------------===//
// Format B - PC-relative control transfer, Inst{1-0} = 10
//   func 0000 = BEQ, 0001 = BNE, 0010 = BLT, 0011 = BLE, 0100 = BGT,
//        0101 = BGE, 0110 = BZ,  0111 = BNZ, 1000 = BC,  1001 = BNC,
//        1010 = BPS, 1011 = BNS, 1100 = JR,  1101 = LJR,
//        1110 = JI,  1111 = LJI
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1, Uses = [PSW] in {
class CondBranch<bits<4> func, string mnemonic>
    : FB_Rel<func, (outs), (ins brtarget:$target), !strconcat(mnemonic, "\t$target"), []>;

def BEQ : CondBranch<0b0000, "beq">;
def BNE : CondBranch<0b0001, "bne">;
def BLT : CondBranch<0b0010, "blt">;
def BLE : CondBranch<0b0011, "ble">;
def BGT : CondBranch<0b0100, "bgt">;
def BGE : CondBranch<0b0101, "bge">;
def BZ  : CondBranch<0b0110, "bz">;
def BNZ : CondBranch<0b0111, "bnz">;
def BC  : CondBranch<0b1000, "bc">;
def BNC : CondBranch<0b1001, "bnc">;
def BPS : CondBranch<0b1010, "bps">;
def BNS : CondBranch<0b1011, "bns">;
}

let isBranch = 1, isTerminator = 1, isBarrier = 1 in
def JR : FB_Rel<0b1100, (outs), (ins brtarget:$target), "jr\t$target",
                [(br bb:$target)]>;

let isCall = 1, Defs = [RA, PSW], Uses = [SP] in
def LJR : FB_Rel<0b1101, (outs), (ins brtarget:$target), "ljr\t$target", []>;

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in
def JI : FB_Ind<0b1110, (outs), (ins XReg:$xs, imm7:$imm),
                "ji\t$xs, $imm", []>;

let isCall = 1, Defs = [RA, PSW], Uses = [SP] in
def LJI : FB_Ind<0b1111, (outs), (ins XReg:$xs, imm7:$imm),
                 "lji\t$xs, $imm", []>;

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

let isPseudo = 1, isCodeGenOnly = 1, Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN : UG24Inst16<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                                  "#ADJCALLSTACKDOWN $amt1, $amt2",
                                  [(UG24callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : UG24Inst16<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                                  "#ADJCALLSTACKUP $amt1, $amt2",
                                  [(UG24callseq_end timm:$amt1, timm:$amt2)]>;
}

// Frame-index addressed load/store.  Expanded by eliminateFrameIndex into a
// DPTR0 setup sequence plus a real LD/ST.
let isPseudo = 1, isCodeGenOnly = 1 in {
let mayLoad = 1, Defs = [DPTR0, PSW] in
def LDFI : UG24Inst16<(outs GPR:$rd), (ins memfi:$addr),
                      "#LDFI $rd, $addr", []>;

let mayStore = 1, Defs = [DPTR0, PSW] in
def STFI : UG24Inst16<(outs), (ins GPR:$rs, memfi:$addr),
                      "#STFI $rs, $addr", []>;

// Materialise the address of a frame slot into a 16-bit register.
let Defs = [DPTR0, PSW] in
def LEAFI : UG24Inst16<(outs GPR16:$rd), (ins memfi:$addr),
                       "#LEAFI $rd, $addr", []>;
}

// 16-bit pseudo operations, expanded to byte-wise sequences after register
// allocation by UG24ExpandPseudo.
let isPseudo = 1, isCodeGenOnly = 1, Defs = [PSW] in {
def MVI16 : UG24Inst16<(outs GPR16:$rd), (ins i16imm:$imm),
                       "#MVI16 $rd, $imm",
                       [(set GPR16:$rd, imm:$imm)]>;

let Constraints = "$src = $rd" in {
def ADD16 : UG24Inst16<(outs GPR16:$rd), (ins GPR16:$src, GPR16:$rs),
                       "#ADD16 $rd, $rs",
                       [(set GPR16:$rd, (add GPR16:$src, GPR16:$rs))]>;
def SUB16 : UG24Inst16<(outs GPR16:$rd), (ins GPR16:$src, GPR16:$rs),
                       "#SUB16 $rd, $rs",
                       [(set GPR16:$rd, (sub GPR16:$src, GPR16:$rs))]>;
def AND16 : UG24Inst16<(outs GPR16:$rd), (ins GPR16:$src, GPR16:$rs),
                       "#AND16 $rd, $rs",
                       [(set GPR16:$rd, (and GPR16:$src, GPR16:$rs))]>;
def OR16  : UG24Inst16<(outs GPR16:$rd), (ins GPR16:$src, GPR16:$rs),
                       "#OR16 $rd, $rs",
                       [(set GPR16:$rd, (or GPR16:$src, GPR16:$rs))]>;
def XOR16 : UG24Inst16<(outs GPR16:$rd), (ins GPR16:$src, GPR16:$rs),
                       "#XOR16 $rd, $rs",
                       [(set GPR16:$rd, (xor GPR16:$src, GPR16:$rs))]>;
}
}

// The hardware MUL and DIV write their result to W implicitly rather than to
// an encoded destination, so they are selected as pseudos carrying an
// ordinary destination operand and split after register allocation.
let isPseudo = 1, isCodeGenOnly = 1, Defs = [W, PSW] in {
let Predicates = [HasMul] in
def MULW : UG24Inst16<(outs GPR16:$dst), (ins GPR:$a, GPR:$b),
                      "#MULW $dst, $a, $b",
                      [(set GPR16:$dst, (UG24mulw GPR:$a, GPR:$b))]>;

// DIV leaves the quotient in the low half of W and the remainder in the high
// half, which is exactly the layout of the pair.
def DIVW : UG24Inst16<(outs GPR16:$dst), (ins GPR:$a, GPR:$b),
                      "#DIVW $dst, $a, $b", []>;
}

// Copy between 16-bit registers; expanded to two MOVs.
let isPseudo = 1, isCodeGenOnly = 1, hasSideEffects = 0, isMoveReg = 1 in
def MOV16 : UG24Inst16<(outs GPR16:$dst), (ins GPR16:$src),
                       "#MOV16 $dst, $src", []>;

// Call a symbol.  Selected before we know whether the target is in range of
// the relative form; always lowered to LJA.
let isCall = 1, isPseudo = 1, isCodeGenOnly = 1,
    Defs = [RA, PSW], Uses = [SP] in
def CALL : UG24Inst16<(outs), (ins calltarget:$addr), "#CALL $addr",
                      [(UG24call tglobaladdr:$addr)]>;

// Call through a function pointer.  The hardware form is LJI, whose target is
// one of the three extended pairs; the pointer can be in any pair, so the
// pseudo takes GPR16 and the expansion parks it in the reserved DPTR0.  Doing
// it that way keeps W and DPTR1 free for arguments, which matters because a
// call through a pointer usually has both of them already occupied.
let isCall = 1, isPseudo = 1, isCodeGenOnly = 1,
    Defs = [RA, PSW, DPTR0, R11], Uses = [SP], Size = 16 in
def CALLi : UG24Inst16<(outs), (ins GPR16:$target), "#CALLi $target",
                       [(UG24call GPR16:$target)]>;

// Conditional branch on the flags set by a preceding CMP.  $cc is a
// UG24CC::CondCode; expanded to the matching Bxx by UG24ExpandPseudo.
let isBranch = 1, isTerminator = 1, isPseudo = 1, isCodeGenOnly = 1,
    Uses = [PSW] in
def BRCC : UG24Inst16<(outs), (ins brtarget:$target, i8imm:$cc),
                      "#BRCC $target, $cc",
                      [(UG24brcc bb:$target, imm:$cc)]>;

// Conditional select.  There is no conditional move on uG24, so these are
// turned into a diamond of basic blocks by EmitInstrWithCustomInserter.
let usesCustomInserter = 1, isPseudo = 1, isCodeGenOnly = 1, Uses = [PSW] in {
def Select8 : UG24Inst16<(outs GPR:$dst), (ins GPR:$t, GPR:$f, i8imm:$cc),
                         "#Select8 $dst, $t, $f, $cc",
                         [(set GPR:$dst, (UG24selectcc GPR:$t, GPR:$f,
                                                       imm:$cc))]>;
def Select16 : UG24Inst16<(outs GPR16:$dst), (ins GPR16:$t, GPR16:$f,
                                                  i8imm:$cc),
                          "#Select16 $dst, $t, $f, $cc",
                          [(set GPR16:$dst, (UG24selectcc GPR16:$t, GPR16:$f,
                                                          imm:$cc))]>;

// A full 16-bit comparison: compare the high bytes, then the low bytes when
// the high bytes are equal, and leave 0 or 1 in the destination byte.
let Defs = [PSW] in
def SetCC16 : UG24Inst16<(outs GPR:$dst), (ins GPR16:$lhs, GPR16:$rhs,
                                               i8imm:$cc),
                         "#SetCC16 $dst, $lhs, $rhs, $cc",
                         [(set GPR:$dst, (UG24setcc16 GPR16:$lhs, GPR16:$rhs,
                                                      imm:$cc))]>;

let Defs = [PSW], isBranch = 1, isTerminator = 1 in
def BrCC16 : UG24Inst16<(outs), (ins brtarget:$target, GPR16:$lhs,
                                     GPR16:$rhs, i8imm:$cc),
                        "#BrCC16 $target, $lhs, $rhs, $cc",
                        [(UG24brcc16 bb:$target, GPR16:$lhs, GPR16:$rhs,
                                     imm:$cc)]>;
}

//===----------------------------------------------------------------------===//
// Memory access pseudos
//===----------------------------------------------------------------------===//
//
// LD and ST take their base address from DPTR0, so a load through an
// arbitrary pointer is "copy the pointer into DPTR0, then LD".  These pseudos
// keep the pair together until UG24ExpandPseudo splits them, which stops the
// register allocator from inserting anything in between.

let isPseudo = 1, isCodeGenOnly = 1, Defs = [DPTR0] in {
let mayLoad = 1 in {
def LOADp   : UG24Inst16<(outs GPR:$rd), (ins memri:$addr),
                         "#LOADp $rd, $addr", []>;
def LOAD16p : UG24Inst16<(outs GPR16:$rd), (ins memri:$addr),
                         "#LOAD16p $rd, $addr", []>;
}
let mayStore = 1 in {
def STOREp   : UG24Inst16<(outs), (ins GPR:$rs, memri:$addr),
                          "#STOREp $rs, $addr", []>;
def STORE16p : UG24Inst16<(outs), (ins GPR16:$rs, memri:$addr),
                          "#STORE16p $rs, $addr", []>;
}
}

// The 16-bit frame accesses are two byte accesses; LDFI/STFI above cover the
// 8-bit case.
let isPseudo = 1, isCodeGenOnly = 1, Defs = [DPTR0, PSW] in {
let mayLoad = 1 in
def LDFI16 : UG24Inst16<(outs GPR16:$rd), (ins memfi:$addr),
                        "#LDFI16 $rd, $addr", []>;
let mayStore = 1 in
def STFI16 : UG24Inst16<(outs), (ins GPR16:$rs, memfi:$addr),
                        "#STFI16 $rs, $addr", []>;
}

//===----------------------------------------------------------------------===//
// Non-instruction patterns
//===----------------------------------------------------------------------===//

// Calls to an external symbol use the same encoding as calls to a global.
def : Pat<(UG24call (i16 texternalsym:$addr)), (CALL texternalsym:$addr)>;

// Address wrappers for globals and block addresses.
def : Pat<(UG24wrapper tglobaladdr:$addr),   (MVI16 tglobaladdr:$addr)>;
def : Pat<(UG24wrapper tblockaddress:$addr), (MVI16 tblockaddress:$addr)>;
def : Pat<(UG24wrapper texternalsym:$addr),  (MVI16 texternalsym:$addr)>;

// Negation: ~x + 1.
def : Pat<(sub 0, GPR:$rs), (ADI (NOT GPR:$rs), 1)>;

// An 8-bit multiply is the low half of the hardware 8x8 -> 16 product.
let Predicates = [HasMul] in
def : Pat<(i8 (mul GPR:$a, GPR:$b)),
          (EXTRACT_SUBREG (MULW GPR:$a, GPR:$b), sub_lo)>;

// Unsigned 8-bit divide and remainder come out of one DIV.
let Predicates = [HasDiv] in {
def : Pat<(i8 (udiv GPR:$a, GPR:$b)),
          (EXTRACT_SUBREG (DIVW GPR:$a, GPR:$b), sub_lo)>;
def : Pat<(i8 (urem GPR:$a, GPR:$b)),
          (EXTRACT_SUBREG (DIVW GPR:$a, GPR:$b), sub_hi)>;
}

//===----------------------------------------------------------------------===//
// Width conversions
//===----------------------------------------------------------------------===//

// Truncation just names the low half of the pair.
def : Pat<(i8 (trunc GPR16:$src)), (EXTRACT_SUBREG GPR16:$src, sub_lo)>;

def : Pat<(UG24lo8 GPR16:$src), (EXTRACT_SUBREG GPR16:$src, sub_lo)>;
def : Pat<(UG24hi8 GPR16:$src), (EXTRACT_SUBREG GPR16:$src, sub_hi)>;
// Building a 16-bit value out of two bytes uses nested INSERT_SUBREGs rather
// than REG_SEQUENCE: the latter requires every half to already be a register,
// which is not true when one of them is a constant.

// An any-extend leaves the high byte undefined.
def : Pat<(i16 (anyext GPR:$src)),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GPR:$src, sub_lo)>;

// Zero-extend clears the high byte.
def : Pat<(i16 (zext GPR:$src)),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GPR:$src, sub_lo),
             (MVI 0), sub_hi)>;

// Sign-extend replicates the sign bit: an arithmetic shift right by seven
// leaves 0x00 or 0xff in every bit of the high byte.
def : Pat<(i16 (sext GPR:$src)),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GPR:$src, sub_lo),
             (ASR GPR:$src, 7), sub_hi)>;

// Explicit half naming and recombination, used by the 16-bit shift expansion.
def : Pat<(UG24pair GPR:$lo, GPR:$hi),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GPR:$lo, sub_lo),
             GPR:$hi, sub_hi)>;
def : Pat<(UG24pair GPR:$lo, (i8 imm:$hi)),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GPR:$lo, sub_lo),
             (MVI imm:$hi), sub_hi)>;
def : Pat<(UG24pair (i8 imm:$lo), GPR:$hi),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), (MVI imm:$lo), sub_lo),
             GPR:$hi, sub_hi)>;
def : Pat<(UG24pair (i8 imm:$lo), (i8 imm:$hi)),
          (INSERT_SUBREG
             (INSERT_SUBREG (i16 (IMPLICIT_DEF)), (MVI imm:$lo), sub_lo),
             (MVI imm:$hi), sub_hi)>;

//===----------------------------------------------------------------------===//
// Loads and stores
//===----------------------------------------------------------------------===//

// Frame slots.
def : Pat<(i8  (load  addr_fi:$addr)), (LDFI   addr_fi:$addr)>;
def : Pat<(i16 (load  addr_fi:$addr)), (LDFI16 addr_fi:$addr)>;
def : Pat<(store GPR:$rs,   addr_fi:$addr), (STFI   GPR:$rs,   addr_fi:$addr)>;
def : Pat<(store GPR16:$rs, addr_fi:$addr), (STFI16 GPR16:$rs, addr_fi:$addr)>;

def : Pat<(i8 (extloadi8  addr_fi:$addr)), (LDFI addr_fi:$addr)>;
def : Pat<(i8 (zextloadi8 addr_fi:$addr)), (LDFI addr_fi:$addr)>;
def : Pat<(i8 (sextloadi8 addr_fi:$addr)), (LDFI addr_fi:$addr)>;

// Arbitrary pointers.
def : Pat<(i8  (load  addr_reg:$addr)), (LOADp   addr_reg:$addr)>;
def : Pat<(i16 (load  addr_reg:$addr)), (LOAD16p addr_reg:$addr)>;
def : Pat<(store GPR:$rs,   addr_reg:$addr), (STOREp   GPR:$rs,   addr_reg:$addr)>;
def : Pat<(store GPR16:$rs, addr_reg:$addr), (STORE16p GPR16:$rs, addr_reg:$addr)>;

def : Pat<(i8 (extloadi8  addr_reg:$addr)), (LOADp addr_reg:$addr)>;
def : Pat<(i8 (zextloadi8 addr_reg:$addr)), (LOADp addr_reg:$addr)>;
def : Pat<(i8 (sextloadi8 addr_reg:$addr)), (LOADp addr_reg:$addr)>;


