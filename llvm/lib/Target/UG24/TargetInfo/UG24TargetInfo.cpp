//===-- UG24TargetInfo.cpp - UG24 Target Implementation ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/UG24TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheUG24Target() {
  static Target TheUG24Target;
  return TheUG24Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24TargetInfo() {
  RegisterTarget<Triple::ug24> X(getTheUG24Target(), "ug24",
                                 "uG24 8-bit Microprocessor", "UG24");
}
