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



SIM="$SIM_BIN"
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
