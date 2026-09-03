//===-- UG24.td - Target Definition for UG24 -------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "UG24RegisterInfo.td"
include "UG24CallingConv.td"
include "UG24InstrInfo.td"

def UG24InstrInfo : InstrInfo;

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
