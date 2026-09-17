#!/bin/sh
# Compile, link and execute the uG24 end-to-end test on the simulator.
set -e

BUILD=${1:-build-ug24}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/$BUILD/bin"

if [ ! -x "$BIN/clang" ]; then
    echo "$0: no uG24 compiler at $BIN/clang" >&2
    echo "  build-ug24/ is a build tree and is deliberately not in the" >&2
    echo "  repository.  Build it first:  ./ug24-setup.sh" >&2
    exit 1
fi

SIM="$ROOT/ug24-sim/ug24sim"
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
