//===-- UG24MCExpr.cpp - UG24 specific MC expression classes --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24MCExpr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

const UG24MCExpr *UG24MCExpr::create(VariantKind Kind, const MCExpr *Expr,
                                     MCContext &Ctx) {
  return new (Ctx) UG24MCExpr(Kind, Expr);
}

void UG24MCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  switch (Kind) {
  case VK_UG24_LO8:
    OS << "lo8(";
    break;
  case VK_UG24_HI8:
    OS << "hi8(";
    break;
  case VK_UG24_None:
    Expr->print(OS, MAI);
    return;
  }
  Expr->print(OS, MAI);
  OS << ')';
}

bool UG24MCExpr::evaluateAsRelocatableImpl(MCValue &Res,
                                           const MCAsmLayout *Layout,
                                           const MCFixup *Fixup) const {
  if (!getSubExpr()->evaluateAsRelocatable(Res, Layout, Fixup))
    return false;

  // Only a fully-resolved constant can be folded here; otherwise the byte
  // selection is left to the relocation.
  if (Res.isAbsolute()) {
    int64_t Value = Res.getConstant();
    switch (Kind) {
    case VK_UG24_LO8:
      Res = MCValue::get(Value & 0xff);
      break;
    case VK_UG24_HI8:
      Res = MCValue::get((Value >> 8) & 0xff);
      break;
    case VK_UG24_None:
      break;
    }
    return true;
  }

  Res = MCValue::get(Res.getSymA(), Res.getSymB(), Res.getConstant(), Kind);
  return true;
}

void UG24MCExpr::visitUsedExpr(MCStreamer &Streamer) const {
  Streamer.visitUsedExpr(*getSubExpr());
}
