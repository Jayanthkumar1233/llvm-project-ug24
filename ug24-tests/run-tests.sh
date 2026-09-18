#!/bin/sh
# Compile, link and execute the uG24 end-to-end test on the simulator.
set -e

BUILD=${1:-build-ug24}
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
# The simulator in this repository wins over any older copy beside it.
[ -x "$SIM_BIN" ] || SIM_BIN="$ROOT/ug24-sim/ug24sim"
[ -x "$SIM_BIN" ] || SIM_BIN="$ROOT/../ug24-sim/ug24sim"



SIM="$SIM_BIN"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

"$BIN/clang" --target=ug24-unknown-none-eabi -Os \
    "$ROOT/ug24-tests/test_compiler.c" -o "$TMP/test.elf"

cd "$TMP"
OUT=$("$SIM" "$TMP/test.elf" --dump)
echo "$OUT"

# main returns the number of failed cases in W.
FAILS=$(echo "$OUT" | sed -n 's/^return value: r0=[0-9]*  *w=\([0-9]*\)$/\1/p')
CASES=$("$BIN/llvm-nm" "$TMP/test.elf" | sed -n 's/^0*\([0-9a-f]*\) . ncases$/\1/p')
RAN=$(xxd -s $((0x$CASES)) -l 1 -p "$TMP/ug24-memory.bin")

echo
echo "cases run: $((0x$RAN)), failures: $FAILS"
[ "$FAILS" = "0" ] || { echo "FAIL"; exit 1; }
echo "PASS"
