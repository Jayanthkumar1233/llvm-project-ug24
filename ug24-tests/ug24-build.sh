#!/bin/sh
# Produce every stage of the uG24 toolchain for a C file.
#
#   ug24-build.sh prog.c            -> prog.ll  prog.s  prog.o  prog.elf  prog.dis
#   OPT=-O0 ug24-build.sh prog.c    change the optimisation level
#
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)

# Locate the uG24 toolchain.  It may be built inside this repository
# (build-ug24/) or in the directory above it, which is where the older
# layout put it.  UG24_BUILD overrides both.
if [ -n "$UG24_BUILD" ]; then
    BIN="$UG24_BUILD/bin"
elif [ -x "$BIN/clang" ]; then
    BIN="$ROOT/build-ug24/bin"
elif [ -x "$ROOT/../build-ug24/bin/clang" ]; then
    BIN="$ROOT/../build-ug24/bin"
else
    echo "$0: no uG24 compiler found" >&2
    echo "  looked in $ROOT/build-ug24/bin and $ROOT/../build-ug24/bin" >&2
    echo "  build it with ./ug24-setup.sh, or set UG24_BUILD=/path/to/build" >&2
    exit 1
fi
SIM_BIN="$SIM_BIN"
[ -x "$SIM_BIN" ] || SIM_BIN="$ROOT/../ug24-sim/ug24sim"

CLANG="$BIN/clang"


OBJDUMP="$BIN/llvm-objdump"
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
