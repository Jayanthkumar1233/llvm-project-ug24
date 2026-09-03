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
#include "llvm/MC/MCStreamer.h"
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

  void emitInstruction(const MachineInstr *MI) override;
};
} // namespace

void UG24AsmPrinter::emitInstruction(const MachineInstr *MI) {
  UG24MCInstLower MCInstLowering(OutContext, *this);
  MCInst LoweredInst;
  MCInstLowering.Lower(MI, LoweredInst);
  EmitToStreamer(*OutStreamer, LoweredInst);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24AsmPrinter() {
  RegisterAsmPrinter<UG24AsmPrinter> X(getTheUG24Target());
}
