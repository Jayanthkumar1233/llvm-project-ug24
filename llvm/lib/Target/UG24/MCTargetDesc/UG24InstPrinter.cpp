//===-- UG24InstPrinter.cpp - Convert UG24 MCInst to assembly -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24InstPrinter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#define PRINT_ALIAS_INSTR
#include "UG24GenAsmWriter.inc"

void UG24InstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) const {
  OS << StringRef(getRegisterName(Reg)).lower();
}

void UG24InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &OS) {
  if (!printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}

void UG24InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    printRegName(O, Op.getReg());
  else if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "unknown operand kind");
    Op.getExpr()->print(O, &MAI);
  }
}

void UG24InstPrinter::printImmOperand(const MCInst *MI, unsigned OpNo,
                                      raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << Op.getImm();
  else
    printOperand(MI, OpNo, O);
}

// Every 8-bit immediate field is unsigned in the encoding, so a value that
// arrived here sign-extended is printed back as the byte it will become.
void UG24InstPrinter::printImm8(const MCInst *MI, unsigned OpNo,
                                raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << (Op.getImm() & 0xff);
  else
    printOperand(MI, OpNo, O);
}

// LD/ST displacements are written in brackets to make the implicit DPTR base
// visible in the listing.
void UG24InstPrinter::printMemDisp(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  O << '[';
  if (Op.isImm())
    O << (Op.getImm() & 0xff);
  else
    Op.getExpr()->print(O, &MAI);
  O << ']';
}

// The two operands below only ever appear on pseudo instructions, which are
// expanded before the assembly is emitted.  The printers exist so that -debug
// dumps of pre-expansion machine code are readable.
void UG24InstPrinter::printFrameSlot(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  O << "fi#" << MI->getOperand(OpNo).getImm() << '+'
    << MI->getOperand(OpNo + 1).getImm();
}

void UG24InstPrinter::printMemRI(const MCInst *MI, unsigned OpNo,
                                 raw_ostream &O) {
  printRegName(O, MI->getOperand(OpNo).getReg());
  O << '+' << MI->getOperand(OpNo + 1).getImm();
}

void UG24InstPrinter::printBranchTarget(const MCInst *MI, uint64_t Address,
                                        unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (!Op.isImm()) {
    printOperand(MI, OpNo, O);
    return;
  }

  // The operand carries a byte displacement from the branch itself.  Printing
  // it as a resolved address is what a disassembly listing wants; assembly
  // meant to be reassembled keeps the displacement.
  int64_t Imm = Op.getImm();
  if (PrintBranchImmAsAddress)
    O << format_hex(Address + Imm, 6);
  else
    O << (Imm < 0 ? "." : ".+") << Imm;
}

void UG24InstPrinter::printCallTarget(const MCInst *MI, unsigned OpNo,
                                      raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << format_hex(Op.getImm() & 0xffff, 6);
  else
    printOperand(MI, OpNo, O);
}
