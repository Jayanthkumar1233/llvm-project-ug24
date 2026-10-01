//===-- UG24CallingConv.td - Calling Conventions for UG24 -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The ABI confirmed by the hardware team:
//
//   arguments     R0, R1, R2, R3 -- four bytes in all, the rest on the stack
//   returns       8-bit in R0, 16-bit in X0 (R0:R1), 32-bit in X0:X1 (R0:R3),
//                 anything wider through a hidden pointer the caller passes
//   caller-saved  R0-R5
//   callee-saved  R6-R11
//   reserved      R12:R13 (DPTR1), R14:R15 (DPTR0), PC, RA, PSW, SP
//
// A 16-bit value occupies a pair, and the pairs are even-aligned, so a byte
// followed by an int lands in R0 and then R2:R3 rather than R1:R2.  The
// specification does not say whether byte arguments pack; even alignment is
// what the register pairs can express.
//
// See docs/uG24-assumptions.md for what is confirmed and what is still ours.
//
//===----------------------------------------------------------------------===//

def CC_UG24 : CallingConv<[
  // Promote anything narrower than a byte to i8.
  CCIfType<[i1], CCPromoteToType<i8>>,

  // Byte arguments take R0-R3 in order.
  CCIfType<[i8], CCAssignToReg<[R0, R1, R2, R3]>>,

  // 16-bit arguments and pointers take the two argument pairs.  Allocating
  // either member of a pair marks the pair, and vice versa, so a mixture of
  // byte and 16-bit arguments cannot overlap.
  CCIfType<[i16], CCAssignToReg<[P0, P1]>>,

  // Everything past four bytes goes on the stack.  A value wider than 32 bits
  // is legalised into 16-bit pieces first, so it can start in the pairs and
  // continue on the stack.
  CCIfType<[i8],  CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;

// Variadic arguments always travel on the stack, even when a register is free.
// The fixed arguments still use CC_UG24 above, so the callee -- which knows
// only its declared parameters -- finds the variadic block as one contiguous
// run starting at the first stack slot, which is what va_arg walks.
def CC_UG24_VarArg : CallingConv<[
  CCIfType<[i1], CCPromoteToType<i8>>,
  CCIfType<[i8],  CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;

def RetCC_UG24 : CallingConv<[
  CCIfType<[i1], CCPromoteToType<i8>>,

  // R1 is here for a two-byte value that reached this point as two i8 pieces;
  // R0 alone is what the ABI specifies for a single byte, and R0:R1 is X0
  // either way.
  CCIfType<[i8],  CCAssignToReg<[R0, R1]>>,

  // X0 for 16 bits, X0:X1 for 32.  Nothing wider is returned in registers:
  // clang gives a return type over four bytes a hidden pointer instead, so a
  // 64-bit value never reaches this list.
  CCIfType<[i16], CCAssignToReg<[P0, P1]>>
]>;

def CSR_UG24 : CalleeSavedRegs<(add R6, R7, R8, R9, R10, R11)>;
