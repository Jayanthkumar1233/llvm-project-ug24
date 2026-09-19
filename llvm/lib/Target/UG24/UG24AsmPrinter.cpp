//===-- UG24AsmPrinter.cpp - UG24 LLVM Assembly Printer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "UG24MCInstLower.h"
#include "UG24TargetMachine.h"
#include "TargetInfo/UG24TargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class UG24AsmPrinter : public AsmPrinter {
public:
  explicit UG24AsmPrinter(TargetMachine &TM,
                          std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "UG24 Assembly Printer"; }

  bool doInitialization(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;
  void emitInstruction(const MachineInstr *MI) override;

private:
  /// Set when this module passes a floating-point value as a variadic
  /// argument -- which is the only way printf can be asked to format one.
  bool NeedsFloatFormatter = false;
};

/// True when any call in \p M passes a floating-point value in the variadic
/// part of an argument list.
static bool passesFloatToVarargs(const Module &M) {
  for (const Function &F : M) {
    for (const BasicBlock &BB : F) {
      for (const Instruction &I : BB) {
        const auto *CB = dyn_cast<CallBase>(&I);
        if (!CB || !CB->getFunctionType()->isVarArg())
          continue;
        for (unsigned Arg = CB->getFunctionType()->getNumParams(),
                      End = CB->arg_size();
             Arg != End; ++Arg)
          if (CB->getArgOperand(Arg)->getType()->isFloatingPointTy())
            return true;
      }
    }
  }
  return false;
}
} // namespace

// printf's %f conversion is implemented in ug24_float.c, alongside the
// soft-float arithmetic it needs, and ug24_stdio.c declares it weak and does
// not define it.  That keeps a program with no floating point in it from
// linking either: "Hello ug24" is 300 bytes, where the formatter and the
// library behind it are about 25 KB on a 64 KB part.
//
// An undefined weak symbol does not make the linker pull a member out of
// libug24.a, so the rule that follows is "a program gets the formatter when
// it already needed soft float".  That rule is wrong for exactly one program:
// one whose floating-point arithmetic the optimiser folded away, leaving a
// constant to print and no call to __addsf3 to drag the member in.
//
//     printf("%f\n", 879 * 9 / 50.0 + 52);      // folds to one constant
//
// Nothing downstream can tell that program from one that never had a float.
// This can: at this point the IR still shows the call, and a floating-point
// value in the variadic part of an argument list is the one thing that makes
// %f meaningful.  Emitting an undefined *global* reference to the formatter
// is what -Wl,-u does, decided per translation unit instead of per link.
bool UG24AsmPrinter::doInitialization(Module &M) {
  NeedsFloatFormatter = passesFloatToVarargs(M);
  return AsmPrinter::doInitialization(M);
}

void UG24AsmPrinter::emitEndOfAsmFile(Module &M) {
  if (!NeedsFloatFormatter)
    return;
  MCSymbol *Formatter = OutContext.getOrCreateSymbol("__ug24_format_float");
  OutStreamer->emitSymbolAttribute(Formatter, MCSA_Global);
}

void UG24AsmPrinter::emitInstruction(const MachineInstr *MI) {
  UG24MCInstLower MCInstLowering(OutContext, *this);
  MCInst LoweredInst;
  MCInstLowering.Lower(MI, LoweredInst);
  EmitToStreamer(*OutStreamer, LoweredInst);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24AsmPrinter() {
  RegisterAsmPrinter<UG24AsmPrinter> X(getTheUG24Target());
}
