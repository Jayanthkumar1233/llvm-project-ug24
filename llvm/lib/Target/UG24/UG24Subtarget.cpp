//===-- UG24Subtarget.cpp - UG24 Subtarget Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24Subtarget.h"
#include "UG24.h"

#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_TARGET_DESC
static constexpr std::nullopt_t None = std::nullopt;
#include "UG24GenSubtargetInfo.inc"

using namespace llvm;

// -mcpu is usually absent, and an empty CPU name selects no processor at all,
// which would leave the optional blocks looking absent on a part that has
// them.  Everything treats "" as the fully-populated default part.
static StringRef selectCPU(StringRef CPU) {
  return CPU.empty() ? StringRef("generic") : CPU;
}

UG24Subtarget &UG24Subtarget::initializeSubtargetDependencies(StringRef CPU,
                                                              StringRef FS) {
  ParseSubtargetFeatures(selectCPU(CPU), /*TuneCPU=*/selectCPU(CPU), FS);
  return *this;
}

UG24Subtarget::UG24Subtarget(const Triple &TT, const std::string &CPU,
                             const std::string &FS, const TargetMachine &TM)
    : UG24GenSubtargetInfo(TT, selectCPU(CPU), /*TuneCPU=*/selectCPU(CPU), FS),
      InstrInfo(), FrameLowering(), RegInfo(),
      // The features have to be parsed before TLInfo is constructed: its
      // constructor asks whether the multiplier and divider are present.
      TLInfo(TM, initializeSubtargetDependencies(CPU, FS)), TSInfo() {}
