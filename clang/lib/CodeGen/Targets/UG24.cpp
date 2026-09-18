//===- UG24.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// uG24 ABI Implementation
//
// The uG24 specification defines no C ABI, so this is the one written down in
// docs/uG24-assumptions.md, spelled out here rather than inherited from
// DefaultABIInfo so that a second implementation has something to match.
//
//   * void, and an empty struct, are returned in nothing at all.
//   * A scalar of 32 bits or fewer is returned in registers: W, then DPTR1.
//     A 64-bit scalar uses P0 and P1 as well -- see RetCC_UG24.
//   * Every other type, aggregates included, is returned through a hidden
//     pointer passed by the caller.  The uG24 has four 16-bit register pairs
//     available to a return value and no way for one to spill, so anything
//     larger has to go through memory.
//   * An argument of 32 bits or fewer is passed in registers where any are
//     left and on the stack otherwise; an aggregate is always passed by
//     reference, with the caller owning the copy.
//   * A narrow integer is promoted to int, as C requires, so that a callee
//     compiled from a prototype-less declaration agrees with its caller.
//
// The register assignment itself lives in UG24CallingConv.td; this file only
// decides direct vs. indirect, which is the part Clang owns.
//===----------------------------------------------------------------------===//

namespace {

/// Widest scalar that comes back in registers, in bits.  Four 16-bit pairs.
static constexpr uint64_t MaxDirectReturnBits = 64;

/// Widest scalar passed in registers before the stack takes over.  The
/// calling convention spills on its own, so this only bounds what is passed
/// by value rather than by reference.
static constexpr uint64_t MaxDirectArgumentBits = 64;

class UG24ABIInfo : public ABIInfo {
public:
  UG24ABIInfo(CodeGenTypes &CGT) : ABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const;
  ABIArgInfo classifyArgumentType(QualType Ty) const;

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }

  Address EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                    QualType Ty) const override {
    return EmitVAArgInstr(CGF, VAListAddr, Ty, classifyArgumentType(Ty));
  }
};

ABIArgInfo UG24ABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy->isVoidType())
    return ABIArgInfo::getIgnore();

  if (isAggregateTypeForABI(RetTy)) {
    // An empty struct occupies nothing and is returned in nothing.
    if (!getContext().getTypeSize(RetTy))
      return ABIArgInfo::getIgnore();
    return getNaturalAlignIndirect(RetTy);
  }

  // _Complex float is two 32-bit halves, which is wider than the return
  // registers hold once anything else is in them; treat it like an aggregate.
  if (RetTy->isAnyComplexType() &&
      getContext().getTypeSize(RetTy) > MaxDirectReturnBits)
    return getNaturalAlignIndirect(RetTy);

  if (const EnumType *ET = RetTy->getAs<EnumType>())
    RetTy = ET->getDecl()->getIntegerType();

  if (getContext().getTypeSize(RetTy) > MaxDirectReturnBits)
    return getNaturalAlignIndirect(RetTy);

  return isPromotableIntegerTypeForABI(RetTy) ? ABIArgInfo::getExtend(RetTy)
                                              : ABIArgInfo::getDirect();
}

ABIArgInfo UG24ABIInfo::classifyArgumentType(QualType Ty) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  if (isAggregateTypeForABI(Ty)) {
    if (!getContext().getTypeSize(Ty))
      return ABIArgInfo::getIgnore();
    // By reference, with the caller owning the copy: the uG24 has no block
    // move and pushing a struct a byte at a time at every call site costs
    // more code than the copy the callee would have made anyway.
    return getNaturalAlignIndirect(Ty, /*ByVal=*/true);
  }

  if (const EnumType *ET = Ty->getAs<EnumType>())
    Ty = ET->getDecl()->getIntegerType();

  if (getContext().getTypeSize(Ty) > MaxDirectArgumentBits)
    return getNaturalAlignIndirect(Ty, /*ByVal=*/true);

  return isPromotableIntegerTypeForABI(Ty) ? ABIArgInfo::getExtend(Ty)
                                           : ABIArgInfo::getDirect();
}

class UG24TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  UG24TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<UG24ABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override;
};

} // namespace

void UG24TargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &M) const {
  if (GV->isDeclaration())
    return;

  const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
  if (!FD || !FD->hasAttr<UG24InterruptAttr>())
    return;

  // The backend looks for this string attribute in UG24FrameLowering and
  // UG24ExpandPseudo: it makes the prologue preserve every register the
  // handler writes and the epilogue return through PSW and PC rather than RA.
  auto *F = cast<llvm::Function>(GV);
  F->addFnAttr("interrupt");

  // Nothing calls a handler, so inlining it into an ordinary function would
  // silently drop the entry and exit code that makes it a handler.
  F->addFnAttr(llvm::Attribute::NoInline);
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createUG24TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<UG24TargetCodeGenInfo>(CGM.getTypes());
}
