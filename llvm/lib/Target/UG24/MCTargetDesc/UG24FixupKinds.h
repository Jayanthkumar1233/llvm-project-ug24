//===-- UG24FixupKinds.h - UG24 Specific Fixup Entries ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24FIXUPKINDS_H
#define LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef UG24

namespace llvm {
namespace UG24 {

enum Fixups {
  // A 10-bit PC-relative displacement in Inst{15-6}, counted in 2-byte
  // instruction words relative to the instruction following the branch.
  fixup_ug24_pcrel_10 = FirstTargetFixupKind,

  // A 16-bit absolute address occupying the second halfword of a JA/LJA.
  fixup_ug24_abs_16,

  // The low / high byte of a 16-bit address, placed in the imm8 field of an
  // MVI at Inst{15-8}.
  fixup_ug24_lo8,
  fixup_ug24_hi8,

  fixup_ug24_invalid,
  NumTargetFixupKinds = fixup_ug24_invalid - FirstTargetFixupKind
};

} // namespace UG24
} // namespace llvm

#endif // LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24FIXUPKINDS_H
