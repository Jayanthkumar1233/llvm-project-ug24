//===-- UG24ISelLowering.cpp - UG24 DAG Lowering Implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24ISelLowering.h"
#include "UG24.h"
#include "UG24MachineFunctionInfo.h"
#include "UG24RegisterInfo.h"
#include "UG24Subtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ug24-lower"

#include "UG24GenCallingConv.inc"

UG24TargetLowering::UG24TargetLowering(const TargetMachine &TM,
                                       const UG24Subtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  addRegisterClass(MVT::i8, &UG24::GPRRegClass);
  addRegisterClass(MVT::i16, &UG24::GPR16RegClass);

  computeRegisterProperties(Subtarget.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(UG24::SP);
  setTargetDAGCombine({ISD::TRUNCATE, ISD::STORE});
  setBooleanContents(ZeroOrOneBooleanContent);
  setMinFunctionAlignment(Align(2));
  setPrefFunctionAlignment(Align(2));

  // There are no condition-code producing selects or setccs; everything goes
  // through CMP followed by a conditional branch.
  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::BR_CC, VT, Custom);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
    setOperationAction(ISD::SETCC, VT, Custom);
    setOperationAction(ISD::SELECT, VT, Expand);
  }
  // BRCOND is chain-typed, so it has to be legalised on MVT::Other -- asking
  // for it on i8/i16 sets nothing and leaves a branch on a plain boolean (a
  // bitfield test, say) with no pattern to match.  Expanding it rewrites the
  // branch as BR_CC against zero, which LowerBR_CC already handles.
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::BRIND, MVT::Other, Expand);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);

  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i16, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i16, Custom);

  // The hardware has no variable shift, so only constant amounts in 1..8 are
  // selectable; everything else is expanded into a libcall or a loop.
  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::SHL, VT, Custom);
    setOperationAction(ISD::SRL, VT, Custom);
    setOperationAction(ISD::SRA, VT, Custom);
    setOperationAction(ISD::SHL_PARTS, VT, Expand);
    setOperationAction(ISD::SRL_PARTS, VT, Expand);
    setOperationAction(ISD::SRA_PARTS, VT, Expand);
    setOperationAction(ISD::ROTL, VT, Expand);
    setOperationAction(ISD::ROTR, VT, Expand);
  }
  setOperationAction(ISD::ROTL, MVT::i8, Legal);
  setOperationAction(ISD::ROTR, MVT::i8, Legal);

  // Division, remainder and the wider multiplies go to compiler-rt.  The
  // hardware MUL/DIV are 8x8 with an implicit destination and are emitted by
  // peepholes rather than by pattern matching.
  // The hardware has an 8x8 multiply and an 8-bit unsigned divide; everything
  // wider, and the signed forms, go to the runtime helpers.
  setOperationAction(ISD::MUL, MVT::i8, Legal);
  setOperationAction(ISD::UDIV, MVT::i8, Legal);
  setOperationAction(ISD::UREM, MVT::i8, Legal);

  setOperationAction(ISD::MUL, MVT::i16, Custom);
  setOperationAction(ISD::UDIV, MVT::i16, Expand);
  setOperationAction(ISD::UREM, MVT::i16, Expand);
  setOperationAction(ISD::SDIV, MVT::i8, Expand);
  setOperationAction(ISD::SREM, MVT::i8, Expand);

  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::SDIV, VT, Expand);
    setOperationAction(ISD::SREM, VT, Expand);
    setOperationAction(ISD::SDIVREM, VT, Expand);
    setOperationAction(ISD::UDIVREM, VT, Expand);
    setOperationAction(ISD::MULHS, VT, Expand);
    setOperationAction(ISD::MULHU, VT, Expand);
    setOperationAction(ISD::SMUL_LOHI, VT, Expand);
    setOperationAction(ISD::UMUL_LOHI, VT, Expand);

    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTLZ, VT, Expand);
    setOperationAction(ISD::CTPOP, VT, Expand);
    setOperationAction(ISD::BSWAP, VT, Expand);
    setOperationAction(ISD::BITREVERSE, VT, Expand);

    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);
    setOperationAction(ISD::ADDC, VT, Expand);
    setOperationAction(ISD::ADDE, VT, Expand);
    setOperationAction(ISD::SUBC, VT, Expand);
    setOperationAction(ISD::SUBE, VT, Expand);
  }
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // A widening load is a byte load followed by an explicit extension, and a
  // narrowing store just stores the low byte.
  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
  }
  setLoadExtAction(ISD::EXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i16, MVT::i8, Expand);
  setTruncStoreAction(MVT::i16, MVT::i8, Custom);
}

// Narrow a 16-bit operation whose result is only used as a byte.
//
// The low byte of a sum, difference, product or bitwise operation depends
// only on the low bytes of its operands, so when the wide result has no other
// use the high half is pure waste.  This matters most for MUL: the 8-bit form
// is a single hardware instruction, while the 16-bit form is a call into the
// runtime.
static SDValue narrowToByte(SDValue V, const SDLoc &DL, SelectionDAG &DAG) {
  if (V.getValueType() != MVT::i16 || !V.hasOneUse())
    return SDValue();

  switch (V.getOpcode()) {
  case ISD::MUL:
  case ISD::ADD:
  case ISD::SUB:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
    break;
  default:
    return SDValue();
  }

  SDValue L = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, V.getOperand(0));
  SDValue R = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, V.getOperand(1));
  return DAG.getNode(V.getOpcode(), DL, MVT::i8, L, R);
}

static SDValue combineTruncate(SDNode *N, SelectionDAG &DAG) {
  if (N->getValueType(0) != MVT::i8)
    return SDValue();
  return narrowToByte(N->getOperand(0), SDLoc(N), DAG);
}

// A truncating store to a byte is the same opportunity, but reaches the
// combiner as a store rather than as a TRUNCATE node.
static SDValue combineStore(SDNode *N, SelectionDAG &DAG) {
  auto *ST = cast<StoreSDNode>(N);
  if (!ST->isTruncatingStore() || ST->isIndexed() ||
      ST->getMemoryVT() != MVT::i8)
    return SDValue();

  SDLoc DL(N);
  SDValue Narrow = narrowToByte(ST->getValue(), DL, DAG);
  if (!Narrow)
    return SDValue();

  return DAG.getStore(ST->getChain(), DL, Narrow, ST->getBasePtr(),
                      ST->getMemOperand());
}

SDValue UG24TargetLowering::PerformDAGCombine(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  switch (N->getOpcode()) {
  case ISD::TRUNCATE:
    return combineTruncate(N, DCI.DAG);
  case ISD::STORE:
    return combineStore(N, DCI.DAG);
  default:
    return SDValue();
  }
}

const char *UG24TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<UG24ISD::NodeType>(Opcode)) {
  case UG24ISD::FIRST_NUMBER: break;
  case UG24ISD::RET_GLUE:     return "UG24ISD::RET_GLUE";
  case UG24ISD::CALL:         return "UG24ISD::CALL";
  case UG24ISD::CMP:          return "UG24ISD::CMP";
  case UG24ISD::BR_CC:        return "UG24ISD::BR_CC";
  case UG24ISD::SELECT_CC:    return "UG24ISD::SELECT_CC";
  case UG24ISD::SETCC16:      return "UG24ISD::SETCC16";
  case UG24ISD::BR_CC16:      return "UG24ISD::BR_CC16";
  case UG24ISD::MULW:         return "UG24ISD::MULW";
  case UG24ISD::WRAPPER:      return "UG24ISD::WRAPPER";
  case UG24ISD::LO8:          return "UG24ISD::LO8";
  case UG24ISD::HI8:          return "UG24ISD::HI8";
  case UG24ISD::PAIR:         return "UG24ISD::PAIR";
  }
  return nullptr;
}

bool UG24TargetLowering::isTruncateFree(Type *SrcTy, Type *DstTy) const {
  return SrcTy->isIntegerTy(16) && DstTy->isIntegerTy(8);
}
bool UG24TargetLowering::isTruncateFree(EVT SrcVT, EVT DstVT) const {
  return SrcVT == MVT::i16 && DstVT == MVT::i8;
}
bool UG24TargetLowering::isZExtFree(Type *, Type *) const { return false; }
bool UG24TargetLowering::isZExtFree(EVT, EVT) const { return false; }

SDValue UG24TargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::BR_CC:          return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:      return LowerSELECT_CC(Op, DAG);
  case ISD::SETCC:          return LowerSETCC(Op, DAG);
  case ISD::GlobalAddress:  return LowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:   return LowerBlockAddress(Op, DAG);
  case ISD::ExternalSymbol: return LowerExternalSymbol(Op, DAG);
  case ISD::VASTART:        return LowerVASTART(Op, DAG);
  case ISD::STORE:          return LowerStore(Op, DAG);
  case ISD::MUL:            return LowerMUL(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:            return LowerShift(Op, DAG);
  default:
    report_fatal_error("unimplemented operation in the uG24 backend");
  }
}

//===----------------------------------------------------------------------===//
// Comparisons and branches
//===----------------------------------------------------------------------===//
//
// CMP records EQ / LT / GT for an *unsigned* byte comparison (and leaves the
// borrow in PSW.Cy).  Signed comparisons are therefore performed by flipping
// the sign bit of both operands first, which turns the signed ordering into
// the unsigned one.  See docs/uG24-assumptions.md.
//
//===----------------------------------------------------------------------===//

/// Map an ISD condition onto the uG24 flag test, reporting whether the
/// operands need swapping and whether the comparison is signed.
static UG24CC::CondCode translateCC(ISD::CondCode CC, bool &IsSigned) {
  switch (CC) {
  case ISD::SETEQ:  IsSigned = false; return UG24CC::COND_EQ;
  case ISD::SETNE:  IsSigned = false; return UG24CC::COND_NE;
  case ISD::SETULT: IsSigned = false; return UG24CC::COND_LT;
  case ISD::SETULE: IsSigned = false; return UG24CC::COND_LE;
  case ISD::SETUGT: IsSigned = false; return UG24CC::COND_GT;
  case ISD::SETUGE: IsSigned = false; return UG24CC::COND_GE;
  case ISD::SETLT:  IsSigned = true;  return UG24CC::COND_LT;
  case ISD::SETLE:  IsSigned = true;  return UG24CC::COND_LE;
  case ISD::SETGT:  IsSigned = true;  return UG24CC::COND_GT;
  case ISD::SETGE:  IsSigned = true;  return UG24CC::COND_GE;
  default:
    report_fatal_error("unsupported condition code for the uG24 target");
  }
}

/// XOR the sign bit of \p V so that an unsigned comparison of the result
/// orders values the same way a signed comparison orders the originals.
static SDValue flipSignBit(SDValue V, const SDLoc &DL, SelectionDAG &DAG) {
  EVT VT = V.getValueType();
  APInt Mask = APInt::getSignMask(VT.getSizeInBits());
  return DAG.getNode(ISD::XOR, DL, VT, V, DAG.getConstant(Mask, DL, VT));
}

SDValue UG24TargetLowering::emitCompare(SDValue LHS, SDValue RHS,
                                        ISD::CondCode CC, const SDLoc &DL,
                                        SelectionDAG &DAG,
                                        SDValue &UG24CondCode) const {
  bool IsSigned = false;
  UG24CC::CondCode Cond = translateCC(CC, IsSigned);

  if (IsSigned) {
    LHS = flipSignBit(LHS, DL, DAG);
    RHS = flipSignBit(RHS, DL, DAG);
  }

  assert(LHS.getValueType() == MVT::i8 &&
         "16-bit compares go through SETCC16");

  UG24CondCode = DAG.getConstant(Cond, DL, MVT::i8);
  return DAG.getNode(UG24ISD::CMP, DL, MVT::Glue, LHS, RHS);
}

SDValue UG24TargetLowering::LowerSETCC(SDValue Op, SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
  SDLoc DL(Op);
  EVT VT = Op.getValueType();

  SDValue Bool;
  if (LHS.getValueType() == MVT::i16) {
    // A 16-bit comparison needs two byte compares, which is control flow; the
    // custom inserter builds it and leaves a 0/1 byte behind.
    bool IsSigned = false;
    UG24CC::CondCode Cond = translateCC(CC, IsSigned);
    if (IsSigned) {
      LHS = flipSignBit(LHS, DL, DAG);
      RHS = flipSignBit(RHS, DL, DAG);
    }
    Bool = DAG.getNode(UG24ISD::SETCC16, DL, MVT::i8, LHS, RHS,
                       DAG.getConstant(Cond, DL, MVT::i8));
  } else {
    SDValue TargetCC;
    SDValue Glue = emitCompare(LHS, RHS, CC, DL, DAG, TargetCC);
    Bool = DAG.getNode(UG24ISD::SELECT_CC, DL, MVT::i8,
                       DAG.getConstant(1, DL, MVT::i8),
                       DAG.getConstant(0, DL, MVT::i8), TargetCC, Glue);
  }

  if (VT != MVT::i8)
    Bool = DAG.getNode(ISD::ZERO_EXTEND, DL, VT, Bool);
  return Bool;
}

SDValue UG24TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  // A 16-bit branch goes straight to the two-compare sequence rather than
  // building a boolean and testing it.
  if (LHS.getValueType() == MVT::i16) {
    bool IsSigned = false;
    UG24CC::CondCode Cond = translateCC(CC, IsSigned);
    if (IsSigned) {
      LHS = flipSignBit(LHS, DL, DAG);
      RHS = flipSignBit(RHS, DL, DAG);
    }
    return DAG.getNode(UG24ISD::BR_CC16, DL, MVT::Other, Chain, Dest, LHS, RHS,
                       DAG.getConstant(Cond, DL, MVT::i8));
  }

  SDValue TargetCC;
  SDValue Glue = emitCompare(LHS, RHS, CC, DL, DAG, TargetCC);
  return DAG.getNode(UG24ISD::BR_CC, DL, MVT::Other, Chain, Dest, TargetCC,
                     Glue);
}

SDValue UG24TargetLowering::LowerSELECT_CC(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  if (LHS.getValueType() == MVT::i16) {
    SDValue Bool = LowerSETCC(
        DAG.getNode(ISD::SETCC, DL, MVT::i8, LHS, RHS, DAG.getCondCode(CC)),
        DAG);
    LHS = Bool;
    RHS = DAG.getConstant(0, DL, MVT::i8);
    CC = ISD::SETNE;
  }

  SDValue TargetCC;
  SDValue Glue = emitCompare(LHS, RHS, CC, DL, DAG, TargetCC);
  return DAG.getNode(UG24ISD::SELECT_CC, DL, Op.getValueType(), TrueV, FalseV,
                     TargetCC, Glue);
}

//===----------------------------------------------------------------------===//
// Addresses
//===----------------------------------------------------------------------===//

SDValue UG24TargetLowering::LowerGlobalAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  auto *N = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  SDValue Addr = DAG.getTargetGlobalAddress(N->getGlobal(), DL, MVT::i16,
                                            N->getOffset());
  return DAG.getNode(UG24ISD::WRAPPER, DL, MVT::i16, Addr);
}

SDValue UG24TargetLowering::LowerBlockAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
  auto *N = cast<BlockAddressSDNode>(Op);
  SDLoc DL(Op);
  SDValue Addr = DAG.getTargetBlockAddress(N->getBlockAddress(), MVT::i16);
  return DAG.getNode(UG24ISD::WRAPPER, DL, MVT::i16, Addr);
}

SDValue UG24TargetLowering::LowerExternalSymbol(SDValue Op,
                                                SelectionDAG &DAG) const {
  auto *N = cast<ExternalSymbolSDNode>(Op);
  SDLoc DL(Op);
  SDValue Addr = DAG.getTargetExternalSymbol(N->getSymbol(), MVT::i16);
  return DAG.getNode(UG24ISD::WRAPPER, DL, MVT::i16, Addr);
}

SDValue UG24TargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<UG24MachineFunctionInfo>();
  SDLoc DL(Op);

  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(DAG.getDataLayout()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

// Is \p V a 16-bit value that is really just a zero-extended byte?
static bool isZExtByte(SDValue V) {
  return V.getOpcode() == ISD::ZERO_EXTEND &&
         V.getOperand(0).getValueType() == MVT::i8;
}

/// The i8 value inside \p V when V is a 16-bit value that provably holds only
/// a byte, or an empty SDValue.  Two spellings reach here: an explicit
/// zero-extend, and `and x, 255`, which is what zext(trunc(x)) folds to -- the
/// shape a byte argument now arrives in, since the calling convention widens
/// bytes to 16 bits.  Missing the second spelling would send every
/// uint8_t * uint8_t to __mulhi3 instead of the hardware multiplier.
static SDValue getByteValue(SDValue V, const SDLoc &DL, SelectionDAG &DAG) {
  if (isZExtByte(V))
    return V.getOperand(0);

  if (V.getOpcode() == ISD::AND)
    if (auto *C = dyn_cast<ConstantSDNode>(V.getOperand(1)))
      if (C->getZExtValue() == 0xff)
        return DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, V.getOperand(0));

  return SDValue();
}

// The hardware multiplies two bytes into a 16-bit product.  When both
// operands are zero-extended bytes that is exactly what is wanted, and the
// MULW pattern selects the instruction; a genuinely 16-bit multiply has no
// hardware support and goes to the runtime helper.
SDValue UG24TargetLowering::LowerMUL(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);

  // The byte behind a widened 16-bit operand, in any of the shapes that can
  // reach here: an explicit zext from i8, a mask with 0xff -- which is what
  // zext-of-trunc folds to, because truncation is free on this target -- or a
  // constant that fits in a byte.
  auto byteOperand = [&](SDValue V) -> SDValue {
    if (isZExtByte(V))
      return V.getOperand(0);
    if (V.getOpcode() == ISD::AND)
      if (auto *C = dyn_cast<ConstantSDNode>(V.getOperand(1)))
        if (C->getZExtValue() == 0xff)
          return DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, V.getOperand(0));
    if (auto *C = dyn_cast<ConstantSDNode>(V))
      if (isUInt<8>(C->getZExtValue()))
        return DAG.getConstant(C->getZExtValue(), DL, MVT::i8);
    return SDValue();
  };

  // The widening multiply becomes a target node rather than another i16 MUL.
  // Returning a plain MUL here put legalisation and the DAG combiner in a
  // loop: the combiner flips the mask form to zext-of-trunc and back, and
  // every flip produced a fresh Custom MUL for LowerMUL to lower once more.
  SDValue L = byteOperand(LHS);
  SDValue R = byteOperand(RHS);
  if (L && R)
    return DAG.getNode(UG24ISD::MULW, DL, MVT::i16, L, R);

  SDValue Ops[] = {LHS, RHS};
  MakeLibCallOptions CallOptions;
  return makeLibCall(DAG, RTLIB::MUL_I16, MVT::i16, Ops, CallOptions, DL).first;
}

// A truncating 16 -> 8 store keeps only the low half of the pair.
SDValue UG24TargetLowering::LowerStore(SDValue Op, SelectionDAG &DAG) const {
  auto *ST = cast<StoreSDNode>(Op);
  if (!ST->isTruncatingStore())
    return Op;

  SDLoc DL(Op);
  SDValue Value = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, ST->getValue());
  return DAG.getStore(ST->getChain(), DL, Value, ST->getBasePtr(),
                      ST->getPointerInfo(), ST->getOriginalAlign(),
                      ST->getMemOperand()->getFlags());
}

// Shift an 8-bit value by a constant.  The hardware encodes amounts of 1..8,
// so anything larger collapses to zero or to a replicated sign bit.
static SDValue shiftByte(unsigned Opcode, SDValue Byte, unsigned Amount,
                         const SDLoc &DL, SelectionDAG &DAG) {
  if (Amount == 0)
    return Byte;
  if (Amount >= 8) {
    // Shifting a byte by its full width is undefined at the IR level and gets
    // folded to poison, so the sign is replicated with a shift of seven -
    // which leaves 0x00 or 0xff either way.
    if (Opcode == ISD::SRA)
      return DAG.getNode(ISD::SRA, DL, MVT::i8, Byte,
                         DAG.getConstant(7, DL, MVT::i8));
    return DAG.getConstant(0, DL, MVT::i8);
  }
  return DAG.getNode(Opcode, DL, MVT::i8, Byte,
                     DAG.getConstant(Amount, DL, MVT::i8));
}

SDValue UG24TargetLowering::LowerShift(SDValue Op, SelectionDAG &DAG) const {
  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  SDValue Value = Op.getOperand(0);
  SDValue Amount = Op.getOperand(1);
  unsigned Opcode = Op.getOpcode();

  auto *C = dyn_cast<ConstantSDNode>(Amount);

  if (VT == MVT::i8) {
    if (!C) {
      // A variable byte shift is done at 16 bits and truncated back.
      SDValue Wide = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i16, Value);
      if (Opcode == ISD::SRA)
        Wide = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i16, Value);
      else if (Opcode == ISD::SRL)
        Wide = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, Value);
      SDValue Wide16 = DAG.getNode(Opcode, DL, MVT::i16, Wide, Amount);
      return DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, Wide16);
    }
    unsigned N = C->getZExtValue() & 0xff;
    if (N >= 1 && N <= 8)
      return Op; // Selected directly by LSL / LSR / ASR.
    return shiftByte(Opcode, Value, N, DL, DAG);
  }

  assert(VT == MVT::i16 && "unexpected shift width");

  // A variable 16-bit shift goes to the runtime helper.
  if (!C) {
    RTLIB::Libcall LC = Opcode == ISD::SHL   ? RTLIB::SHL_I16
                        : Opcode == ISD::SRL ? RTLIB::SRL_I16
                                             : RTLIB::SRA_I16;
    SDValue Ops[] = {Value, Amount};
    MakeLibCallOptions CallOptions;
    CallOptions.setSExt(Opcode == ISD::SRA);
    return makeLibCall(DAG, LC, VT, Ops, CallOptions, DL).first;
  }

  unsigned N = C->getZExtValue() & 0xffff;
  if (N == 0)
    return Value;

  SDValue Lo = DAG.getNode(UG24ISD::LO8, DL, MVT::i8, Value);
  SDValue Hi = DAG.getNode(UG24ISD::HI8, DL, MVT::i8, Value);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i8);
  SDValue NewLo, NewHi;

  if (N >= 16) {
    if (Opcode == ISD::SRA) {
      SDValue Sign = shiftByte(ISD::SRA, Hi, 8, DL, DAG);
      NewLo = NewHi = Sign;
    } else {
      NewLo = NewHi = Zero;
    }
  } else if (Opcode == ISD::SHL) {
    if (N >= 8) {
      NewHi = shiftByte(ISD::SHL, Lo, N - 8, DL, DAG);
      NewLo = Zero;
    } else {
      // The bits leaving the low byte become the bottom of the high byte.
      SDValue Carry = shiftByte(ISD::SRL, Lo, 8 - N, DL, DAG);
      NewHi = DAG.getNode(ISD::OR, DL, MVT::i8,
                          shiftByte(ISD::SHL, Hi, N, DL, DAG), Carry);
      NewLo = shiftByte(ISD::SHL, Lo, N, DL, DAG);
    }
  } else { // SRL or SRA
    bool IsArith = Opcode == ISD::SRA;
    SDValue Fill = IsArith ? shiftByte(ISD::SRA, Hi, 8, DL, DAG) : Zero;
    if (N >= 8) {
      NewLo = shiftByte(IsArith ? ISD::SRA : ISD::SRL, Hi, N - 8, DL, DAG);
      NewHi = Fill;
    } else {
      SDValue Carry = shiftByte(ISD::SHL, Hi, 8 - N, DL, DAG);
      NewLo = DAG.getNode(ISD::OR, DL, MVT::i8,
                          shiftByte(ISD::SRL, Lo, N, DL, DAG), Carry);
      NewHi = shiftByte(IsArith ? ISD::SRA : ISD::SRL, Hi, N, DL, DAG);
    }
  }

  return DAG.getNode(UG24ISD::PAIR, DL, MVT::i16, NewLo, NewHi);
}

//===----------------------------------------------------------------------===//
// Calling convention
//===----------------------------------------------------------------------===//

SDValue UG24TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_UG24);

  for (const CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      MVT RegVT = VA.getLocVT();
      const TargetRegisterClass *RC =
          RegVT == MVT::i8 ? static_cast<const TargetRegisterClass *>(
                                 &UG24::GPRRegClass)
                           : static_cast<const TargetRegisterClass *>(
                                 &UG24::GPR16RegClass);
      Register VReg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue Value = DAG.getCopyFromReg(Chain, DL, VReg, RegVT);

      switch (VA.getLocInfo()) {
      case CCValAssign::Full:
        break;
      case CCValAssign::BCvt:
        Value = DAG.getNode(ISD::BITCAST, DL, VA.getValVT(), Value);
        break;
      case CCValAssign::SExt:
        Value = DAG.getNode(ISD::AssertSext, DL, RegVT, Value,
                            DAG.getValueType(VA.getValVT()));
        Value = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Value);
        break;
      case CCValAssign::ZExt:
        Value = DAG.getNode(ISD::AssertZext, DL, RegVT, Value,
                            DAG.getValueType(VA.getValVT()));
        Value = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Value);
        break;
      case CCValAssign::AExt:
        // Byte arguments are promoted to 16 bits; the upper half carries
        // nothing, so just narrow it back.
        Value = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Value);
        break;
      default:
        report_fatal_error("unhandled argument location on uG24");
      }

      InVals.push_back(Value);
      continue;
    }

    // Stack argument.  A promoted byte occupies a full 16-bit slot, so the
    // load has to be of the promoted width and then narrowed.
    assert(VA.isMemLoc() && "argument is neither in a register nor in memory");
    EVT LocVT = VA.getLocVT();
    int FI = MFI.CreateFixedObject(LocVT.getStoreSize(), VA.getLocMemOffset(),
                                   /*IsImmutable=*/true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
    SDValue Loaded = DAG.getLoad(LocVT, DL, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(MF, FI));
    if (LocVT != VA.getValVT())
      Loaded = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Loaded);
    InVals.push_back(Loaded);
  }

  if (IsVarArg) {
    auto *FuncInfo = MF.getInfo<UG24MachineFunctionInfo>();
    // Variadic arguments start immediately after the fixed ones.
    int FI = MFI.CreateFixedObject(1, CCInfo.getStackSize(), true);
    FuncInfo->setVarArgsFrameIndex(FI);
  }

  return Chain;
}

SDValue UG24TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();

  // Tail calls are not implemented.
  CLI.IsTailCall = false;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_UG24);

  unsigned NumBytes = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;
  SDValue StackPtr;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    switch (VA.getLocInfo()) {
    case CCValAssign::Full: break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::BCvt:
      Arg = DAG.getNode(ISD::BITCAST, DL, VA.getLocVT(), Arg);
      break;
    default:
      report_fatal_error("unhandled argument location on uG24");
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    assert(VA.isMemLoc());
    if (!StackPtr.getNode())
      StackPtr = DAG.getCopyFromReg(Chain, DL, UG24::SP, MVT::i16);
    SDValue Address =
        DAG.getNode(ISD::ADD, DL, MVT::i16, StackPtr,
                    DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));
    MemOpChains.push_back(
        DAG.getStore(Chain, DL, Arg, Address, MachinePointerInfo()));
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
    Glue = Chain.getValue(1);
  }

  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i16,
                                        G->getOffset());
  else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i16);

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

  const UG24RegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
  assert(Mask && "missing call-preserved register mask");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (Glue.getNode())
    Ops.push_back(Glue);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(UG24ISD::CALL, DL, NodeTys, Ops);
  Glue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  // Collect the return value.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_UG24);

  for (const CCValAssign &VA : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue)
                .getValue(1);
    Glue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}

bool UG24TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_UG24);
}

SDValue UG24TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_UG24);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "uG24 returns values in registers only");
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  return DAG.getNode(UG24ISD::RET_GLUE, DL, MVT::Other, RetOps);
}


//===----------------------------------------------------------------------===//
// Custom inserters
//===----------------------------------------------------------------------===//

// Lower a select into
//
//     BB ------[cond]------> SinkBB      (carrying the true value)
//       \-> FalseBB --------/            (carrying the false value)
//
// The conditional branch has to target the sink rather than a "true" block,
// because the block laid out immediately after BB is reached by falling
// through and so cannot be the taken path.
static MachineBasicBlock *emitSelect(MachineInstr &MI, MachineBasicBlock *BB,
                                     const UG24InstrInfo &TII) {
  const BasicBlock *LLVMBB = BB->getBasicBlock();
  MachineFunction *MF = BB->getParent();
  MachineFunction::iterator Insert = ++BB->getIterator();
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *FalseBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *SinkBB = MF->CreateMachineBasicBlock(LLVMBB);
  MF->insert(Insert, FalseBB);
  MF->insert(Insert, SinkBB);

  // Everything after the select belongs to the sink block.
  SinkBB->splice(SinkBB->begin(), BB, std::next(MachineBasicBlock::iterator(MI)),
                 BB->end());
  SinkBB->transferSuccessorsAndUpdatePHIs(BB);

  BB->addSuccessor(FalseBB);
  BB->addSuccessor(SinkBB);
  FalseBB->addSuccessor(SinkBB);

  auto CC = static_cast<UG24CC::CondCode>(MI.getOperand(3).getImm());
  BuildMI(BB, DL, TII.get(UG24::BRCC)).addMBB(SinkBB).addImm(CC);

  BuildMI(*SinkBB, SinkBB->begin(), DL, TII.get(UG24::PHI),
          MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg()) // true value, taken branch from BB
      .addMBB(BB)
      .addReg(MI.getOperand(2).getReg()) // false value, fallen through
      .addMBB(FalseBB);

  MI.eraseFromParent();
  return SinkBB;
}

// A 16-bit comparison: compare the high bytes; if they are equal fall through
// to a low-byte comparison, otherwise the high-byte result decides.  Leaves 0
// or 1 in the destination byte.
static MachineBasicBlock *emitSetCC16(MachineInstr &MI, MachineBasicBlock *BB,
                                      const UG24InstrInfo &TII) {
  const BasicBlock *LLVMBB = BB->getBasicBlock();
  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  MachineFunction::iterator Insert = ++BB->getIterator();
  DebugLoc DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  auto CC = static_cast<UG24CC::CondCode>(MI.getOperand(3).getImm());

  // The operands are still virtual at this point, so the halves are named
  // with sub-register indices on the machine operands rather than resolved to
  // physical registers.
  auto addLo = [&](MachineInstrBuilder &MIB, Register R) -> MachineInstrBuilder & {
    MIB.addReg(R, 0, sub_lo);
    return MIB;
  };
  auto addHi = [&](MachineInstrBuilder &MIB, Register R) -> MachineInstrBuilder & {
    MIB.addReg(R, 0, sub_hi);
    return MIB;
  };

  // When the high bytes differ, the ordering is decided there - but the
  // "or equal" part of <= and >= must not be applied to the high byte alone.
  UG24CC::CondCode HiCC = CC;
  switch (CC) {
  case UG24CC::COND_LE: HiCC = UG24CC::COND_LT; break;
  case UG24CC::COND_GE: HiCC = UG24CC::COND_GT; break;
  default: break;
  }

  MachineBasicBlock *LowBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *TrueBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *FalseBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *SinkBB = MF->CreateMachineBasicBlock(LLVMBB);
  MF->insert(Insert, LowBB);
  MF->insert(Insert, TrueBB);
  MF->insert(Insert, FalseBB);
  MF->insert(Insert, SinkBB);

  SinkBB->splice(SinkBB->begin(), BB, std::next(MachineBasicBlock::iterator(MI)),
                 BB->end());
  SinkBB->transferSuccessorsAndUpdatePHIs(BB);

  // Entry: compare the high bytes.
  {
    auto MIB = BuildMI(BB, DL, TII.get(UG24::CMP));
    addHi(MIB, LHS);
    addHi(MIB, RHS);
  }
  BuildMI(BB, DL, TII.get(UG24::BRCC)).addMBB(LowBB).addImm(UG24CC::COND_EQ);
  if (CC == UG24CC::COND_EQ) {
    // Equality can only hold when the high bytes match.
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(FalseBB);
  } else if (CC == UG24CC::COND_NE) {
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(TrueBB);
  } else {
    BuildMI(BB, DL, TII.get(UG24::BRCC)).addMBB(TrueBB).addImm(HiCC);
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(FalseBB);
  }
  BB->addSuccessor(LowBB);
  BB->addSuccessor(CC == UG24CC::COND_EQ ? FalseBB : TrueBB);
  if (CC != UG24CC::COND_EQ && CC != UG24CC::COND_NE)
    BB->addSuccessor(FalseBB);

  // High bytes equal: the low bytes decide, always unsigned.
  {
    auto MIB = BuildMI(LowBB, DL, TII.get(UG24::CMP));
    addLo(MIB, LHS);
    addLo(MIB, RHS);
  }
  BuildMI(LowBB, DL, TII.get(UG24::BRCC)).addMBB(TrueBB).addImm(CC);
  BuildMI(LowBB, DL, TII.get(UG24::JR)).addMBB(FalseBB);
  LowBB->addSuccessor(TrueBB);
  LowBB->addSuccessor(FalseBB);

  Register TrueReg = MRI.createVirtualRegister(&UG24::GPRRegClass);
  Register FalseReg = MRI.createVirtualRegister(&UG24::GPRRegClass);
  BuildMI(TrueBB, DL, TII.get(UG24::MVI), TrueReg).addImm(1);
  BuildMI(TrueBB, DL, TII.get(UG24::JR)).addMBB(SinkBB);
  TrueBB->addSuccessor(SinkBB);

  BuildMI(FalseBB, DL, TII.get(UG24::MVI), FalseReg).addImm(0);
  FalseBB->addSuccessor(SinkBB);

  BuildMI(*SinkBB, SinkBB->begin(), DL, TII.get(UG24::PHI), Dst)
      .addReg(TrueReg)
      .addMBB(TrueBB)
      .addReg(FalseReg)
      .addMBB(FalseBB);

  MI.eraseFromParent();
  return SinkBB;
}

// A 16-bit conditional branch:
//
//     BB:     cmp lhs.hi, rhs.hi
//             beq LowBB              ; high bytes equal, low bytes decide
//             b<hi cc> Dest          ; otherwise the high bytes decide
//             jr Fallthrough
//     LowBB:  cmp lhs.lo, rhs.lo
//             b<cc> Dest
//             (falls through to Fallthrough)
//
// Equality is special: it can only hold when the high bytes match, so the
// "high bytes differ" edge goes straight to one side or the other.
// Splitting a block behind LLVM's back leaves the PHI nodes in the successors
// still naming the original block.  When control can now arrive from either
// the original or the new block, every PHI entry naming the original needs a
// twin naming the new one; when the original no longer reaches the successor
// at all, the entry has to be renamed instead.  Getting this wrong produces a
// PHI whose incoming blocks disagree with the CFG, and PHI elimination then
// quietly replaces the missing value with IMPLICIT_DEF -- a function that
// returns garbage with no diagnostic anywhere.
static void addPhiEntryLike(MachineBasicBlock *Succ, MachineBasicBlock *From,
                            MachineBasicBlock *Also) {
  for (MachineInstr &Phi : Succ->phis()) {
    for (unsigned I = 1, E = Phi.getNumOperands(); I + 1 < E; I += 2) {
      if (Phi.getOperand(I + 1).getMBB() != From)
        continue;
      MachineOperand Value = Phi.getOperand(I);
      Phi.addOperand(MachineOperand::CreateReg(
          Value.getReg(), /*isDef=*/false, /*isImp=*/false, /*isKill=*/false,
          /*isDead=*/false, /*isUndef=*/false, /*isEarlyClobber=*/false,
          Value.getSubReg()));
      Phi.addOperand(MachineOperand::CreateMBB(Also));
      break;
    }
  }
}

static MachineBasicBlock *emitBrCC16(MachineInstr &MI, MachineBasicBlock *BB,
                                     const UG24InstrInfo &TII) {
  MachineFunction *MF = BB->getParent();
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *Dest = MI.getOperand(0).getMBB();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  auto CC = static_cast<UG24CC::CondCode>(MI.getOperand(3).getImm());

  // The not-taken edge is whichever successor is not the branch target.
  MachineBasicBlock *Fallthrough = nullptr;
  for (MachineBasicBlock *Succ : BB->successors())
    if (Succ != Dest)
      Fallthrough = Succ;

  // This pseudo and anything after it are the block's terminators, and are
  // being replaced wholesale.
  while (std::next(MachineBasicBlock::iterator(MI)) != BB->end())
    std::next(MachineBasicBlock::iterator(MI))->eraseFromParent();
  MI.eraseFromParent();

  if (!Fallthrough) {
    // Both edges lead to the same block, so the condition is irrelevant.
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(Dest);
    return BB;
  }

  MachineBasicBlock *LowBB = MF->CreateMachineBasicBlock(BB->getBasicBlock());
  MF->insert(++BB->getIterator(), LowBB);

  // "Less than or equal" and "greater than or equal" must drop the equal part
  // when applied to the high bytes alone: equal high bytes means the low
  // bytes have not been looked at yet.
  UG24CC::CondCode HiCC = CC;
  if (CC == UG24CC::COND_LE)
    HiCC = UG24CC::COND_LT;
  else if (CC == UG24CC::COND_GE)
    HiCC = UG24CC::COND_GT;

  auto Cmp = [&](MachineBasicBlock *In, unsigned SubIdx) {
    auto MIB = BuildMI(In, DL, TII.get(UG24::CMP));
    MIB.addReg(LHS, 0, SubIdx);
    MIB.addReg(RHS, 0, SubIdx);
  };

  BB->removeSuccessor(Dest);
  BB->removeSuccessor(Fallthrough);

  Cmp(BB, sub_hi);
  BuildMI(BB, DL, TII.get(UG24::BRCC)).addMBB(LowBB).addImm(UG24CC::COND_EQ);
  BB->addSuccessor(LowBB);

  if (CC == UG24CC::COND_EQ) {
    // Equal is impossible once the high bytes differ.
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(Fallthrough);
    BB->addSuccessor(Fallthrough);
  } else if (CC == UG24CC::COND_NE) {
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(Dest);
    BB->addSuccessor(Dest);
  } else {
    BuildMI(BB, DL, TII.get(UG24::BRCC)).addMBB(Dest).addImm(HiCC);
    BuildMI(BB, DL, TII.get(UG24::JR)).addMBB(Fallthrough);
    BB->addSuccessor(Dest);
    BB->addSuccessor(Fallthrough);
  }

  Cmp(LowBB, sub_lo);
  BuildMI(LowBB, DL, TII.get(UG24::BRCC)).addMBB(Dest).addImm(CC);
  BuildMI(LowBB, DL, TII.get(UG24::JR)).addMBB(Fallthrough);
  LowBB->addSuccessor(Dest);
  LowBB->addSuccessor(Fallthrough);

  // Both successors are now reachable through LowBB, so their PHIs have to be
  // told about it.  Where BB has stopped being a predecessor the entry moves
  // rather than multiplies.
  if (BB->isSuccessor(Dest))
    addPhiEntryLike(Dest, BB, LowBB);
  else
    Dest->replacePhiUsesWith(BB, LowBB);

  if (BB->isSuccessor(Fallthrough))
    addPhiEntryLike(Fallthrough, BB, LowBB);
  else
    Fallthrough->replacePhiUsesWith(BB, LowBB);

  return BB;
}

MachineBasicBlock *
UG24TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                MachineBasicBlock *BB) const {
  const auto &TII = *Subtarget.getInstrInfo();
  switch (MI.getOpcode()) {
  case UG24::Select8:
  case UG24::Select16:
    return emitSelect(MI, BB, TII);
  case UG24::SetCC16:
    return emitSetCC16(MI, BB, TII);
  case UG24::BrCC16:
    return emitBrCC16(MI, BB, TII);
  default:
    llvm_unreachable("unexpected instruction for the uG24 custom inserter");
  }
}
