#!/bin/sh
# Produce the assembly and the encoded disassembly for a C file, for checking
# the compiler's output against the uG24 ISA spreadsheet.
#
#   ug24-asm.sh prog.c              whole file
#   ug24-asm.sh prog.c main         disassemble just one function
#   OPT=-O0 ug24-asm.sh prog.c      change the optimisation level
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



OPT=${OPT:--Os}
TRIPLE=ug24-unknown-none-eabi

SRC=$1
[ -n "$SRC" ] || { echo "usage: $0 prog.c [function]" >&2; exit 2; }
FUNC=$2
BASE=${SRC%.c}

# Compiler output: mnemonics, labels, directives and relocation modifiers.
"$BIN/clang" --target=$TRIPLE $OPT -S "$SRC" -o "$BASE.s"

# Linked image: the same code with its final addresses and encoded bytes.
"$BIN/clang" --target=$TRIPLE $OPT "$SRC" -o "$BASE.elf"
if [ -n "$FUNC" ]; then
    "$BIN/llvm-objdump" -d --triple=$TRIPLE \
        --disassemble-symbols="$FUNC" "$BASE.elf" > "$BASE.dis"
else
    "$BIN/llvm-objdump" -d --triple=$TRIPLE "$BASE.elf" > "$BASE.dis"
fi

printf '%s\n' "$BASE.s    assembly as the compiler emitted it"
printf '%s\n' "$BASE.dis  disassembly with addresses and encoded bytes"
