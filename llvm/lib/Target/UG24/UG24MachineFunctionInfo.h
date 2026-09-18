//===-- UG24MachineFunctionInfo.h - UG24 per-function state ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_UG24_UG24MACHINEFUNCTIONINFO_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class UG24MachineFunctionInfo : public MachineFunctionInfo {
  /// Frame index of the first variadic argument.
  int VarArgsFrameIndex = 0;

  /// Registers an interrupt handler's prologue pushes and its epilogue pops,
  /// in push order.  Empty in an ordinary function.  Filled in by
  /// UG24FrameLowering::determineCalleeSaves, which is the last point at
  /// which register liveness is still tracked.
  SmallVector<MCRegister, 8> InterruptSaves;

public:
  explicit UG24MachineFunctionInfo(const Function &F,
                                   const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<UG24MachineFunctionInfo>(*this);
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  ArrayRef<MCRegister> getInterruptSaves() const { return InterruptSaves; }
  void addInterruptSave(MCRegister Reg) { InterruptSaves.push_back(Reg); }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24MACHINEFUNCTIONINFO_H
