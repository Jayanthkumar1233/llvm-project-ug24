//===-- UG24FrameLowering.cpp - UG24 Frame Lowering ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 stack grows downwards.  SP is a special function register, so
// adjusting it means copying it into DPTR0, doing 16-bit arithmetic there, and
// copying it back.
//
// Frames are addressed through SP wherever possible, because SP costs no
// register.  A function with a variable-length array or an alloca cannot do
// that: it moves SP after the prologue, and every SP-relative offset computed
// before that point becomes wrong.  Those functions take P3 as a frame
// pointer, set to the post-prologue SP and left alone thereafter, and address
// their frame through it instead.
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
#include "UG24MachineFunctionInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/Function.h"

using namespace llvm;

// PSW.DP selects DPTR1 over DPTR0 as the base for LD/ST.  Bit index taken
// from the PSW layout in the uG24 register spreadsheet.
static constexpr int64_t PSW_DP_BIT = 8;

// RA holds the return address and is clobbered by the next call, so any
// function that makes one has to preserve it across its own body.
unsigned UG24FrameLowering::getRASaveSize(const MachineFunction &MF) {
  return MF.getFrameInfo().hasCalls() ? 2 : 0;
}

unsigned UG24FrameLowering::getFPSaveSize(const MachineFunction &MF) {
  const auto *TFI = MF.getSubtarget().getFrameLowering();
  return TFI->hasFP(MF) ? 2 : 0;
}

bool UG24FrameLowering::isInterruptHandler(const MachineFunction &MF) {
  return MF.getFunction().hasFnAttribute("interrupt");
}

// An interrupt handler is entered by the hardware, not by a call, so the
// caller-saved/callee-saved split does not apply to it: the code it preempted
// expects every register to come back unchanged.  The hardware saves PC and
// PSW (see the interrupt model in docs/uG24-assumptions.md); everything else
// is this prologue's job.
//
// Rather than push all sixteen registers, push the ones the handler actually
// writes.  That has to be decided here, inside PEI, because this is the last
// point at which the function's register liveness is still tracked.
void UG24FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                             BitVector &SavedRegs,
                                             RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  if (!isInterruptHandler(MF))
    return;

  const MachineRegisterInfo &MRI = MF.getRegInfo();
  auto *MFInfo = MF.getInfo<UG24MachineFunctionInfo>();

  // A handler that calls out cannot know what the callee writes, so it saves
  // everything.  RA is covered separately: getRASaveSize() already pushes it
  // whenever the function makes a call, which is the only way RA is written.
  bool SavesEverything = MF.getFrameInfo().hasCalls();

  // R11 and DPTR0 are reserved, so the generic callee-saved machinery would
  // never consider them, yet the expansion temporary and the memory base
  // register are exactly what a handler is most likely to overwrite.
  for (MCRegister Reg : UG24::GPRRegClass)
    if (SavesEverything || MRI.isPhysRegModified(Reg))
      MFInfo->addInterruptSave(Reg);
}

bool UG24FrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // Only the cases that actually move SP behind the prologue's back, or that
  // need the frame's address as a value.  Everything else stays SP-relative
  // and keeps P3 available to the allocator.
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
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
  bool UseFP = hasFP(MF);
  ArrayRef<MCRegister> IntSaves =
      MF.getInfo<UG24MachineFunctionInfo>()->getInterruptSaves();

  if (StackSize == 0 && !SaveRA && !UseFP && IntSaves.empty())
    return;

  // Preserve the return address before anything else, so that it sits between
  // the incoming stack pointer and this frame's locals.
  if (SaveRA)
    BuildMI(MBB, MBBI, DL, TII.get(UG24::PUSHRA));

  // Then, in an interrupt handler, every register it is about to overwrite.
  for (MCRegister Reg : IntSaves)
    BuildMI(MBB, MBBI, DL, TII.get(UG24::PUSH)).addReg(Reg);

  // Make DPTR0 the active data pointer for the whole function.
  BuildMI(MBB, MBBI, DL, TII.get(UG24::CLRF)).addImm(PSW_DP_BIT);

  // Then the caller's frame pointer, if this function is taking it over.
  if (UseFP) {
    BuildMI(MBB, MBBI, DL, TII.get(UG24::PUSH)).addReg(UG24::R6);
    BuildMI(MBB, MBBI, DL, TII.get(UG24::PUSH)).addReg(UG24::R7);
  }

  // SP -= StackSize, via DPTR0.  DPTR0 is left holding the new SP.
  BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
  if (StackSize != 0) {
    TII.addImmediate(MBB, MBBI, DL, UG24::DPTR0,
                     -static_cast<int64_t>(StackSize));
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
  }

  // FP = SP, once and for all.  Everything this function does to SP after
  // this point -- a VLA, an alloca, a call sequence -- leaves FP alone.
  if (UseFP) {
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOV), UG24::R6).addReg(UG24::R14);
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOV), UG24::R7).addReg(UG24::R15);
  }
}

void UG24FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &TII = *static_cast<const UG24InstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  uint64_t StackSize = MFI.getStackSize();
  bool SaveRA = getRASaveSize(MF) != 0;
  bool UseFP = hasFP(MF);
  ArrayRef<MCRegister> IntSaves =
      MF.getInfo<UG24MachineFunctionInfo>()->getInterruptSaves();

  if (StackSize == 0 && !SaveRA && !UseFP && IntSaves.empty())
    return;

  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  if (UseFP) {
    // SP = FP.  This is the whole reason the frame pointer exists: it undoes
    // the prologue's allocation and every dynamic one in a single move,
    // without knowing how much was taken.
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOV), UG24::R14).addReg(UG24::R6);
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOV), UG24::R15).addReg(UG24::R7);
    // SP = FP + StackSize.  FP was set to the stack pointer *after* the locals
    // were allocated, so that every frame offset is a non-negative
    // displacement -- LD and ST have no signed form.  Undoing the prologue
    // therefore steps back over the locals too, and adding a constant is what
    // makes this independent of whatever a VLA did to SP in between.
    if (StackSize != 0)
      TII.addImmediate(MBB, MBBI, DL, UG24::DPTR0,
                       static_cast<int64_t>(StackSize));
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
    BuildMI(MBB, MBBI, DL, TII.get(UG24::POP), UG24::R7);
    BuildMI(MBB, MBBI, DL, TII.get(UG24::POP), UG24::R6);
  } else if (StackSize != 0) {
    // SP += StackSize - the mirror image of the prologue.
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
    TII.addImmediate(MBB, MBBI, DL, UG24::DPTR0,
                     static_cast<int64_t>(StackSize));
    BuildMI(MBB, MBBI, DL, TII.get(UG24::MOVSPX)).addReg(UG24::DPTR0);
  }

  // Unwind the interrupt saves in the opposite order to the prologue's.
  for (MCRegister Reg : reverse(IntSaves))
    BuildMI(MBB, MBBI, DL, TII.get(UG24::POP), Reg);

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
