//===--- UG24.cpp - Implement UG24 target feature support -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/STLExtras.h"
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

  // Let a source file see which optional arithmetic blocks it was built for.
  // ug24_builtins.c does not need them -- its helpers are shift-and-add loops
  // either way -- but a hand-written inner loop may want to choose.
  if (HasMul)
    Builder.defineMacro("__UG24_HAS_MUL__");
  if (HasDiv)
    Builder.defineMacro("__UG24_HAS_DIV__");
}

// "generic" and "ug24" are the fully-populated part; "ug24-base" is the same
// core with neither optional block.  The names match the ProcessorModels in
// llvm/lib/Target/UG24/UG24.td and must stay in step with them.
static constexpr StringRef ValidCPUNames[] = {"generic", "ug24", "ug24-base"};

bool UG24TargetInfo::isValidCPUName(StringRef Name) const {
  return llvm::is_contained(ValidCPUNames, Name);
}

void UG24TargetInfo::fillValidCPUList(SmallVectorImpl<StringRef> &Values) const {
  Values.append(std::begin(ValidCPUNames), std::end(ValidCPUNames));
}

bool UG24TargetInfo::setCPU(const std::string &Name) {
  if (!isValidCPUName(Name))
    return false;
  CPU = Name;
  HasMul = HasDiv = Name != "ug24-base";
  return true;
}

// Explicit -target-feature flags are applied after the CPU, so that
// "-mcpu=ug24-base -Xclang -target-feature -Xclang +mul" describes a part with
// the multiplier but not the divider.
bool UG24TargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                          DiagnosticsEngine &Diags) {
  for (StringRef Feature : Features) {
    bool Enable = Feature.starts_with("+");
    StringRef Name = Feature.drop_front();
    if (Name == "mul")
      HasMul = Enable;
    else if (Name == "div")
      HasDiv = Enable;
  }
  return true;
}
