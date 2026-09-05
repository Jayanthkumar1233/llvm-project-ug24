//===-- UG24CallingConv.td - Calling Conventions for UG24 -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The uG24 specification does not define a C ABI.  The convention below is
// the one documented in docs/uG24-assumptions.md.
//
//===----------------------------------------------------------------------===//

def CC_UG24 : CallingConv<[
  // Promote anything narrower than a byte to i8.
  CCIfType<[i1], CCPromoteToType<i8>>,

  // In a variadic call every argument travels on the stack, fixed ones
  // included.  The callee analyses only its declared parameters, so this is
  // the one arrangement in which both sides agree on where the variadic block
  // begins: immediately after the fixed arguments, in one contiguous run that
  // va_arg can walk.  Passing the first variadic argument in a register would
  // leave the callee reading from the second one onwards.
  CCIfVarArg<CCIfType<[i8],  CCAssignToStack<1, 1>>>,
  CCIfVarArg<CCIfType<[i16], CCAssignToStack<2, 1>>>,

  // 8-bit arguments go in R0-R3.
  CCIfType<[i8], CCAssignToReg<[R0, R1, R2, R3]>>,

  // 16-bit arguments and pointers go in the extended pairs.  DPTR0 is the
  // default data pointer, so it is used last.
  CCIfType<[i16], CCAssignToReg<[W, DPTR1]>>,

  // Everything else is passed on the stack.
  CCIfType<[i8],  CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;

def RetCC_UG24 : CallingConv<[
  CCIfType<[i1], CCPromoteToType<i8>>,
  CCIfType<[i8],  CCAssignToReg<[R0, R1]>>,
  CCIfType<[i16], CCAssignToReg<[W, DPTR1]>>
]>;

// R4-R7 and R10 are preserved across calls.  R11 and DPTR0 are reserved and
// so appear in neither list.
def CSR_UG24 : CalleeSavedRegs<(add R4, R5, R6, R7, R10)>;
