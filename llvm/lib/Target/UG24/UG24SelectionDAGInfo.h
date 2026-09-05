//===-- UG24SelectionDAGInfo.h - UG24 SelectionDAG Info ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24SELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_UG24_UG24SELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class UG24SelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  /// Emit the memset libcall by hand so that the fill value is passed as an
  /// int.  LLVM's generic path types that argument as i8, taking it from the
  /// value's own legalised type, while <string.h> declares memset's second
  /// parameter as int.  On a target where a byte and a word travel in
  /// different registers -- R0 versus a pair here -- those two disagree, and
  /// the callee reads its length from the wrong place.  Nothing diagnoses it:
  /// the memset simply writes no bytes.
  SDValue EmitTargetCodeForMemset(SelectionDAG &DAG, const SDLoc &DL,
                                  SDValue Chain, SDValue Dst, SDValue Src,
                                  SDValue Size, Align Alignment,
                                  bool IsVolatile, bool AlwaysInline,
                                  MachinePointerInfo DstPtrInfo) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24SELECTIONDAGINFO_H
