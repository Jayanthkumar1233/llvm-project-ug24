//===--- UG24.h - uG24 Tool and ToolChain Implementations -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_UG24_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_UG24_H

#include "Gnu.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {

namespace tools {
namespace ug24 {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("ug24::Linker", "ld.lld", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // namespace ug24
} // namespace tools

namespace toolchains {

/// Bare-metal toolchain for the uG24 microprocessor.  There is no operating
/// system and no GCC installation to find: the runtime is the small
/// libug24 shipped alongside the compiler, and linking always goes through
/// lld.
class LLVM_LIBRARY_VISIBILITY UG24ToolChain final : public ToolChain {
public:
  UG24ToolChain(const Driver &D, const llvm::Triple &Triple,
                const llvm::opt::ArgList &Args);

  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &) const override { return false; }
  bool isPICDefaultForced() const override { return true; }
  bool IsIntegratedAssemblerDefault() const override { return true; }
  bool SupportsProfiling() const override { return false; }
  bool HasNativeLLVMSupport() const override { return true; }

  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &) const override {
    return UnwindTableLevel::None;
  }

  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }
  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }
  UnwindLibType GetUnwindLibType(const llvm::opt::ArgList &) const override {
    return ToolChain::UNW_None;
  }

  const char *getDefaultLinker() const override { return "ld.lld"; }

  void AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                                 llvm::opt::ArgStringList &CC1Args) const override;

  /// Directory holding crt0.o, libug24.a and ug24.ld.
  std::string getRuntimeDir() const;

protected:
  Tool *buildLinker() const override;
};

} // namespace toolchains
} // namespace driver
} // namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_UG24_H
