//===--- UG24.h - Declare UG24 target feature support -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares UG24 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_UG24_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_UG24_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY UG24TargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

public:
  UG24TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    TLSSupported = false;
    BigEndian = false;

    // The uG24 ALU is 8 bits wide with a 16-bit address space, so int and
    // pointers are 16 bits.  Everything is byte-aligned: the hardware has no
    // alignment requirement and the stack is byte granular.
    BoolWidth = BoolAlign = 8;
    IntWidth = 16;
    IntAlign = 8;
    LongWidth = 32;
    LongAlign = 8;
    LongLongWidth = 64;
    LongLongAlign = 8;
    PointerWidth = 16;
    PointerAlign = 8;
    SuitableAlign = 8;
    DefaultAlignForAttributeAligned = 8;

    // 32- and 64-bit floating point are emulated through compiler-rt.
    FloatWidth = 32;
    FloatAlign = 8;
    DoubleWidth = 32;
    DoubleAlign = 8;
    DoubleFormat = &llvm::APFloat::IEEEsingle();
    LongDoubleWidth = 32;
    LongDoubleAlign = 8;
    LongDoubleFormat = &llvm::APFloat::IEEEsingle();

    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
    IntMaxType = SignedLongLong;
    WCharType = SignedInt;
    WIntType = SignedInt;
    Char16Type = UnsignedShort;
    Char32Type = UnsignedLong;
    SigAtomicType = SignedChar;

    // Must stay in step with UG24TargetMachine::computeDataLayout.
    resetDataLayout("e"         // little endian
                    "-m:e"      // ELF name mangling
                    "-p:16:16"  // 16-bit pointers, 16-bit ABI alignment
                    "-i8:8"     // 8-bit integers, byte aligned
                    "-i16:8"    // 16-bit integers, byte aligned
                    "-a:8"      // aggregates byte aligned
                    "-n8:16"    // 8- and 16-bit native integer widths
                    "-S8"       // byte-aligned stack
    );
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return std::nullopt;
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  ArrayRef<Builtin::Info> getTargetBuiltins() const override {
    return std::nullopt;
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    switch (*Name) {
    case 'r': // an 8-bit general purpose register
    case 'w': // a 16-bit extended register pair
      Info.setAllowsRegister();
      return true;
    default:
      return false;
    }
  }

  std::string_view getClobbers() const override { return ""; }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "ug24";
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_UG24_H
