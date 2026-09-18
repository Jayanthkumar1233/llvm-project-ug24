//===-- UG24AsmParser.cpp - Parse UG24 assembly to MCInst -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/UG24MCExpr.h"
#include "MCTargetDesc/UG24MCTargetDesc.h"
#include "TargetInfo/UG24TargetInfo.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

namespace {

/// A parsed uG24 assembly operand: a mnemonic/token, a register or an
/// immediate expression.
class UG24Operand : public MCParsedAsmOperand {
  enum KindTy { k_Token, k_Register, k_Immediate } Kind;

  struct TokOp {
    const char *Data;
    unsigned Length;
  };

  struct RegOp {
    unsigned RegNum;
  };

  struct ImmOp {
    const MCExpr *Val;
  };

  union {
    TokOp Tok;
    RegOp Reg;
    ImmOp Imm;
  };

  SMLoc StartLoc, EndLoc;

public:
  UG24Operand(KindTy K, SMLoc Start, SMLoc End)
      : Kind(K), StartLoc(Start), EndLoc(End) {}

  bool isToken() const override { return Kind == k_Token; }
  bool isReg() const override { return Kind == k_Register; }
  bool isImm() const override { return Kind == k_Immediate; }
  bool isMem() const override { return false; }

  /// An immediate whose value is either unknown or inside [Lo, Hi].
  ///
  /// A symbolic expression is accepted unconditionally: nothing here knows
  /// where the symbol will land, and UG24AsmBackend range-checks the value
  /// when it applies the fixup.
  bool isImmInRange(int64_t Lo, int64_t Hi) const {
    if (Kind != k_Immediate)
      return false;
    const auto *CE = dyn_cast<MCConstantExpr>(Imm.Val);
    if (!CE)
      return true;
    int64_t Val = CE->getValue();
    return Val >= Lo && Val <= Hi;
  }

  // The Format I immediate field is eight bits wide and the instructions that
  // use it are a mix of signed (ADI, SBI) and unsigned (MVI, ANDI) readings,
  // so both spellings of a byte are in range.
  bool isImm8() const { return isImmInRange(-128, 255); }
  bool isUImm8() const { return isImmInRange(0, 255); }
  bool isUImm7() const { return isImmInRange(0, 127); }
  bool isUImm4() const { return isImmInRange(0, 15); }
  // INC/DEC and the shifts encode "amount - 1", so zero is not expressible.
  bool isIncImm() const { return isImmInRange(1, 16); }
  bool isShiftImm() const { return isImmInRange(1, 8); }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  StringRef getToken() const {
    assert(Kind == k_Token && "Invalid access!");
    return StringRef(Tok.Data, Tok.Length);
  }

  unsigned getReg() const override {
    assert(Kind == k_Register && "Invalid access!");
    return Reg.RegNum;
  }

  const MCExpr *getImm() const {
    assert(Kind == k_Immediate && "Invalid access!");
    return Imm.Val;
  }

  static void addExpr(MCInst &Inst, const MCExpr *Expr) {
    if (auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void print(raw_ostream &OS) const override {
    switch (Kind) {
    case k_Token:
      OS << "Token:" << getToken();
      break;
    case k_Register:
      OS << "Register:" << getReg();
      break;
    case k_Immediate:
      OS << "Immediate:" << *getImm();
      break;
    }
  }

  static std::unique_ptr<UG24Operand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<UG24Operand>(k_Token, S, S);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    return Op;
  }

  static std::unique_ptr<UG24Operand> createReg(unsigned RegNo, SMLoc S,
                                                SMLoc E) {
    auto Op = std::make_unique<UG24Operand>(k_Register, S, E);
    Op->Reg.RegNum = RegNo;
    return Op;
  }

  static std::unique_ptr<UG24Operand> createImm(const MCExpr *Val, SMLoc S,
                                                SMLoc E) {
    auto Op = std::make_unique<UG24Operand>(k_Immediate, S, E);
    Op->Imm.Val = Val;
    return Op;
  }
};

class UG24AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

#define GET_ASSEMBLER_HEADER
#include "UG24GenAsmMatcher.inc"

  bool parseOperand(OperandVector &Operands);
  bool parseExpressionWithModifier(const MCExpr *&Res);
  bool immRangeError(const OperandVector &Operands, uint64_t ErrorInfo,
                     int64_t Lo, int64_t Hi);

public:
  UG24AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;

  OperandMatchResultTy tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                        SMLoc &EndLoc) override;

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  // One Match_Invalid* per DiagnosticType in UG24InstrInfo.td, generated from
  // the parser match classes.
  enum UG24MatchResultTy {
    Match_Dummy = FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "UG24GenAsmMatcher.inc"
#undef GET_OPERAND_DIAGNOSTIC_TYPES
  };
};

} // namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "UG24GenAsmMatcher.inc"

OperandMatchResultTy UG24AsmParser::tryParseRegister(MCRegister &Reg,
                                                     SMLoc &StartLoc,
                                                     SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();

  if (Tok.isNot(AsmToken::Identifier))
    return MatchOperand_NoMatch;

  unsigned RegNo = MatchRegisterName(Tok.getString().lower());
  if (RegNo == UG24::NoRegister)
    return MatchOperand_NoMatch;

  Reg = RegNo;
  Parser.Lex();
  return MatchOperand_Success;
}

bool UG24AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                  SMLoc &EndLoc) {
  if (tryParseRegister(Reg, StartLoc, EndLoc) != MatchOperand_Success)
    return Error(StartLoc, "invalid register name");
  return false;
}

bool UG24AsmParser::parseOperand(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  SMLoc E = Parser.getTok().getEndLoc();

  MCRegister Reg;
  SMLoc RegStart, RegEnd;
  if (tryParseRegister(Reg, RegStart, RegEnd) == MatchOperand_Success) {
    Operands.push_back(UG24Operand::createReg(Reg, RegStart, RegEnd));
    return false;
  }

  // LD/ST displacements are written "[expr]" to make the implicit DPTR base
  // visible; the brackets carry no extra meaning.
  bool Bracketed = Parser.getTok().is(AsmToken::LBrac);
  if (Bracketed)
    Parser.Lex();

  // An optional '#' prefix is accepted on immediates.
  if (Parser.getTok().is(AsmToken::Hash))
    Parser.Lex();

  const MCExpr *Expr;
  if (parseExpressionWithModifier(Expr))
    return Error(S, "failed to parse operand");

  if (Bracketed) {
    if (Parser.getTok().isNot(AsmToken::RBrac))
      return Error(Parser.getTok().getLoc(), "expected ']'");
    Parser.Lex();
  }

  E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
  Operands.push_back(UG24Operand::createImm(Expr, S, E));
  return false;
}

// Parse an expression, recognising the lo8()/hi8() byte selectors that the
// compiler emits when it splits a 16-bit address across two MVIs.
bool UG24AsmParser::parseExpressionWithModifier(const MCExpr *&Res) {
  UG24MCExpr::VariantKind Kind = UG24MCExpr::VK_UG24_None;

  if (Parser.getTok().is(AsmToken::Identifier)) {
    StringRef Name = Parser.getTok().getString();
    if (Name == "lo8")
      Kind = UG24MCExpr::VK_UG24_LO8;
    else if (Name == "hi8")
      Kind = UG24MCExpr::VK_UG24_HI8;

    if (Kind != UG24MCExpr::VK_UG24_None) {
      if (getLexer().peekTok().isNot(AsmToken::LParen))
        Kind = UG24MCExpr::VK_UG24_None;
      else {
        Parser.Lex(); // the modifier name
        Parser.Lex(); // '('
        if (Parser.parseExpression(Res))
          return true;
        if (Parser.getTok().isNot(AsmToken::RParen))
          return Error(Parser.getTok().getLoc(), "expected ')'");
        Parser.Lex();
        Res = UG24MCExpr::create(Kind, Res, getContext());
        return false;
      }
    }
  }

  return Parser.parseExpression(Res);
}

bool UG24AsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                     StringRef Name, SMLoc NameLoc,
                                     OperandVector &Operands) {
  Operands.push_back(UG24Operand::createToken(Name, NameLoc));

  if (getLexer().is(AsmToken::EndOfStatement)) {
    Parser.Lex();
    return false;
  }

  if (parseOperand(Operands))
    return true;

  while (getLexer().is(AsmToken::Comma)) {
    Parser.Lex();
    if (parseOperand(Operands))
      return true;
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // consume the EndOfStatement
  return false;
}

bool UG24AsmParser::immRangeError(const OperandVector &Operands,
                                  uint64_t ErrorInfo, int64_t Lo, int64_t Hi) {
  SMLoc Loc = ErrorInfo < Operands.size()
                  ? ((UG24Operand &)*Operands[ErrorInfo]).getStartLoc()
                  : ((UG24Operand &)*Operands[0]).getStartLoc();
  return Error(Loc, "immediate must be an integer in the range [" + Twine(Lo) +
                        ", " + Twine(Hi) + "]");
}

bool UG24AsmParser::MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                            OperandVector &Operands,
                                            MCStreamer &Out,
                                            uint64_t &ErrorInfo,
                                            bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    Opcode = Inst.getOpcode();
    return false;
  case Match_MnemonicFail:
    return Error(IDLoc, "invalid instruction mnemonic");
  case Match_MissingFeature:
    // The only predicated instructions are MUL and DIV, whose blocks are
    // optional in the SoC configuration.
    return Error(IDLoc, "instruction requires an optional block that this "
                        "-mcpu or -mattr leaves out");
  case Match_InvalidImm8:
    return immRangeError(Operands, ErrorInfo, -128, 255);
  case Match_InvalidUImm8:
    return immRangeError(Operands, ErrorInfo, 0, 255);
  case Match_InvalidUImm7:
    return immRangeError(Operands, ErrorInfo, 0, 127);
  case Match_InvalidUImm4:
    return immRangeError(Operands, ErrorInfo, 0, 15);
  case Match_InvalidIncImm:
    return immRangeError(Operands, ErrorInfo, 1, 16);
  case Match_InvalidShiftImm:
    return immRangeError(Operands, ErrorInfo, 1, 8);
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo >= Operands.size())
        return Error(IDLoc, "too few operands for instruction");
      ErrorLoc = ((UG24Operand &)*Operands[ErrorInfo]).getStartLoc();
      if (ErrorLoc == SMLoc())
        ErrorLoc = IDLoc;
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  default:
    break;
  }

  llvm_unreachable("Unhandled instruction match result");
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeUG24AsmParser() {
  RegisterMCAsmParser<UG24AsmParser> X(getTheUG24Target());
}
