//===-- UG24RegisterInfo.cpp - UG24 Register Information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24RegisterInfo.h"
#include "UG24.h"
#include "UG24InstrInfo.h"
#include "UG24Subtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "UG24FrameLowering.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

#define GET_REGINFO_TARGET_DESC
#include "UG24GenRegisterInfo.inc"

using namespace llvm;

UG24RegisterInfo::UG24RegisterInfo() : UG24GenRegisterInfo(UG24::RA) {}

const MCPhysReg *
UG24RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_UG24_SaveList;
}

const uint32_t *
UG24RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const {
  return CSR_UG24_RegMask;
}

BitVector UG24RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  // Special function registers are never allocated.
  Reserved.set(UG24::PC);
  Reserved.set(UG24::RA);
  Reserved.set(UG24::SP);
  Reserved.set(UG24::PSW);

  // R11 is the expansion temporary, so the pair containing it is unusable.
  Reserved.set(UG24::R11);
  Reserved.set(UG24::P5);

  // DPTR0 (R15:R14) is the dedicated memory base register.
  Reserved.set(UG24::R14);
  Reserved.set(UG24::R15);

  // P3 becomes the frame pointer in a function with a variable-sized object,
  // so the allocator must not hand it out there.  Every other function keeps
  // it, which matters on a machine with ten allocatable bytes.
  if (MF.getSubtarget().getFrameLowering()->hasFP(MF)) {
    Reserved.set(UG24::R6);
    Reserved.set(UG24::R7);
    Reserved.set(UG24::P3);
  }
  Reserved.set(UG24::DPTR0);

  return Reserved;
}

Register UG24RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  // The uG24 backend addresses the frame entirely through SP; there is no
  // frame pointer.
  return UG24::SP;
}

bool UG24RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetFrameLowering &TFI = *MF.getSubtarget().getFrameLowering();
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();

  // Object offsets are measured from the incoming stack pointer, so turning
  // one into a displacement from the final SP means adding back the frame
  // size.  Incoming arguments live above the saved return address, so they
  // also have to step over it; locals sit below it and do not.
  bool UseFP = TFI.hasFP(MF);
  unsigned RASave = UG24FrameLowering::getRASaveSize(MF);
  unsigned FPSave = UG24FrameLowering::getFPSaveSize(MF);
  int64_t Offset = MFI.getObjectOffset(FrameIndex) +
                   MFI.getStackSize() +
                   (MFI.isFixedObjectIndex(FrameIndex) ? RASave + FPSave : 0) +
                   MI.getOperand(FIOperandNum + 1).getImm();

  // Without a frame pointer the base is SP, so a call sequence that has
  // pushed arguments shifts every offset; with one, SP is irrelevant.
  if (!UseFP)
    Offset += SPAdj;
  else
    assert(SPAdj == 0 || true);

  assert(Offset >= 0 && "frame slot lies below the stack pointer");

  const UG24InstrInfo &TII =
      *static_cast<const UG24InstrInfo *>(MF.getSubtarget().getInstrInfo());

  // Point DPTR0 at the stack frame.  Displacements up to 255 are folded into
  // the LD/ST itself; anything larger is added into DPTR0 first.
  if (UseFP) {
    BuildMI(MBB, II, DL, TII.get(UG24::MOV), UG24::R14).addReg(UG24::R6);
    BuildMI(MBB, II, DL, TII.get(UG24::MOV), UG24::R15).addReg(UG24::R7);
  } else {
    BuildMI(MBB, II, DL, TII.get(UG24::MOVXSP), UG24::DPTR0);
  }

  bool Is16 = MI.getOpcode() == UG24::LDFI16 || MI.getOpcode() == UG24::STFI16;
  int64_t Disp = Offset;
  if (!isUInt<8>(Disp) || (Is16 && !isUInt<8>(Disp + 1))) {
    TII.addImmediate(MBB, II, DL, UG24::DPTR0, Disp);
    Disp = 0;
  }

  switch (MI.getOpcode()) {
  case UG24::LDFI:
    BuildMI(MBB, II, DL, TII.get(UG24::LD), MI.getOperand(0).getReg())
        .addImm(Disp)
        .setMemRefs(MI.memoperands());
    break;
  case UG24::STFI:
    BuildMI(MBB, II, DL, TII.get(UG24::ST))
        .addReg(MI.getOperand(0).getReg(),
                getKillRegState(MI.getOperand(0).isKill()))
        .addImm(Disp)
        .setMemRefs(MI.memoperands());
    break;
  case UG24::LDFI16: {
    Register Val = MI.getOperand(0).getReg();
    BuildMI(MBB, II, DL, TII.get(UG24::LD), getSubReg(Val, sub_lo))
        .addImm(Disp)
        .setMemRefs(MI.memoperands());
    BuildMI(MBB, II, DL, TII.get(UG24::LD), getSubReg(Val, sub_hi))
        .addImm(Disp + 1);
    break;
  }
  case UG24::STFI16: {
    Register Val = MI.getOperand(0).getReg();
    BuildMI(MBB, II, DL, TII.get(UG24::ST))
        .addReg(getSubReg(Val, sub_lo))
        .addImm(Disp)
        .setMemRefs(MI.memoperands());
    BuildMI(MBB, II, DL, TII.get(UG24::ST))
        .addReg(getSubReg(Val, sub_hi))
        .addImm(Disp + 1);
    break;
  }
  case UG24::LEAFI: {
    // The address itself is wanted, so copy DPTR0 into the destination and
    // fold any remaining displacement in.
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(MBB, II, DL, TII.get(UG24::MOV16), Dst).addReg(UG24::DPTR0);
    if (Disp != 0)
      TII.addImmediate(MBB, II, DL, Dst, Disp);
    break;
  }
  default:
    llvm_unreachable("unexpected frame-index instruction");
  }

  MI.eraseFromParent();
  return true;
}
