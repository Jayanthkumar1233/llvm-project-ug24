//===-- UG24MCExpr.h - UG24 specific MC expression classes -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCEXPR_H
#define LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCEXPR_H

#include "llvm/MC/MCExpr.h"

namespace llvm {

/// Selects one byte of a 16-bit address so it can be placed in the 8-bit
/// immediate field of an MVI.
class UG24MCExpr : public MCTargetExpr {
public:
  enum VariantKind {
    VK_UG24_None,
    VK_UG24_LO8, // low byte of the address
    VK_UG24_HI8, // high byte of the address
  };

  static const UG24MCExpr *create(VariantKind Kind, const MCExpr *Expr,
                                  MCContext &Ctx);

  VariantKind getKind() const { return Kind; }
  const MCExpr *getSubExpr() const { return Expr; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAsmLayout *Layout,
                                 const MCFixup *Fixup) const override;
  void visitUsedExpr(MCStreamer &Streamer) const override;
  MCFragment *findAssociatedFragment() const override {
    return getSubExpr()->findAssociatedFragment();
  }
  void fixELFSymbolsInTLSFixups(MCAssembler &) const override {}

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }

private:
  const VariantKind Kind;
  const MCExpr *Expr;

  explicit UG24MCExpr(VariantKind Kind, const MCExpr *Expr)
      : Kind(Kind), Expr(Expr) {}
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCEXPR_H
