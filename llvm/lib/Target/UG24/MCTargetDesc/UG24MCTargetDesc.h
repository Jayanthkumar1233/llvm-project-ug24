//===-- UG24MCTargetDesc.h - UG24 Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCTARGETDESC_H
#define LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

#include "llvm/MC/MCTargetOptions.h"

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

MCCodeEmitter *createUG24MCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx);

MCAsmBackend *createUG24AsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createUG24ELFObjectWriter(uint8_t OSABI);

namespace UG24II {
/// Target-specific operand flags, telling MCInstLower which byte of a 16-bit
/// address an MVI operand wants.
enum TOF {
  MO_NO_FLAG = 0,
  MO_LO8,
  MO_HI8,
};
} // namespace UG24II

} // namespace llvm

#define GET_REGINFO_ENUM
#include "UG24GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "UG24GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "UG24GenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_UG24_MCTARGETDESC_UG24MCTARGETDESC_H
