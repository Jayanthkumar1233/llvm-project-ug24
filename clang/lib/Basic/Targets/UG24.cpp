//===--- UG24.cpp - Implement UG24 target feature support -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

const char *const UG24TargetInfo::GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "w",   "dptr1", "dptr0",
    "pc",  "ra",  "psw", "sp",
};

ArrayRef<const char *> UG24TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void UG24TargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__ug24__");
  Builder.defineMacro("__UG24__");
  Builder.defineMacro("__UG24");
  Builder.defineMacro("__ELF__");
}
