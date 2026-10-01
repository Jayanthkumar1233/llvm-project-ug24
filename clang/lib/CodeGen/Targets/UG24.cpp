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
// The ABI confirmed by the hardware team, spelled out here rather than
// inherited from DefaultABIInfo so that a second implementation has something
// to match.  See docs/uG24-assumptions.md.
//
//   * void, and an empty struct, are returned in nothing at all.
//   * A scalar of 32 bits or fewer is returned in registers: R0 for a byte,
//     X0 (R0:R1) for 16 bits, X0:X1 (R0:R3) for 32 -- see RetCC_UG24.
//   * Everything wider, aggregates included, is returned through a hidden
//     pointer the caller passes in R0:R1.  Four bytes is all the ABI gives a
//     return value, and a return value has nowhere to spill, so a `long long`
//     goes through memory exactly as a large struct does.  The confirmed
//     answers stop at 32 bits and name the hidden pointer only for structs;
//     applying the same rule to a wider scalar is the one reading that keeps
//     both consistent.
//   * An argument of 32 bits or fewer is passed in R0-R3 where room is left
//     and on the stack otherwise; an aggregate is always passed by reference,
//     with the caller owning the copy.
//   * A narrow integer is promoted to int, as C requires, so that a callee
//     compiled from a prototype-less declaration agrees with its caller.
//
// The register assignment itself lives in UG24CallingConv.td; this file only
// decides direct vs. indirect, which is the part Clang owns.
//===----------------------------------------------------------------------===//

namespace {

/// Widest scalar that comes back in registers, in bits: X0:X1, four bytes.
/// Anything wider gets a hidden pointer.
static constexpr uint64_t MaxDirectReturnBits = 32;

/// Widest scalar passed by value before it goes by reference.  Larger than the
/// four bytes the argument registers hold, because the calling convention
/// continues on the stack by itself and copying a `long long` through memory at
/// every call site would cost more than letting it split.
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

  // _Complex float is two 32-bit halves, twice what the return registers hold;
  // treat it like an aggregate.
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
