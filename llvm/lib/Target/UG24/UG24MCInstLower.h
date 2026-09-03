//===-- UG24MCInstLower.h - Lower MachineInstr to MCInst -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24MCINSTLOWER_H
#define LLVM_LIB_TARGET_UG24_UG24MCINSTLOWER_H

#include "llvm/Support/Compiler.h"

namespace llvm {

class AsmPrinter;
class MCContext;
class MCInst;
class MachineInstr;
class MachineOperand;
class MCOperand;
class MCSymbol;

class LLVM_LIBRARY_VISIBILITY UG24MCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

public:
  UG24MCInstLower(MCContext &C, AsmPrinter &AP) : Ctx(C), Printer(AP) {}

  void Lower(const MachineInstr *MI, MCInst &OutMI) const;

  MCOperand lowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24MCINSTLOWER_H
