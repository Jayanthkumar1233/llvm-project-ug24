//===-- UG24ISelLowering.h - UG24 DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24ISELLOWERING_H
#define LLVM_LIB_TARGET_UG24_UG24ISELLOWERING_H

#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class UG24Subtarget;

namespace UG24ISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET_GLUE,
  CALL,
  CMP,
  BR_CC,
  SELECT_CC,
  SETCC16,
  BR_CC16,
  WRAPPER,
  LO8,
  HI8,
  PAIR
};
} // namespace UG24ISD

class UG24TargetLowering : public TargetLowering {
  const UG24Subtarget &Subtarget;

public:
  explicit UG24TargetLowering(const TargetMachine &TM,
                              const UG24Subtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &DL, SelectionDAG &DAG) const override;

  // The only shift amounts the hardware encodes are 1..8, so shifts by a
  // value that is not a constant must be expanded.
  bool isTruncateFree(Type *SrcTy, Type *DstTy) const override;
  bool isTruncateFree(EVT SrcVT, EVT DstVT) const override;
  bool isZExtFree(Type *SrcTy, Type *DstTy) const override;
  bool isZExtFree(EVT SrcVT, EVT DstVT) const override;

  MVT getScalarShiftAmountTy(const DataLayout &, EVT) const override {
    return MVT::i8;
  }

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override {
    return MVT::i8;
  }

  /// A sign- or zero-extended return value is passed in R0 as a byte.  The
  /// generic hook would widen it to the register type of i32, which on this
  /// target is i16, and that would disagree with what the call site reads.
  EVT getTypeForExtReturn(LLVMContext &Context, EVT VT,
                          ISD::NodeType /*ExtendKind*/) const override {
    return VT.bitsLT(MVT::i8) ? MVT::i8 : VT;
  }

private:
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSETCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerStore(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerMUL(SDValue Op, SelectionDAG &DAG) const;

  /// Emit a CMP that sets PSW and return the uG24 condition code that
  /// corresponds to \p CC, swapping the operands where that is cheaper.
  SDValue emitCompare(SDValue LHS, SDValue RHS, ISD::CondCode CC,
                      const SDLoc &DL, SelectionDAG &DAG,
                      SDValue &UG24CondCode) const;

public:
  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24ISELLOWERING_H
