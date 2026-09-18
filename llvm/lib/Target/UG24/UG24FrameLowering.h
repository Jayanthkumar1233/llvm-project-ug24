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
      // StackRealignable is false because nothing here realigns the stack:
      // the prologue moves SP by the frame size and no more.  Left at its
      // default of true, MachineFrameInfo keeps whatever alignment a type
      // would prefer -- eight bytes for a `long long` -- and
      // computeKnownBitsForFrameIndex then tells the DAG that the low bits
      // of that slot's address are zero.  SelectionDAGBuilder turns the
      // offset arithmetic into an OR on the strength of that, and on a
      // byte-granular stack the bits are not actually clear: "a >> 32" on a
      // 64-bit local reads its own low half back.
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            /*StackAlignment=*/Align(1),
                            /*LocalAreaOffset=*/0,
                            /*TransientStackAlignment=*/Align(1),
                            /*StackRealignable=*/false) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS) const override;

  bool hasFP(const MachineFunction &MF) const override;

  /// True when \p MF carries the "interrupt" function attribute, which Clang
  /// attaches for __attribute__((interrupt)).  Such a function is entered by
  /// the hardware rather than by a call, so it preserves every register it
  /// touches and returns by restoring PSW and popping PC.
  static bool isInterruptHandler(const MachineFunction &MF);

  /// Number of bytes the prologue pushes below the incoming stack pointer
  /// before allocating locals.  This is the saved return address of a
  /// non-leaf function, and zero for a leaf.
  static unsigned getRASaveSize(const MachineFunction &MF);

  /// Bytes the prologue pushes to preserve the caller's frame pointer, which
  /// is two when this function needs one and zero otherwise.
  static unsigned getFPSaveSize(const MachineFunction &MF);


  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24FRAMELOWERING_H
