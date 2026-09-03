//===-- UG24FrameLowering.cpp - UG24 Frame Lowering ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 stack grows downwards and is addressed exclusively through SP:
// there is no frame pointer.  SP is a special function register, so adjusting
// it means copying it into DPTR0, doing 16-bit arithmetic there, and copying
// it back.
//
// Because LD and ST always read their base address from DPTR0 (when PSW.DP is
// clear), the prologue of any function with a frame clears PSW.DP once so
// that the reserved DPTR0 is the pointer the hardware selects.
//
//===----------------------------------------------------------------------===//

#include "UG24FrameLowering.h"
#include "UG24.h"
#include "UG24InstrInfo.h"
#include "UG24Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

// PSW.DP selects DPTR1 over DPTR0 as the base for LD/ST.  Bit index taken
// from the PSW layout in the uG24 register spreadsheet.
static constexpr int64_t PSW_DP_BIT = 8;

// RA holds the return address and is clobbered by the next call, so any
// function that makes one has to preserve it across its own body.
unsigned UG24FrameLowering::getRASaveSize(const MachineFunction &MF) {
  return MF.getFrameInfo().hasCalls() ? 2 : 0;
}

bool UG24FrameLowering::hasFP(const MachineFunction &MF) const {
  // Every frame slot is reachable from SP, and variable-sized objects are
  // rejected during lowering, so a frame pointer is never required.
  return false;
}

void UG24FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "prologue belongs in the entry block");

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &TII = *static_cast<const UG24InstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  uint64_t StackSize = MFI.getStackSize();
  bool SaveRA = getRASaveSize(MF) != 0;

  if (StackSize == 0 && !SaveRA)
    return;

  // Preserve the return address before anything else, so that it sits between
  // the incoming stack pointer and this frame's locals.
  if (SaveRA)
    BuildMI(MBB, MBBI, DL, TII.get(UG24::PUSHRA));

  if (StackSize == 0)
    return;

  // Make DPTR0 the active data pointer for the whole function.
  BuildMI(MBB, MBBI, DL, TII.get(UG24::CLRF)).addImm(PSW_DP_BIT);

  // SP -= StackSize, via DPTR0.
  BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
  TII.addImmediate(MBB, MBBI, DL, UG24::DPTR0, -static_cast<int64_t>(StackSize));
  BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
}

void UG24FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &TII = *static_cast<const UG24InstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  uint64_t StackSize = MFI.getStackSize();
  bool SaveRA = getRASaveSize(MF) != 0;

  if (StackSize == 0 && !SaveRA)
    return;

  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // SP += StackSize, then restore RA - the mirror image of the prologue.
  if (StackSize != 0) {
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
    TII.addImmediate(MBB, MBBI, DL, UG24::DPTR0, static_cast<int64_t>(StackSize));
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
  }

  if (SaveRA)
    BuildMI(MBB, MBBI, DL, TII.get(UG24::POPRA));
}

MachineBasicBlock::iterator UG24FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const auto &TII = *static_cast<const UG24InstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  // Outgoing arguments are placed in a fixed area reserved by the prologue, so
  // the call-sequence markers only need adjusting when the frame is not
  // pre-reserved.
  if (!hasReservedCallFrame(MF)) {
    int64_t Amount = MI->getOperand(0).getImm();
    if (MI->getOpcode() == UG24::ADJCALLSTACKDOWN)
      Amount = -Amount;
    if (Amount != 0) {
      DebugLoc DL = MI->getDebugLoc();
      BuildMI(MBB, MI, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
      TII.addImmediate(MBB, MI, DL, UG24::DPTR0, Amount);
      BuildMI(MBB, MI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
    }
  }

  return MBB.erase(MI);
}
