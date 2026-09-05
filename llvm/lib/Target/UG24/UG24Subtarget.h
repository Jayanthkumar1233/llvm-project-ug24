//===-- UG24Subtarget.h - Define Subtarget for the UG24 -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24SUBTARGET_H
#define LLVM_LIB_TARGET_UG24_UG24SUBTARGET_H

#include "UG24FrameLowering.h"
#include "UG24ISelLowering.h"
#include "UG24InstrInfo.h"
#include "UG24RegisterInfo.h"
#include "UG24SelectionDAGInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "UG24GenSubtargetInfo.inc"

namespace llvm {

class UG24Subtarget : public UG24GenSubtargetInfo {
  // Declaration order is construction order.  RegInfo has to come before
  // TLInfo, because UG24TargetLowering's constructor calls
  // computeRegisterProperties(Subtarget.getRegisterInfo()) -- reading a member
  // that has not been constructed yet is undefined behaviour.
  UG24InstrInfo InstrInfo;
  UG24FrameLowering FrameLowering;
  UG24RegisterInfo RegInfo;
  UG24TargetLowering TLInfo;
  UG24SelectionDAGInfo TSInfo;

public:
  UG24Subtarget(const Triple &TT, const std::string &CPU,
                const std::string &FS, const TargetMachine &TM);

  const UG24InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const UG24FrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const UG24TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const UG24RegisterInfo *getRegisterInfo() const override {
    return &RegInfo;
  }
  // Without this the default returns nullptr, and SelectionDAGBuilder
  // dereferences it the first time a call to strlen, memcmp or friends is
  // recognised as a library function.
  const UG24SelectionDAGInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  bool enableMachineScheduler() const override { return false; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24SUBTARGET_H
