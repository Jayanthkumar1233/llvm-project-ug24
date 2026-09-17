#!/bin/sh
# Compile, run and diff each program in this directory against its .expected
# output, at every optimisation level.
#
#   run-suite.sh            all programs, all levels
#   run-suite.sh t2_string  one program, all levels
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)

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
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT

FAIL=0
for SRC in "$DIR"/${1:-*}.c; do
    NAME=$(basename "$SRC" .c)
    printf '%-14s' "$NAME"
    for OPT in -O0 -O1 -O2 -Os -O3; do
        if ! "$BIN/clang" --target=ug24-unknown-none-eabi $OPT "$SRC" \
                -o "$WORK/$NAME.elf" > "$WORK/build.log" 2>&1; then
            printf ' %s:BUILD' "$OPT"; FAIL=$((FAIL+1)); continue
        fi
        ( cd "$WORK" && "$SIM" --quiet --max 20000000 "$NAME.elf" ) > "$WORK/out.txt" 2>&1 || true
        if diff -q "$WORK/out.txt" "$DIR/$NAME.expected" > /dev/null 2>&1; then
            printf ' %s:ok' "$OPT"
        else
            printf ' %s:DIFF' "$OPT"; FAIL=$((FAIL+1))
            cp "$WORK/out.txt" "$DIR/$NAME$OPT.actual"
        fi
    done
    echo
done
echo
[ "$FAIL" -eq 0 ] && echo "all programs match at all levels" || echo "$FAIL mismatches (see *.actual)"
exit $FAIL
