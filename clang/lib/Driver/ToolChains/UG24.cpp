//===--- UG24.cpp - uG24 ToolChain Implementations --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

UG24ToolChain::UG24ToolChain(const Driver &D, const llvm::Triple &Triple,
                             const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);

  std::string RuntimeDir = getRuntimeDir();
  if (!RuntimeDir.empty())
    getFilePaths().push_back(RuntimeDir);
}

// The runtime lives next to the compiler, in <prefix>/lib/ug24.
std::string UG24ToolChain::getRuntimeDir() const {
  llvm::SmallString<128> Dir(getDriver().Dir);
  llvm::sys::path::append(Dir, "..", "lib", "ug24");
  return std::string(Dir);
}

void UG24ToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                              ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc) ||
      DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // There is no host libc that makes sense for a 16-bit bare-metal target, so
  // /usr/include must never be searched: its headers would compile and then
  // fail to link, or worse, silently describe the wrong machine.  Only the
  // uG24 headers and clang's own resource headers are visible.
  CC1Args.push_back("-nostdsysteminc");

  llvm::SmallString<128> Dir(getRuntimeDir());
  llvm::sys::path::append(Dir, "include");
  if (llvm::sys::fs::exists(Dir))
    addSystemInclude(DriverArgs, CC1Args, Dir);
}

Tool *UG24ToolChain::buildLinker() const {
  return new tools::ug24::Linker(*this);
}

void ug24::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {
  const auto &TC = static_cast<const UG24ToolChain &>(getToolChain());
  const Driver &D = TC.getDriver();
  ArgStringList CmdArgs;

  // lld needs to be told which ELF flavour to produce; there is only one for
  // this target.
  CmdArgs.push_back("-m");
  CmdArgs.push_back("elf32ug24");

  std::string RuntimeDir = TC.getRuntimeDir();

  // Unless the user supplied their own script, use the default memory layout.
  if (!Args.hasArg(options::OPT_T) && !Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nostartfiles)) {
    llvm::SmallString<128> Script(RuntimeDir);
    llvm::sys::path::append(Script, "ug24.ld");
    if (llvm::sys::fs::exists(Script)) {
      CmdArgs.push_back("-T");
      CmdArgs.push_back(Args.MakeArgString(Script));
    }
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles))
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt0.o")));

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  TC.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs,
                  {options::OPT_T_Group, options::OPT_s, options::OPT_t});

  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs))
    CmdArgs.push_back("-lug24");

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));
  (void)D;
}
