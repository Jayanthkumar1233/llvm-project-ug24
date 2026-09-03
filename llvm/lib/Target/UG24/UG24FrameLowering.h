//===-- UG24FrameLowering.h - Define Frame Lowering for UG24 -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24FRAMELOWERING_H
#define LLVM_LIB_TARGET_UG24_UG24FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class UG24FrameLowering : public TargetFrameLowering {
public:
  UG24FrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            /*StackAlignment=*/Align(1),
                            /*LocalAreaOffset=*/0) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  bool hasFP(const MachineFunction &MF) const override;

  /// Number of bytes the prologue pushes below the incoming stack pointer
  /// before allocating locals.  This is the saved return address of a
  /// non-leaf function, and zero for a leaf.
  static unsigned getRASaveSize(const MachineFunction &MF);

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24FRAMELOWERING_H
