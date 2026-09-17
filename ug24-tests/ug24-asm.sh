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
BIN="$ROOT/build-ug24/bin"

if [ ! -x "$BIN/clang" ]; then
    echo "$0: no uG24 compiler at $BIN/clang" >&2
    echo "  build-ug24/ is a build tree and is deliberately not in the" >&2
    echo "  repository.  Build it first:  ./ug24-setup.sh" >&2
    exit 1
fi

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
