//===- UG24.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 is an 8-bit microprocessor with a 16-bit address space.  All code
// is position dependent and there is no dynamic linking, so the relocations
// are a short list of absolute byte/word forms plus the PC-relative branch
// displacement.
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class UG24 final : public TargetInfo {
public:
  UG24();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

UG24::UG24() {
  // Fill gaps with WFI (0x8000), which halts rather than running into data.
  trapInstr = {0x00, 0x80, 0x00, 0x80};
}

RelExpr UG24::getRelExpr(RelType type, const Symbol &s,
                         const uint8_t *loc) const {
  switch (type) {
  case R_UG24_PCREL10:
    return R_PC;
  case R_UG24_NONE:
    return R_NONE;
  default:
    return R_ABS;
  }
}

void UG24::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_UG24_NONE:
    break;

  case R_UG24_8:
    checkUInt(loc, val, 8, rel);
    *loc = val & 0xff;
    break;

  case R_UG24_16:
  case R_UG24_ABS16:
    checkUInt(loc, val, 16, rel);
    write16le(loc, val & 0xffff);
    break;

  case R_UG24_32:
    write32le(loc, val & 0xffffffff);
    break;

  case R_UG24_LO8:
    // The byte goes into the imm8 field at Inst{15-8}, i.e. the high byte of
    // the little-endian halfword.
    loc[1] = val & 0xff;
    break;

  case R_UG24_HI8:
    loc[1] = (val >> 8) & 0xff;
    break;

  case R_UG24_PCREL10: {
    // The hardware adds the encoded amount to the address of the instruction
    // that follows the branch, counting in 2-byte instruction words.
    int64_t offset = static_cast<int64_t>(val) - 2;
    if (offset & 1)
      error(getErrorLocation(loc) + "branch target is not 2-byte aligned");
    offset >>= 1;
    checkInt(loc, offset, 10, rel);
    uint16_t insn = read16le(loc);
    insn = (insn & 0x003f) | ((offset & 0x3ff) << 6);
    write16le(loc, insn);
    break;
  }

  default:
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
  }
}

TargetInfo *elf::getUG24TargetInfo() {
  static UG24 target;
  return &target;
}
