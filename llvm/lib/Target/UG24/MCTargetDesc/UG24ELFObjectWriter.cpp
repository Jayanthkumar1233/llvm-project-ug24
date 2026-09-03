//===-- UG24ELFObjectWriter.cpp - UG24 ELF Writer -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/UG24FixupKinds.h"
#include "MCTargetDesc/UG24MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {
class UG24ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  UG24ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, OSABI, ELF::EM_UG24,
                                /*HasRelocationAddend=*/true) {}

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    switch (Fixup.getTargetKind()) {
    case FK_Data_1:
      return ELF::R_UG24_8;
    case FK_Data_2:
      return ELF::R_UG24_16;
    case FK_Data_4:
      return ELF::R_UG24_32;
    case UG24::fixup_ug24_pcrel_10:
      return ELF::R_UG24_PCREL10;
    case UG24::fixup_ug24_abs_16:
      return ELF::R_UG24_ABS16;
    case UG24::fixup_ug24_lo8:
      return ELF::R_UG24_LO8;
    case UG24::fixup_ug24_hi8:
      return ELF::R_UG24_HI8;
    default:
      Ctx.reportError(Fixup.getLoc(), "unsupported relocation type");
      return ELF::R_UG24_NONE;
    }
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createUG24ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<UG24ELFObjectWriter>(OSABI);
}
