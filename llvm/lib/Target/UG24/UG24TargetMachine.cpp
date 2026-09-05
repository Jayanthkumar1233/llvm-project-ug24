//===-- UG24TargetMachine.cpp - UG24 TargetMachine Implementation --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24TargetMachine.h"
#include "TargetInfo/UG24TargetInfo.h"
#include "UG24.h"
#include "UG24MachineFunctionInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24Target() {
  RegisterTargetMachine<UG24TargetMachine> X(getTheUG24Target());
  auto &PR = *PassRegistry::getPassRegistry();
  initializeUG24ExpandPseudoPass(PR);
}

static std::string computeDataLayout(const Triple &TT) {
  // Little endian, ELF mangling, 16-bit byte-addressed pointers, no minimum
  // aggregate alignment, 8- and 16-bit native integer widths, 8-bit stack
  // alignment (the uG24 stack is byte granular).
  //
  // i32, i64, f32 and f64 have to be spelled out even though nothing here is
  // native at those widths.  Left unstated, LLVM assumes its own 4- and 8-byte
  // ABI alignments while Clang says one byte, and the two disagree wherever
  // alignment is observable -- most visibly in va_arg, which then rounds the
  // argument pointer up and reads a 32-bit variadic argument two bytes late.
  return "e-m:e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-n8:16-S8";
}

UG24TargetMachine::UG24TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(TT), TT, CPU, FS, Options,
                        Reloc::Static, CodeModel::Small, OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
  initAsmInfo();
}

namespace {
class UG24PassConfig : public TargetPassConfig {
public:
  UG24PassConfig(UG24TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  UG24TargetMachine &getUG24TargetMachine() const {
    return getTM<UG24TargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createUG24ISelDag(getUG24TargetMachine(), getOptLevel()));
    return false;
  }

  void addPreEmitPass() override {
    // Split the 16-bit and frame pseudos once real registers are assigned.
    addPass(createUG24ExpandPseudoPass());
    // Conditional branches reach only +/- 512 instruction words.  Relaxation
    // has to run after the pseudos are gone, because that is the first point
    // at which every instruction has its final size.  Anything out of reach
    // becomes an inverted short branch over an absolute JA.
    addPass(&BranchRelaxationPassID);
  }
};
} // namespace

TargetPassConfig *UG24TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new UG24PassConfig(*this, PM);
}

MachineFunctionInfo *UG24TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return UG24MachineFunctionInfo::create<UG24MachineFunctionInfo>(Allocator, F,
                                                                  STI);
}
