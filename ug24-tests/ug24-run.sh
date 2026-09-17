#!/bin/sh
# Compile, link and run a C file for the uG24, then print any globals named
# on the command line.
#
#   ug24-run.sh prog.c                     compile and run
#   ug24-run.sh prog.c results total       ... and dump those globals
#   OPT=-O0 ug24-run.sh prog.c             change the optimisation level
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

SIM="$ROOT/ug24-sim/ug24sim"
OPT=${OPT:--Os}

SRC=$1
[ -n "$SRC" ] || { echo "usage: $0 prog.c [global ...]" >&2; exit 2; }
shift

ELF=${SRC%.c}.elf
"$BIN/clang" --target=ug24-unknown-none-eabi $OPT "$SRC" -o "$ELF"

# The simulator writes its memory image into the current directory, so run it
# from wherever the executable landed.
WORK=$(cd "$(dirname "$ELF")" && pwd)
(cd "$WORK" && "$SIM" "$(basename "$ELF")" --dump)

# Print the requested globals, taking each one's length from the symbol table.
for NAME in "$@"; do
    LINE=$("$BIN/llvm-nm" --print-size "$ELF" | awk -v n="$NAME" '$NF==n {print $1, $2}')
    if [ -z "$LINE" ]; then
        echo "  $NAME: no such symbol in $ELF" >&2
        continue
    fi
    ADDR=${LINE% *}
    SIZE=${LINE#* }
    case $SIZE in ''|*[!0-9a-fA-F]*) SIZE=1 ;; esac
    printf "  %-12s @0x%s  %s\n" "$NAME" "$ADDR" \
        "$(xxd -s $((0x$ADDR)) -l $((0x$SIZE)) -p "$WORK/ug24-memory.bin")"
done
