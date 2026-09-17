#!/bin/sh
# Produce every stage of the uG24 toolchain for a C file.
#
#   ug24-build.sh prog.c            -> prog.ll  prog.s  prog.o  prog.elf  prog.dis
#   OPT=-O0 ug24-build.sh prog.c    change the optimisation level
#
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CLANG="$ROOT/build-ug24/bin/clang"
OBJDUMP="$ROOT/build-ug24/bin/llvm-objdump"
TRIPLE=ug24-unknown-none-eabi
OPT=${OPT:--Os}

SRC=$1
[ -n "$SRC" ] || { echo "usage: $0 prog.c" >&2; exit 2; }
B=${SRC%.c}

"$CLANG" --target=$TRIPLE $OPT -S -emit-llvm "$SRC" -o "$B.ll"   # LLVM IR
"$CLANG" --target=$TRIPLE $OPT -S            "$SRC" -o "$B.s"    # uG24 assembly
"$CLANG" --target=$TRIPLE $OPT -c            "$SRC" -o "$B.o"    # object
"$CLANG" --target=$TRIPLE $OPT               "$SRC" -o "$B.elf"  # executable
"$OBJDUMP" -d --triple=$TRIPLE "$B.elf" > "$B.dis"               # disassembly

ls -l "$B".ll "$B".s "$B".o "$B".elf "$B".dis |
    awk '{printf "  %-14s %7s bytes\n", $9, $5}'
