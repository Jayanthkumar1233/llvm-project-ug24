//===-- UG24InstrFormats.td - UG24 Instruction Formats ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Encoding formats for the uG24 instruction set, transcribed from
// "Copy of uG24xx1616uP_ISA.xlsx" (sheet uG24081616uP_ISA).
//
// Instructions are 16 bits wide, except JA/LJA which take a second fetch
// carrying a 16-bit absolute address.  The two low bits select the major
// encoding class:
//
//   Inst{0}   == 1   -> Format I  (8-bit immediate ALU / data transfer)
//   Inst{1-0} == 10  -> Format B  (PC-relative branches and jumps)
//   Inst{3-0} == 0000-> Format P  (2-operand implied: MUL/DIV/CMP/SWAP/JA/LJA)
//   Inst{3-0} == 0100-> Format S  (stack and register moves)
//   Inst{3-2} == 10  -> Format A  (register/immediate arithmetic)
//   Inst{3-2} == 11  -> Format L  (register/immediate logical)
//
//===----------------------------------------------------------------------===//

// Base class for all 16-bit uG24 instructions.
class UG24Inst16<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<16> Inst;
  field bits<16> SoftFail = 0;

  let Namespace = "UG24";
  let Size = 2;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// Base class for the 4-byte (2-fetch) absolute jump instructions.  The low
// halfword holds the opcode, the high halfword the absolute target address.
class UG24Inst32<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<32> Inst;
  field bits<32> SoftFail = 0;

  let Namespace = "UG24";
  let Size = 4;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

//===----------------------------------------------------------------------===//
// Format I - 8-bit immediate, Inst{0} = 1
//===----------------------------------------------------------------------===//

// Layout used by MVI/LD/ANDI/ORI/XORI/ADI/SBI: the immediate occupies the
// high byte and the register the following nibble.
class FI_RI<bits<3> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<8> imm;
  bits<4> rd;

  let Inst{15-8} = imm;
  let Inst{7-4}  = rd;
  let Inst{3-1}  = func;
  let Inst{0}    = 0b1;
}

// Layout used by ST only: the source register occupies the high nibble and
// the immediate the middle byte.
class FI_ST<bits<3> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<4> rs;
  bits<8> imm;

  let Inst{15-12} = rs;
  let Inst{11-4}  = imm;
  let Inst{3-1}   = func;
  let Inst{0}     = 0b1;
}

//===----------------------------------------------------------------------===//
// Format B - PC-relative control transfer, Inst{1-0} = 10
//===----------------------------------------------------------------------===//

class FB_Rel<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<10> target;

  let Inst{15-6} = target;
  let Inst{5-2}  = func;
  let Inst{1-0}  = 0b10;
}

// Register-indirect jumps: PC <- {Xs, i7}
class FB_Ind<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<7> imm;
  bits<3> xs;

  let Inst{15-9} = imm;
  let Inst{8-6}  = xs;
  let Inst{5-2}  = func;
  let Inst{1-0}  = 0b10;
}

//===----------------------------------------------------------------------===//
// Format P - two implied operands, Inst{3-0} = 0000
//===----------------------------------------------------------------------===//

class FP_RR<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<4> rs1;
  bits<4> rs2;

  let Inst{15-12} = rs1;
  let Inst{11-8}  = rs2;
  let Inst{7-4}   = func;
  let Inst{3-0}   = 0b0000;
}

// Machine-control instructions: the whole opcode is fixed.
class FP_Ctrl<bits<8> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  let Inst{15-8} = op;
  let Inst{7-0}  = 0b00000000;
}

// SWAP Xs, SFR - Inst{14-12} = Xs, Inst{9-8} = SFR code.
class FP_XSfr<bits<4> func, bits<2> sfr, dag outs, dag ins, string asmstr,
              list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  bits<3> xs;

  let Inst{15}    = 0b0;
  let Inst{14-12} = xs;
  let Inst{11-10} = 0b00;
  let Inst{9-8}   = sfr;
  let Inst{7-4}   = func;
  let Inst{3-0}   = 0b0000;
}

// 2-fetch absolute jumps.  Low halfword is an FP_RR opcode with rs1/rs2 = 0.
class FP_Abs<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst32<outs, ins, asmstr, pattern> {
  bits<16> addr;

  let Inst{15-8}  = 0b00000000;
  let Inst{7-4}   = func;
  let Inst{3-0}   = 0b0000;
  let Inst{31-16} = addr;
}

//===----------------------------------------------------------------------===//
// Format S - stack and register transfer, Inst{3-0} = 0100
//===----------------------------------------------------------------------===//

class FS_Base<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  let Inst{11-8} = func;
  let Inst{3-0}  = 0b0100;
}

// MOV Rd, Rs / PUSH Rs / POP Rd
class FS_RR<bits<4> func, dag outs, dag ins, string asmstr, list<dag> pattern>
    : FS_Base<func, outs, ins, asmstr, pattern> {
  bits<4> src;
  bits<4> dst;

  let Inst{15-12} = src;
  let Inst{7-4}   = dst;
}

// MOV Xd, SFR - Inst{13-12} = SFR code, Inst{6-4} = Xd
class FS_XFromSfr<bits<2> sfr, dag outs, dag ins, string asmstr,
                  list<dag> pattern>
    : FS_Base<0b0001, outs, ins, asmstr, pattern> {
  bits<3> xd;

  let Inst{15-14} = 0b00;
  let Inst{13-12} = sfr;
  let Inst{7}     = 0b0;
  let Inst{6-4}   = xd;
}

// MOV SFR, Xs - Inst{14-12} = Xs, Inst{5-4} = SFR code
class FS_SfrFromX<bits<2> sfr, dag outs, dag ins, string asmstr,
                  list<dag> pattern>
    : FS_Base<0b0010, outs, ins, asmstr, pattern> {
  bits<3> xs;

  let Inst{15}    = 0b0;
  let Inst{14-12} = xs;
  let Inst{7-6}   = 0b00;
  let Inst{5-4}   = sfr;
}

// PUSH SFR - Inst{13-12} = SFR code
class FS_PushSfr<bits<2> sfr, dag outs, dag ins, string asmstr,
                 list<dag> pattern>
    : FS_Base<0b1001, outs, ins, asmstr, pattern> {
  let Inst{15-14} = 0b00;
  let Inst{13-12} = sfr;
  let Inst{7-4}   = 0b0000;
}

// POP SFR - Inst{5-4} = SFR code
class FS_PopSfr<bits<2> sfr, dag outs, dag ins, string asmstr,
                list<dag> pattern>
    : FS_Base<0b1011, outs, ins, asmstr, pattern> {
  let Inst{15-12} = 0b0000;
  let Inst{7-6}   = 0b00;
  let Inst{5-4}   = sfr;
}

//===----------------------------------------------------------------------===//
// Formats A and L - register ALU, Inst{3-2} = 10 (arith) or 11 (logic)
//===----------------------------------------------------------------------===//

class FALU<bits<2> cls, bits<4> func, dag outs, dag ins, string asmstr,
           list<dag> pattern>
    : UG24Inst16<outs, ins, asmstr, pattern> {
  let Inst{11-8} = func;
  let Inst{3-2}  = cls;
  let Inst{1-0}  = 0b00;
}

// Rd <- Rd op Rs
class FALU_RR<bits<2> cls, bits<4> func, dag outs, dag ins, string asmstr,
              list<dag> pattern>
    : FALU<cls, func, outs, ins, asmstr, pattern> {
  bits<4> rs;
  bits<4> rd;

  let Inst{15-12} = rs;
  let Inst{7-4}   = rd;
}

// Rd <- Rd op (i4 + 1)   (INC / DEC)
class FALU_RI4<bits<2> cls, bits<4> func, dag outs, dag ins, string asmstr,
               list<dag> pattern>
    : FALU<cls, func, outs, ins, asmstr, pattern> {
  bits<4> imm;
  bits<4> rd;

  let Inst{15-12} = imm;
  let Inst{7-4}   = rd;
}

// Rd <- Rd shift (i3 + 1)
class FALU_RI3<bits<2> cls, bits<4> func, dag outs, dag ins, string asmstr,
               list<dag> pattern>
    : FALU<cls, func, outs, ins, asmstr, pattern> {
  bits<3> imm;
  bits<4> rd;

  let Inst{15}    = 0b0;
  let Inst{14-12} = imm;
  let Inst{7-4}   = rd;
}

// NOT Rd - no source operand
class FALU_R<bits<2> cls, bits<4> func, dag outs, dag ins, string asmstr,
             list<dag> pattern>
    : FALU<cls, func, outs, ins, asmstr, pattern> {
  bits<4> rd;

  let Inst{15-12} = 0b0000;
  let Inst{7-4}   = rd;
}

// CLRF / INVF - PSW bit index in the high nibble
class FALU_Flag<bits<4> func, dag outs, dag ins, string asmstr,
                list<dag> pattern>
    : FALU<0b11, func, outs, ins, asmstr, pattern> {
  bits<4> imm;

  let Inst{15-12} = imm;
  let Inst{7-4}   = 0b0000;
}
