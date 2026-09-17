#!/bin/sh
# Build the uG24 startup file, helper library and linker script into the
# toolchain's lib/ug24 directory, which is where the clang driver looks.
set -e

BUILD=${1:-build-ug24}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/$BUILD/bin"
OUT="$ROOT/$BUILD/lib/ug24"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$OUT"

# -ffreestanding -fno-builtin keeps the optimiser from turning the helper
# loops back into calls to the very helpers being defined.
for SRC in ug24_builtins ug24_io ug24_stdio ug24_stdlib; do
    # -ffunction-sections/-fdata-sections give each routine its own section, so
    # a link with --gc-sections can drop the ones a program never calls.
    # Without them the archive member is the unit of linking and one call to
    # printf drags in the whole formatter and the 32-bit arithmetic behind it:
    # 15102 bytes for "Hello ug24", against 234 with them.
    "$BIN/clang" --target=ug24-unknown-none-eabi -Os -ffreestanding -fno-builtin \
        -ffunction-sections -fdata-sections \
        -I "$ROOT/ug24-runtime/include" \
        -c "$ROOT/ug24-runtime/$SRC.c" -o "$TMP/$SRC.o"
done
"$BIN/llvm-ar" rcs "$OUT/libug24.a" "$TMP/ug24_builtins.o" "$TMP/ug24_io.o" \
    "$TMP/ug24_stdio.o" "$TMP/ug24_stdlib.o"

"$BIN/llvm-mc" -triple=ug24-unknown-none-eabi -filetype=obj \
    "$ROOT/ug24-runtime/crt0.s" -o "$OUT/crt0.o"

cp "$ROOT/ug24-runtime/ug24.ld" "$OUT/ug24.ld"

# Headers go where the clang driver adds them to the include path.
mkdir -p "$OUT/include"
cp "$ROOT/ug24-runtime/include/"*.h "$OUT/include/"

echo "uG24 runtime installed in $OUT"
