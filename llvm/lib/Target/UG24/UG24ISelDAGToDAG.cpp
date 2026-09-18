//===-- UG24ISelDAGToDAG.cpp - A DAG to DAG Inst Selector for UG24 -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "UG24Subtarget.h"
#include "UG24TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "ug24-isel"

namespace {
class UG24DAGToDAGISel : public SelectionDAGISel {
  /// The patterns gated on the optional multiplier and divider are emitted as
  /// "Subtarget->hasMul()", so the generated matcher needs one to ask.
  const UG24Subtarget *Subtarget = nullptr;

public:
  static char ID;

  UG24DAGToDAGISel(UG24TargetMachine &TM, CodeGenOpt::Level OptLevel)
      : SelectionDAGISel(ID, TM, OptLevel) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<UG24Subtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  StringRef getPassName() const override {
    return "UG24 DAG->DAG Pattern Instruction Selection";
  }

  void Select(SDNode *N) override;

  /// Match a frame-index address, optionally with a constant displacement.
  bool SelectAddrFI(SDValue Addr, SDValue &Base, SDValue &Offset);

  /// Match a register address with a displacement that fits the 8-bit field
  /// of an LD or ST.
  bool SelectAddrReg(SDValue Addr, SDValue &Base, SDValue &Offset);

#include "UG24GenDAGISel.inc"
};
} // namespace

char UG24DAGToDAGISel::ID = 0;

bool UG24DAGToDAGISel::SelectAddrFI(SDValue Addr, SDValue &Base,
                                    SDValue &Offset) {
  EVT VT = Addr.getValueType();

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i16);
    return true;
  }

  // frameindex + constant
  if (Addr.getOpcode() == ISD::ADD) {
    if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
      if (auto *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
        Offset =
            CurDAG->getTargetConstant(CN->getSExtValue(), SDLoc(Addr), MVT::i16);
        return true;
      }
    }
  }

  return false;
}

bool UG24DAGToDAGISel::SelectAddrReg(SDValue Addr, SDValue &Base,
                                     SDValue &Offset) {
  // Anything SelectAddrFI can match is left to it, because the SP-relative
  // pseudo forms the address without occupying a register.  A frame index
  // with a *variable* index is not one of those, so it falls through to the
  // register form below and the bare frame index is materialised by LEAFI.
  if (isa<FrameIndexSDNode>(Addr))
    return false;
  if (Addr.getOpcode() == ISD::ADD &&
      isa<FrameIndexSDNode>(Addr.getOperand(0)) &&
      isa<ConstantSDNode>(Addr.getOperand(1)))
    return false;

  // reg + unsigned 8-bit displacement folds into the instruction.
  if (Addr.getOpcode() == ISD::ADD) {
    if (auto *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      int64_t Imm = CN->getSExtValue();
      // Leave one byte of headroom so a 16-bit access can use disp and
      // disp + 1 without overflowing the field.
      if (Imm >= 0 && Imm <= 254) {
        Base = Addr.getOperand(0);
        Offset = CurDAG->getTargetConstant(Imm, SDLoc(Addr), MVT::i8);
        return true;
      }
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i8);
  return true;
}

void UG24DAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  switch (N->getOpcode()) {
  case ISD::FrameIndex: {
    // A bare frame index used as a value becomes an address computation.
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i16);
    SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i16);
    CurDAG->SelectNodeTo(N, UG24::LEAFI, MVT::i16, TFI, Zero);
    return;
  }
  default:
    break;
  }

  SelectCode(N);
}

FunctionPass *llvm::createUG24ISelDag(UG24TargetMachine &TM,
                                      CodeGenOpt::Level OptLevel) {
  return new UG24DAGToDAGISel(TM, OptLevel);
}
