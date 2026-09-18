//===-- UG24.td - Target Definition for UG24 -------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Subtarget features
//===----------------------------------------------------------------------===//
//
// The multiplier and the divider are optional blocks in the uG24 SoC
// configuration.  Both are present on the part this toolchain was written
// against, so they default to on; -mattr=-mul,-div compiles for a part built
// without them, and the wider arithmetic then goes through the same runtime
// helpers that 16- and 32-bit operations already use.  Those helpers are
// shift-and-add loops, so they need no hardware block themselves.

def FeatureMul : SubtargetFeature<"mul", "HasMul", "true",
                                  "Has the 8x8 -> 16 hardware multiplier">;

def FeatureDiv : SubtargetFeature<"div", "HasDiv", "true",
                                  "Has the 8-bit hardware divider">;

def HasMul : Predicate<"Subtarget->hasMul()">,
             AssemblerPredicate<(all_of FeatureMul), "the multiplier">;

def HasDiv : Predicate<"Subtarget->hasDiv()">,
             AssemblerPredicate<(all_of FeatureDiv), "the divider">;

include "UG24RegisterInfo.td"
include "UG24CallingConv.td"
include "UG24InstrInfo.td"

def UG24InstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Processors
//===----------------------------------------------------------------------===//

// "generic" is the fully-populated part.  "ug24-base" is the same core with
// neither optional arithmetic block, which is what -mcpu selects when a
// customer's SoC leaves them out.
def : ProcessorModel<"generic",   NoSchedModel, [FeatureMul, FeatureDiv]>;
def : ProcessorModel<"ug24",      NoSchedModel, [FeatureMul, FeatureDiv]>;
def : ProcessorModel<"ug24-base", NoSchedModel, []>;

def UG24AsmParser : AsmParser {
  let ShouldEmitMatchRegisterName = 1;
}

def UG24AsmWriter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  int Variant = 0;
}

def UG24 : Target {
  let InstructionSet = UG24InstrInfo;
  let AssemblyParsers = [UG24AsmParser];
  let AssemblyWriters = [UG24AsmWriter];
}
