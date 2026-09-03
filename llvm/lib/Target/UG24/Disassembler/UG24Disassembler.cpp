//===-- UG24Disassembler.cpp - Disassembler for UG24 -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/UG24MCTargetDesc.h"
#include "TargetInfo/UG24TargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "ug24-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class UG24Disassembler : public MCDisassembler {
public:
  UG24Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // namespace

static const uint16_t GPRDecoderTable[] = {
    UG24::R0,  UG24::R1,  UG24::R2,  UG24::R3,
    UG24::R4,  UG24::R5,  UG24::R6,  UG24::R7,
    UG24::R8,  UG24::R9,  UG24::R10, UG24::R11,
    UG24::R12, UG24::R13, UG24::R14, UG24::R15,
};

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  if (RegNo >= std::size(GPRDecoderTable))
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

static const uint16_t GPR16DecoderTable[8] = {
    UG24::NoRegister, UG24::NoRegister, UG24::NoRegister, UG24::NoRegister,
    UG24::W,          UG24::NoRegister, UG24::DPTR1,      UG24::DPTR0,
};

static DecodeStatus DecodeGPR16RegisterClass(MCInst &Inst, uint64_t RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  if (RegNo >= std::size(GPR16DecoderTable) ||
      GPR16DecoderTable[RegNo] == UG24::NoRegister)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPR16DecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// The extended-register field names the same three pairs.
static DecodeStatus DecodeXRegRegisterClass(MCInst &Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const MCDisassembler *Decoder) {
  return DecodeGPR16RegisterClass(Inst, RegNo, Address, Decoder);
}

// INC/DEC and the shift instructions encode "amount - 1".
template <unsigned Bits>
static DecodeStatus decodeImmPlus1Operand(MCInst &Inst, uint64_t Imm,
                                          uint64_t Address,
                                          const MCDisassembler *Decoder) {
  if (!isUInt<Bits>(Imm))
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createImm(Imm + 1));
  return MCDisassembler::Success;
}

// Branch displacements are a signed count of instruction words taken from the
// instruction that follows the branch.  Print them as a byte offset from the
// branch itself so the value round-trips through the assembler.
static DecodeStatus decodeBranchTarget(MCInst &Inst, uint64_t Imm,
                                       uint64_t Address,
                                       const MCDisassembler *Decoder) {
  int64_t Offset = SignExtend64<10>(Imm) * 2 + 2;
  Inst.addOperand(MCOperand::createImm(Offset));
  return MCDisassembler::Success;
}

#include "UG24GenDisassemblerTables.inc"

DecodeStatus UG24Disassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                              ArrayRef<uint8_t> Bytes,
                                              uint64_t Address,
                                              raw_ostream &CStream) const {
  Size = 0;
  if (Bytes.size() < 2)
    return MCDisassembler::Fail;

  // Try the 32-bit (2-fetch) forms first: they are distinguished by a zero
  // high byte in the first halfword, which no 16-bit encoding uses.
  if (Bytes.size() >= 4) {
    uint32_t Insn32 = support::endian::read32le(Bytes.data());
    DecodeStatus Result = decodeInstruction(DecoderTable32, Instr, Insn32,
                                            Address, this, STI);
    if (Result != MCDisassembler::Fail) {
      Size = 4;
      return Result;
    }
    Instr.clear();
  }

  uint16_t Insn16 = support::endian::read16le(Bytes.data());
  DecodeStatus Result =
      decodeInstruction(DecoderTable16, Instr, Insn16, Address, this, STI);
  if (Result != MCDisassembler::Fail) {
    Size = 2;
    return Result;
  }
  return MCDisassembler::Fail;
}

static MCDisassembler *createUG24Disassembler(const Target &T,
                                              const MCSubtargetInfo &STI,
                                              MCContext &Ctx) {
  return new UG24Disassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheUG24Target(),
                                         createUG24Disassembler);
}
