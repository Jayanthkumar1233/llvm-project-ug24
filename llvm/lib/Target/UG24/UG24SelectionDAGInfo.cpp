//===-- UG24SelectionDAGInfo.cpp - UG24 SelectionDAG Info -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24SelectionDAGInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

using namespace llvm;

SDValue UG24SelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool IsVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo) const {
  const TargetLowering &TLI = DAG.getTargetLoweringInfo();
  LLVMContext &Ctx = *DAG.getContext();
  const DataLayout &DL32 = DAG.getDataLayout();

  const char *Name = TLI.getLibcallName(RTLIB::MEMSET);
  if (!Name)
    return SDValue();

  // memset(void *dst, int value, size_t size) -- the fill value widened to
  // int, which is what the library actually declares.
  TargetLowering::ArgListTy Args;
  TargetLowering::ArgListEntry Entry;

  Entry.Node = Dst;
  Entry.Ty = PointerType::getUnqual(Ctx);
  Args.push_back(Entry);

  Entry.Node = DAG.getZExtOrTrunc(Src, DL, MVT::i16);
  Entry.Ty = Type::getInt16Ty(Ctx);
  Args.push_back(Entry);

  Entry.Node = Size;
  Entry.Ty = DL32.getIntPtrType(Ctx);
  Args.push_back(Entry);

  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL)
      .setChain(Chain)
      .setLibCallee(TLI.getLibcallCallingConv(RTLIB::MEMSET),
                    PointerType::getUnqual(Ctx),
                    DAG.getExternalSymbol(Name, TLI.getPointerTy(DL32)),
                    std::move(Args))
      .setDiscardResult();

  return TLI.LowerCallTo(CLI).second;
}
