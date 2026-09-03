//===-- UG24Subtarget.cpp - UG24 Subtarget Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UG24Subtarget.h"
#include "UG24.h"

#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_TARGET_DATA
static constexpr std::nullopt_t None = std::nullopt;
#include "UG24GenSubtargetInfo.inc"

using namespace llvm;

UG24Subtarget::UG24Subtarget(const Triple &TT, const std::string &CPU,
                             const std::string &FS, const TargetMachine &TM)
    : UG24GenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(), FrameLowering(), TLInfo(TM, *this), RegInfo() {}
