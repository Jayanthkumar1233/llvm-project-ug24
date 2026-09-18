//===-- UG24MCTargetDesc.cpp - UG24 Target Descriptions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24MCTargetDesc.h"
#include "UG24InstPrinter.h"
#include "UG24MCAsmInfo.h"
#include "TargetInfo/UG24TargetInfo.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "UG24GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
static constexpr std::nullopt_t None = std::nullopt;
#include "UG24GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "UG24GenRegisterInfo.inc"

static MCAsmInfo *createUG24MCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  return new UG24MCAsmInfo(TT);
}

static MCInstrInfo *createUG24MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitUG24MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createUG24MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitUG24MCRegisterInfo(X, UG24::RA);
  return X;
}

static MCSubtargetInfo *createUG24MCSubtargetInfo(const Triple &TT,
                                                   StringRef CPU,
                                                   StringRef FS) {
  // An empty -mcpu means the default part, which has both optional
  // arithmetic blocks; leaving it empty would make the assembler reject "mul".
  StringRef CPUName = CPU.empty() ? StringRef("generic") : CPU;
  return createUG24MCSubtargetInfoImpl(TT, CPUName, /*TuneCPU=*/CPUName, FS);
}

static MCInstPrinter *createUG24MCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  return new UG24InstPrinter(MAI, MII, MRI);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24TargetMC() {
  Target &TheUG24Target = getTheUG24Target();

  TargetRegistry::RegisterMCAsmInfo(TheUG24Target, createUG24MCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(TheUG24Target, createUG24MCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(TheUG24Target, createUG24MCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(TheUG24Target, createUG24MCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(TheUG24Target, createUG24MCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(TheUG24Target, createUG24MCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(TheUG24Target, createUG24AsmBackend);
}
