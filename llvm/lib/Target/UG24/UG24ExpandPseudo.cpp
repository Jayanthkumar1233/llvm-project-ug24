//===-- UG24ExpandPseudo.cpp - Expand uG24 pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 ALU is 8 bits wide, so every 16-bit operation is selected as a
// pseudo instruction and split into a low-byte / high-byte pair here, after
// register allocation has assigned real register pairs.  The same pass turns
// BRCC into the conditional branch that matches its condition code and CALL
// into an LJA.
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "UG24InstrInfo.h"
#include "UG24Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define UG24_EXPAND_PSEUDO_NAME "uG24 pseudo instruction expansion"

namespace {

class UG24ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;

  UG24ExpandPseudo() : MachineFunctionPass(ID) {
    initializeUG24ExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return UG24_EXPAND_PSEUDO_NAME; }

private:
  const UG24InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);

  /// Emit \p Opcode on the low bytes and \p HiOpcode on the high bytes of a
  /// 16-bit destination / source pair.
  void expandBinary16(MachineInstr &MI, unsigned LoOpcode, unsigned HiOpcode);
};

char UG24ExpandPseudo::ID = 0;

void UG24ExpandPseudo::expandBinary16(MachineInstr &MI, unsigned LoOpcode,
                                      unsigned HiOpcode) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  Register Src = MI.getOperand(2).getReg();
  bool KillSrc = MI.getOperand(2).isKill();

  Register DstLo = TRI->getSubReg(Dst, sub_lo);
  Register DstHi = TRI->getSubReg(Dst, sub_hi);
  Register SrcLo = TRI->getSubReg(Src, sub_lo);
  Register SrcHi = TRI->getSubReg(Src, sub_hi);

  BuildMI(MBB, MI, DL, TII->get(LoOpcode), DstLo)
      .addReg(DstLo)
      .addReg(SrcLo, getKillRegState(KillSrc));
  BuildMI(MBB, MI, DL, TII->get(HiOpcode), DstHi)
      .addReg(DstHi)
      .addReg(SrcHi, getKillRegState(KillSrc));
}

bool UG24ExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  default:
    return false;

  case UG24::MOV16: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    bool KillSrc = MI.getOperand(1).isKill();
    BuildMI(MBB, MI, DL, TII->get(UG24::MOV),
            TRI->getSubReg(Dst, sub_lo))
        .addReg(TRI->getSubReg(Src, sub_lo), getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, TII->get(UG24::MOV),
            TRI->getSubReg(Dst, sub_hi))
        .addReg(TRI->getSubReg(Src, sub_hi), getKillRegState(KillSrc));
    break;
  }

  case UG24::MVI16: {
    Register Dst = MI.getOperand(0).getReg();
    Register Lo = TRI->getSubReg(Dst, sub_lo);
    Register Hi = TRI->getSubReg(Dst, sub_hi);
    const MachineOperand &Imm = MI.getOperand(1);

    if (Imm.isImm()) {
      uint16_t Value = static_cast<uint16_t>(Imm.getImm());
      BuildMI(MBB, MI, DL, TII->get(UG24::MVI), Lo).addImm(Value & 0xff);
      BuildMI(MBB, MI, DL, TII->get(UG24::MVI), Hi).addImm((Value >> 8) & 0xff);
    } else {
      // A symbol address: the two MVIs carry lo8/hi8 relocations against it.
      // The MC layer defaults a symbol in an imm8 field to its low byte, so
      // the high half is requested with a target flag.
      BuildMI(MBB, MI, DL, TII->get(UG24::MVI), Lo)
          .addGlobalAddress(Imm.getGlobal(), Imm.getOffset(), UG24II::MO_LO8);
      BuildMI(MBB, MI, DL, TII->get(UG24::MVI), Hi)
          .addGlobalAddress(Imm.getGlobal(), Imm.getOffset(), UG24II::MO_HI8);
    }
    break;
  }

  // Carry/borrow propagate from the low byte into the high byte.
  case UG24::ADD16:
    expandBinary16(MI, UG24::ADD, UG24::ADC);
    break;
  case UG24::SUB16:
    expandBinary16(MI, UG24::SUB, UG24::SBB);
    break;
  case UG24::AND16:
    expandBinary16(MI, UG24::AND, UG24::AND);
    break;
  case UG24::OR16:
    expandBinary16(MI, UG24::OR, UG24::OR);
    break;
  case UG24::XOR16:
    expandBinary16(MI, UG24::XOR, UG24::XOR);
    break;

  // MUL and DIV write W implicitly; move the result where it is wanted.
  case UG24::MULW:
  case UG24::DIVW: {
    Register Dst = MI.getOperand(0).getReg();
    unsigned Opc = MI.getOpcode() == UG24::MULW ? UG24::MUL : UG24::DIV;
    BuildMI(MBB, MI, DL, TII->get(Opc))
        .addReg(MI.getOperand(1).getReg(),
                getKillRegState(MI.getOperand(1).isKill()))
        .addReg(MI.getOperand(2).getReg(),
                getKillRegState(MI.getOperand(2).isKill()));
    if (Dst != UG24::W)
      BuildMI(MBB, MI, DL, TII->get(UG24::MOV16), Dst).addReg(UG24::W);
    break;
  }

  case UG24::BRCC: {
    auto CC = static_cast<UG24CC::CondCode>(MI.getOperand(1).getImm());
    BuildMI(MBB, MI, DL, TII->get(UG24CC::getBranchOpcode(CC)))
        .addMBB(MI.getOperand(0).getMBB());
    break;
  }

  // A pointer dereference: park the address in the reserved DPTR0, then use
  // the real LD/ST, whose base is always DPTR0.
  case UG24::LOADp:
  case UG24::LOAD16p:
  case UG24::STOREp:
  case UG24::STORE16p: {
    bool IsLoad = MI.getOpcode() == UG24::LOADp ||
                  MI.getOpcode() == UG24::LOAD16p;
    bool Is16 = MI.getOpcode() == UG24::LOAD16p ||
                MI.getOpcode() == UG24::STORE16p;

    unsigned ValIdx = 0;
    unsigned PtrIdx = IsLoad ? 1 : 1;
    Register Val = MI.getOperand(ValIdx).getReg();
    Register Ptr = MI.getOperand(PtrIdx).getReg();
    int64_t Disp = MI.getOperand(PtrIdx + 1).getImm();

    if (Ptr != UG24::DPTR0)
      BuildMI(MBB, MI, DL, TII->get(UG24::MOV16), UG24::DPTR0).addReg(Ptr);

    if (Is16) {
      Register Lo = TRI->getSubReg(Val, sub_lo);
      Register Hi = TRI->getSubReg(Val, sub_hi);
      if (IsLoad) {
        BuildMI(MBB, MI, DL, TII->get(UG24::LD), Lo).addImm(Disp);
        BuildMI(MBB, MI, DL, TII->get(UG24::LD), Hi).addImm(Disp + 1);
      } else {
        BuildMI(MBB, MI, DL, TII->get(UG24::ST)).addReg(Lo).addImm(Disp);
        BuildMI(MBB, MI, DL, TII->get(UG24::ST)).addReg(Hi).addImm(Disp + 1);
      }
    } else if (IsLoad) {
      BuildMI(MBB, MI, DL, TII->get(UG24::LD), Val).addImm(Disp);
    } else {
      BuildMI(MBB, MI, DL, TII->get(UG24::ST)).addReg(Val).addImm(Disp);
    }
    break;
  }

  case UG24::CALL: {
    auto MIB = BuildMI(MBB, MI, DL, TII->get(UG24::LJA));
    MIB.add(MI.getOperand(0));
    break;
  }

  case UG24::CALLi: {
    // A call through a function pointer.  LJI is not usable for this: its
    // target is {Xs, i7} -- the top nine bits come from the pair and the low
    // seven from the immediate -- so it cannot reach an address that is only
    // known at run time.  (What {Xs, i7} means is open specification query D5;
    // this sequence is correct under either reading, which is the point.)
    //
    // Instead the return address is built from PC and put in RA, the target is
    // pushed high byte first, and POP PC jumps to it.  The two pushes are
    // undone by the pop, so the sequence leaves SP where it found it and the
    // callee still sees the outgoing arguments at the right offsets.
    //
    //     mov  dptr0, pc      ; the address of this very instruction
    //     mvi  r11, 0
    //     adi  r14, 16        ; ... plus the length of the whole sequence
    //     adc  r15, r11
    //     mov  ra, dptr0
    //     push target.hi
    //     push target.lo
    //     pop  pc
    Register Target = MI.getOperand(0).getReg();
    Register Lo = TRI->getSubReg(Target, sub_lo);
    Register Hi = TRI->getSubReg(Target, sub_hi);
    const unsigned SequenceBytes = 16;

    BuildMI(MBB, MI, DL, TII->get(UG24::MOVXPC), UG24::DPTR0);
    BuildMI(MBB, MI, DL, TII->get(UG24::MVI), UG24::R11).addImm(0);
    BuildMI(MBB, MI, DL, TII->get(UG24::ADI), UG24::R14)
        .addReg(UG24::R14)
        .addImm(SequenceBytes);
    BuildMI(MBB, MI, DL, TII->get(UG24::ADC), UG24::R15)
        .addReg(UG24::R15)
        .addReg(UG24::R11);
    BuildMI(MBB, MI, DL, TII->get(UG24::MOVRAX)).addReg(UG24::DPTR0);
    BuildMI(MBB, MI, DL, TII->get(UG24::PUSH)).addReg(Hi);
    BuildMI(MBB, MI, DL, TII->get(UG24::PUSH)).addReg(Lo);
    BuildMI(MBB, MI, DL, TII->get(UG24::POPPC));
    break;
  }
  }

  MI.eraseFromParent();
  return true;
}

bool UG24ExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
  while (I != E) {
    MachineBasicBlock::iterator NMBBI = std::next(I);
    Modified |= expandMI(MBB, I);
    I = NMBBI;
  }
  return Modified;
}

// One expansion can introduce another - the pointer loads emit a MOV16, which
// is itself a pseudo - so the block is rewritten until it stops changing.
static bool hasPseudos(const MachineBasicBlock &MBB) {
  for (const MachineInstr &MI : MBB)
    switch (MI.getOpcode()) {
    case UG24::MOV16:
    case UG24::MVI16:
    case UG24::ADD16:
    case UG24::SUB16:
    case UG24::AND16:
    case UG24::OR16:
    case UG24::XOR16:
    case UG24::BRCC:
    case UG24::CALL:
    case UG24::CALLi:
    case UG24::MULW:
    case UG24::DIVW:
    case UG24::LOADp:
    case UG24::LOAD16p:
    case UG24::STOREp:
    case UG24::STORE16p:
      return true;
    default:
      break;
    }
  return false;
}

bool UG24ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  const UG24Subtarget &STI = MF.getSubtarget<UG24Subtarget>();
  TII = STI.getInstrInfo();
  TRI = STI.getRegisterInfo();

  bool Modified = false;
  for (MachineBasicBlock &MBB : MF) {
    while (hasPseudos(MBB)) {
      bool Changed = expandMBB(MBB);
      Modified |= Changed;
      if (!Changed)
        break; // Nothing left this pass knows how to rewrite.
    }
  }
  return Modified;
}

} // namespace

INITIALIZE_PASS(UG24ExpandPseudo, "ug24-expand-pseudo", UG24_EXPAND_PSEUDO_NAME,
                false, false)

FunctionPass *llvm::createUG24ExpandPseudoPass() {
  return new UG24ExpandPseudo();
}
