//===-- UG24InstrInfo.cpp - UG24 Instruction Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24InstrInfo.h"
#include "UG24.h"
#include "UG24Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "UG24GenInstrInfo.inc"

using namespace llvm;

void UG24InstrInfo::anchor() {}

UG24InstrInfo::UG24InstrInfo()
    : UG24GenInstrInfo(UG24::ADJCALLSTACKDOWN, UG24::ADJCALLSTACKUP), RI() {}

unsigned UG24CC::getBranchOpcode(UG24CC::CondCode CC) {
  switch (CC) {
  case UG24CC::COND_EQ: return UG24::BEQ;
  case UG24CC::COND_NE: return UG24::BNE;
  case UG24CC::COND_LT: return UG24::BLT;
  case UG24CC::COND_LE: return UG24::BLE;
  case UG24CC::COND_GT: return UG24::BGT;
  case UG24CC::COND_GE: return UG24::BGE;
  case UG24CC::COND_Z:  return UG24::BZ;
  case UG24CC::COND_NZ: return UG24::BNZ;
  case UG24CC::COND_C:  return UG24::BC;
  case UG24CC::COND_NC: return UG24::BNC;
  case UG24CC::COND_PS: return UG24::BPS;
  case UG24CC::COND_NS: return UG24::BNS;
  case UG24CC::COND_INVALID: break;
  }
  llvm_unreachable("invalid uG24 condition code");
}

UG24CC::CondCode UG24CC::getOppositeCondition(UG24CC::CondCode CC) {
  switch (CC) {
  case UG24CC::COND_EQ: return UG24CC::COND_NE;
  case UG24CC::COND_NE: return UG24CC::COND_EQ;
  case UG24CC::COND_LT: return UG24CC::COND_GE;
  case UG24CC::COND_GE: return UG24CC::COND_LT;
  case UG24CC::COND_GT: return UG24CC::COND_LE;
  case UG24CC::COND_LE: return UG24CC::COND_GT;
  case UG24CC::COND_Z:  return UG24CC::COND_NZ;
  case UG24CC::COND_NZ: return UG24CC::COND_Z;
  case UG24CC::COND_C:  return UG24CC::COND_NC;
  case UG24CC::COND_NC: return UG24CC::COND_C;
  case UG24CC::COND_PS: return UG24CC::COND_NS;
  case UG24CC::COND_NS: return UG24CC::COND_PS;
  case UG24CC::COND_INVALID: break;
  }
  llvm_unreachable("invalid uG24 condition code");
}

UG24CC::CondCode UG24CC::getSwappedCondition(UG24CC::CondCode CC) {
  switch (CC) {
  case UG24CC::COND_LT: return UG24CC::COND_GT;
  case UG24CC::COND_GT: return UG24CC::COND_LT;
  case UG24CC::COND_LE: return UG24CC::COND_GE;
  case UG24CC::COND_GE: return UG24CC::COND_LE;
  default: return CC;
  }
}

void UG24InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI,
                                const DebugLoc &DL, MCRegister DestReg,
                                MCRegister SrcReg, bool KillSrc) const {
  if (UG24::GPRRegClass.contains(DestReg, SrcReg)) {
    BuildMI(MBB, MI, DL, get(UG24::MOV), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  // 16-bit pairs are copied a byte at a time; MOV16 is split by
  // UG24ExpandPseudo.
  if (UG24::GPR16RegClass.contains(DestReg, SrcReg)) {
    BuildMI(MBB, MI, DL, get(UG24::MOV16), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  // Moving to or from SP and RA goes through the 3-bit extended-register
  // field, which can only name W, DPTR1 and DPTR0.  Any other pair has to be
  // reached via the reserved DPTR0.
  if (SrcReg == UG24::SP || SrcReg == UG24::RA) {
    unsigned Opc = SrcReg == UG24::SP ? UG24::MOVXSP : UG24::MOVXRA;
    if (UG24::XRegRegClass.contains(DestReg)) {
      BuildMI(MBB, MI, DL, get(Opc), DestReg);
    } else {
      BuildMI(MBB, MI, DL, get(Opc), UG24::DPTR0);
      BuildMI(MBB, MI, DL, get(UG24::MOV16), DestReg).addReg(UG24::DPTR0);
    }
    return;
  }
  if (DestReg == UG24::SP || DestReg == UG24::RA) {
    unsigned Opc = DestReg == UG24::SP ? UG24::MOVSPX : UG24::MOVRAX;
    if (UG24::XRegRegClass.contains(SrcReg)) {
      BuildMI(MBB, MI, DL, get(Opc)).addReg(SrcReg, getKillRegState(KillSrc));
    } else {
      BuildMI(MBB, MI, DL, get(UG24::MOV16), UG24::DPTR0)
          .addReg(SrcReg, getKillRegState(KillSrc));
      BuildMI(MBB, MI, DL, get(Opc)).addReg(UG24::DPTR0);
    }
    return;
  }

  llvm_unreachable("cannot copy this register pair on uG24");
}

void UG24InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        Register SrcReg, bool isKill,
                                        int FrameIndex,
                                        const TargetRegisterClass *RC,
                                        const TargetRegisterInfo *TRI,
                                        Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  if (UG24::GPRRegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(UG24::STFI))
        .addReg(SrcReg, getKillRegState(isKill))
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO);
    return;
  }

  // A 16-bit pair is spilled whole; STFI16 is split into two byte stores by
  // eliminateFrameIndex, once real registers have been assigned.
  if (UG24::GPR16RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(UG24::STFI16))
        .addReg(SrcReg, getKillRegState(isKill))
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO);
    return;
  }

  llvm_unreachable("cannot spill this register class on uG24");
}

void UG24InstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI,
                                         Register DestReg, int FrameIndex,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI,
                                         Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  if (UG24::GPRRegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(UG24::LDFI), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO);
    return;
  }

  if (UG24::GPR16RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(UG24::LDFI16), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO);
    return;
  }

  llvm_unreachable("cannot reload this register class on uG24");
}

//===----------------------------------------------------------------------===//
// Branch analysis
//===----------------------------------------------------------------------===//

bool UG24InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;

    if (I->getOpcode() == UG24::JR) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }
      // Delete any instructions after an unconditional branch.
      while (std::next(I) != MBB.end())
        std::next(I)->eraseFromParent();
      Cond.clear();
      FBB = nullptr;
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    if (I->getOpcode() == UG24::BRCC) {
      if (!Cond.empty())
        return true; // More than one conditional branch: give up.
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(I->getOperand(1)); // the condition code
      continue;
    }

    return true; // Some terminator we do not understand.
  }

  return false;
}

unsigned UG24InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not tracked");

  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != UG24::JR && I->getOpcode() != UG24::BRCC)
      break;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  return Count;
}

unsigned UG24InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *TBB,
                                     MachineBasicBlock *FBB,
                                     ArrayRef<MachineOperand> Cond,
                                     const DebugLoc &DL,
                                     int *BytesAdded) const {
  assert(!BytesAdded && "code size not tracked");
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.empty()) &&
         "uG24 branch conditions have exactly one component");

  if (Cond.empty()) {
    assert(!FBB && "unconditional branch cannot have two targets");
    BuildMI(&MBB, DL, get(UG24::JR)).addMBB(TBB);
    return 1;
  }

  BuildMI(&MBB, DL, get(UG24::BRCC)).addMBB(TBB).addImm(Cond[0].getImm());
  if (!FBB)
    return 1;

  BuildMI(&MBB, DL, get(UG24::JR)).addMBB(FBB);
  return 2;
}

bool UG24InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "expected exactly one condition component");
  auto CC = static_cast<UG24CC::CondCode>(Cond[0].getImm());
  Cond[0].setImm(UG24CC::getOppositeCondition(CC));
  return false;
}

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

void UG24InstrInfo::addImmediate(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 const DebugLoc &DL, Register Reg16,
                                 int64_t Amount) const {
  if (Amount == 0)
    return;

  Register Lo = RI.getSubReg(Reg16, sub_lo);
  Register Hi = RI.getSubReg(Reg16, sub_hi);

  bool IsSub = Amount < 0;
  uint16_t Value = static_cast<uint16_t>(IsSub ? -Amount : Amount);
  uint8_t LoByte = Value & 0xff;
  uint8_t HiByte = (Value >> 8) & 0xff;

  // Operate on the low byte with flags, then propagate the carry (or borrow)
  // into the high byte.  ADC/SBB take a register, so the high byte of the
  // constant is materialised in the reserved temporary R11 first.
  BuildMI(MBB, MI, DL, get(IsSub ? UG24::SBI : UG24::ADI), Lo)
      .addReg(Lo)
      .addImm(LoByte);
  BuildMI(MBB, MI, DL, get(UG24::MVI), UG24::R11).addImm(HiByte);
  BuildMI(MBB, MI, DL, get(IsSub ? UG24::SBB : UG24::ADC), Hi)
      .addReg(Hi)
      .addReg(UG24::R11);
}
