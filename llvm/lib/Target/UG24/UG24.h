//===-- UG24.h - Top-level interface for UG24 representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_UG24_H
#define LLVM_LIB_TARGET_UG24_UG24_H

#include "MCTargetDesc/UG24MCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class UG24TargetMachine;
class FunctionPass;
class PassRegistry;

namespace UG24CC {
// Condition codes, mapped one-to-one onto the conditional branch opcodes.
enum CondCode {
  COND_EQ = 0, // BEQ  - PSW.EQ
  COND_NE,     // BNE  - !PSW.EQ
  COND_LT,     // BLT  - PSW.LT
  COND_LE,     // BLE  - PSW.LT | PSW.EQ
  COND_GT,     // BGT  - PSW.GT
  COND_GE,     // BGE  - PSW.GT | PSW.EQ
  COND_Z,      // BZ   - PSW.Z
  COND_NZ,     // BNZ  - !PSW.Z
  COND_C,      // BC   - PSW.Cy
  COND_NC,     // BNC  - !PSW.Cy
  COND_PS,     // BPS  - !PSW.S
  COND_NS,     // BNS  - PSW.S
  COND_INVALID
};

/// Map a condition code onto the opcode of the branch that tests it.
unsigned getBranchOpcode(CondCode CC);

/// The condition that is true exactly when \p CC is false.
CondCode getOppositeCondition(CondCode CC);

/// The condition obtained by swapping the operands of the compare.
CondCode getSwappedCondition(CondCode CC);
} // namespace UG24CC

FunctionPass *createUG24ISelDag(UG24TargetMachine &TM,
                                CodeGenOpt::Level OptLevel);

/// Post-RA pass expanding the 16-bit and frame-index pseudo instructions into
/// real uG24 encodings.
FunctionPass *createUG24ExpandPseudoPass();
void initializeUG24ExpandPseudoPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_UG24_H
