#!/bin/sh
# Build the cross-check kit: five sample programs as uG24 ELF files, each with
# the console output and the instruction count our simulator produces, plus the
# platform contract.  Hand the whole directory to whoever is writing another
# simulator; nothing in it depends on this repository.
#
#   make-handoff.sh [output-directory]      default: ug24-handoff/
set -e

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
OUT=${1:-$ROOT/ug24-handoff}

if   [ -x "$ROOT/build-ug24/bin/clang" ];    then BIN="$ROOT/build-ug24/bin"
elif [ -x "$ROOT/../build-ug24/bin/clang" ]; then BIN="$ROOT/../build-ug24/bin"
else echo "$0: no uG24 compiler found; run ./ug24-setup.sh" >&2; exit 1
fi
SIM="$ROOT/ug24-sim/ug24sim"
[ -x "$SIM" ] || { echo "$0: no simulator at $SIM" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"

COMMIT=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)
BUILT=$(date -u +%Y-%m-%dT%H:%MZ)

for SRC in "$DIR"/src/*.c; do
    NAME=$(basename "$SRC" .c)
    cp "$SRC" "$OUT/$NAME.c"
    "$BIN/clang" --target=ug24-unknown-none-eabi -Os -Wall -Wextra \
        "$SRC" -o "$OUT/$NAME.elf"
    "$SIM" "$OUT/$NAME.elf" --quiet > "$OUT/$NAME.expected"
    # The instruction count is a second, independent check: the same image on a
    # correct simulator retires exactly the same instructions.
    "$SIM" "$OUT/$NAME.elf" 2>/dev/null | grep '^halted' \
        | sed 's/halted after /instructions: /' > "$OUT/$NAME.count"
done

# Cross-check every expected output against host gcc and glibc, which share no
# code with this toolchain.  Skipped silently when there is no host compiler.
if command -v gcc > /dev/null 2>&1; then
    HOSTTMP=$(mktemp -d); trap 'rm -rf "$HOSTTMP"' EXIT
    for SRC in "$DIR"/src/*.c; do
        NAME=$(basename "$SRC" .c)
        if gcc -w -o "$HOSTTMP/$NAME" "$SRC" 2> /dev/null &&
           "$HOSTTMP/$NAME" > "$HOSTTMP/$NAME.out" 2> /dev/null &&
           cmp -s "$HOSTTMP/$NAME.out" "$OUT/$NAME.expected"; then
            echo "  $NAME: agrees with host gcc and glibc"
        else
            echo "  $NAME: DIFFERS from host gcc -- check before sending" >&2
            diff "$HOSTTMP/$NAME.out" "$OUT/$NAME.expected" >&2 || true
        fi
    done
fi

cp "$ROOT/docs/uG24-platform.md" "$OUT/"

# README, with the numbers filled in from the build that just happened.
{
    sed "s/@COMMIT@/$COMMIT/; s/@BUILT@/$BUILT/" "$DIR/README.in"
    echo
    echo '## The samples'
    echo
    echo '| Program | Console output | Instructions | What it exercises |'
    echo '| :--- | :--- | ---: | :--- |'
    for SRC in "$DIR"/src/*.c; do
        NAME=$(basename "$SRC" .c)
        LINES=$(wc -l < "$OUT/$NAME.expected" | tr -d ' ')
        if [ "$LINES" = 1 ]; then UNIT=line; else UNIT=lines; fi
        COUNT=$(sed 's/instructions: //; s/ instructions//' "$OUT/$NAME.count")
        case $NAME in
        01_hello)    WHAT='the console and the halt, nothing else' ;;
        02_integers) WHAT='the ALU, the flags, multiply/divide/shift helpers' ;;
        03_control)  WHAT='branches both ways, calls, recursion, function pointers' ;;
        04_memory)   WHAT='.data copied to its run address, .bss zeroed, the heap' ;;
        05_float)    WHAT='soft float end to end, the heaviest program here' ;;
        # A program dropped into src/ after the fact: describe it from the
        # first line of its leading comment when it has one, so the table stays
        # readable without this script having to know about it.
        *)           WHAT=$(sed -n 's|^/\* *[0-9A-Za-z_]* *- *||p' "$SRC" | head -1)
                     [ -n "$WHAT" ] || WHAT='added to the kit locally' ;;
        esac
        printf '| `%s` | %s %s | %s | %s |\n' "$NAME" "$LINES" "$UNIT" \
               "$COUNT" "$WHAT"
    done
} > "$OUT/README.md"

( cd "$OUT" && sha256sum *.elf > SHA256SUMS )

echo "kit in $OUT"
ls "$OUT"
