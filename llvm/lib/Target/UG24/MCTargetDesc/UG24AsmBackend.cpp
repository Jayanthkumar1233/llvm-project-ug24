//===-- UG24AsmBackend.cpp - UG24 Assembler Backend ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/UG24FixupKinds.h"
#include "MCTargetDesc/UG24MCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class UG24AsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  UG24AsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::support::endianness::little), OSABI(OSABI) {}

  unsigned getNumFixupKinds() const override {
    return UG24::NumTargetFixupKinds;
  }

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    return false;
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    // uG24 instructions are 2 bytes wide, so padding is only meaningful in
    // even multiples.  NOP is encoded as 0x0000.
    if (Count % 2 != 0)
      return false;
    for (uint64_t i = 0; i < Count; i += 2)
      support::endian::write<uint16_t>(OS, 0x0000,
                                       llvm::support::endianness::little);
    return true;
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createUG24ELFObjectWriter(OSABI);
  }
};

const MCFixupKindInfo &
UG24AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // name, offset (bits), size (bits), flags
  static const MCFixupKindInfo Infos[UG24::NumTargetFixupKinds] = {
      {"fixup_ug24_pcrel_10", 6, 10, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_ug24_abs_16", 0, 16, 0},
      {"fixup_ug24_lo8", 8, 8, 0},
      {"fixup_ug24_hi8", 8, 8, 0},
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "invalid fixup kind");
  return Infos[Kind - FirstTargetFixupKind];
}

// Narrow \p Value to the field described by \p Kind, diagnosing anything that
// does not fit.
static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("unhandled uG24 fixup kind");

  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return Value;

  case UG24::fixup_ug24_pcrel_10: {
    // Value is the byte displacement from the branch itself.  The hardware
    // adds the encoded amount to the address of the following instruction and
    // counts in instruction words.
    int64_t Offset = static_cast<int64_t>(Value) - 2;
    if (Offset % 2 != 0) {
      Ctx.reportError(Fixup.getLoc(), "branch target is not 2-byte aligned");
      return 0;
    }
    Offset /= 2;
    if (!isInt<10>(Offset)) {
      Ctx.reportError(Fixup.getLoc(), "branch target out of range "
                                      "(must be within +/- 1024 instructions)");
      return 0;
    }
    return static_cast<uint64_t>(Offset) & 0x3ff;
  }

  case UG24::fixup_ug24_abs_16:
    if (!isUInt<16>(Value)) {
      Ctx.reportError(Fixup.getLoc(), "absolute address does not fit in 16 bits");
      return 0;
    }
    return Value & 0xffff;

  case UG24::fixup_ug24_lo8:
    return Value & 0xff;

  case UG24::fixup_ug24_hi8:
    return (Value >> 8) & 0xff;
  }
}

void UG24AsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                const MCValue &Target,
                                MutableArrayRef<char> Data, uint64_t Value,
                                bool IsResolved,
                                const MCSubtargetInfo *STI) const {
  MCContext &Ctx = Asm.getContext();
  Value = adjustFixupValue(Fixup, Value, Ctx);
  if (!Value)
    return; // Either nothing to do, or an error was already reported.

  MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetOffset + Info.TargetSize, 8) / 8;
  assert(Offset + NumBytes <= Data.size() && "fixup is out of range");

  // Shift the value into position and OR it into the little-endian encoding.
  Value <<= Info.TargetOffset;
  for (unsigned i = 0; i != NumBytes; ++i)
    Data[Offset + i] |= static_cast<uint8_t>((Value >> (i * 8)) & 0xff);
}

} // namespace

MCAsmBackend *llvm::createUG24AsmBackend(const Target &T,
                                         const MCSubtargetInfo &STI,
                                         const MCRegisterInfo &MRI,
                                         const MCTargetOptions &Options) {
  return new UG24AsmBackend(
      MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS()));
}
