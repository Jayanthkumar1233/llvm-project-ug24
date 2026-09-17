#!/bin/sh
# Compile, run and diff each program in this directory against its .expected
# output, at every optimisation level.
#
#   run-suite.sh            all programs, all levels
#   run-suite.sh t2_string  one program, all levels
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
BIN="$ROOT/build-ug24/bin"

if [ ! -x "$BIN/clang" ]; then
    echo "$0: no uG24 compiler at $BIN/clang" >&2
    echo "  build-ug24/ is a build tree and is deliberately not in the" >&2
    echo "  repository.  Build it first:  ./ug24-setup.sh" >&2
    exit 1
fi

SIM="$ROOT/ug24-sim/ug24sim"
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
