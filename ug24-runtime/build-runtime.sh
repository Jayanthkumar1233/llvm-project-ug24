#!/bin/sh
# Build the uG24 startup file, helper library and linker script into the
# toolchain's lib/ug24 directory, which is where the clang driver looks.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)

# The argument may be an absolute path or a directory name relative to the
# repository root; UG24_BUILD overrides it.  Falls back to the directory above
# the repository, which is where the older layout kept the build tree.
BUILD=${1:-${UG24_BUILD:-build-ug24}}
case "$BUILD" in
    /*) BUILD_DIR=$BUILD ;;
    *)  if [ -d "$ROOT/$BUILD" ]; then BUILD_DIR="$ROOT/$BUILD"
        else BUILD_DIR="$ROOT/../$BUILD"; fi ;;
esac
BIN="$BUILD_DIR/bin"
OUT="$BUILD_DIR/lib/ug24"

[ -x "$BIN/clang" ] || {
    echo "$0: no uG24 clang at $BIN/clang" >&2
    exit 1
}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$OUT"

SOURCES="ug24_builtins ug24_int64 ug24_float ug24_printf_float ug24_printf_u64 ug24_u64dec ug24_uart ug24_io ug24_stdio ug24_stdlib"

# Build the C part of the library once per CPU variant.  The second variant
# exists because -mcpu=ug24-base only keeps MUL and DIV out of the code the
# compiler generates for the program: __mulsi3 and its relatives are library
# code, and an archive built for the default CPU would put a MUL back into the
# image of a program targeting a part that has no multiplier.
#
# -ffreestanding -fno-builtin keeps the optimiser from turning the helper
# loops back into calls to the very helpers being defined.
build_variant() {                       # $1 = output archive, $2... = extra flags
    OUTLIB=$1; shift
    VARTMP="$TMP/$(basename "$OUTLIB" .a)"
    mkdir -p "$VARTMP"
    OBJS=""
    for SRC in $SOURCES; do
        # -ffunction-sections/-fdata-sections give each routine its own section,
        # so a link with --gc-sections can drop the ones a program never calls.
        # Without them the archive member is the unit of linking and one call to
        # printf drags in the whole formatter and the 32-bit arithmetic behind
        # it: 15102 bytes for "Hello ug24", against 234 with them.
        "$BIN/clang" --target=ug24-unknown-elf -Os -ffreestanding -fno-builtin \
            -ffunction-sections -fdata-sections "$@" \
            -I "$ROOT/ug24-runtime/include" \
            -c "$ROOT/ug24-runtime/$SRC.c" -o "$VARTMP/$SRC.o"
        OBJS="$OBJS $VARTMP/$SRC.o"
    done
    # setjmp has to be assembly: a C function cannot see the return address the
    # call left in RA, nor the stack pointer its own prologue has already moved.
    # It contains no multiply, so one copy serves both variants.
    "$BIN/llvm-ar" rcs "$OUT/$OUTLIB" $OBJS "$TMP/ug24_setjmp.o" \
        "$TMP/ug24_platform.o"
}
"$BIN/llvm-mc" -triple=ug24-unknown-elf -filetype=obj \
    "$ROOT/ug24-runtime/ug24_setjmp.s" -o "$TMP/ug24_setjmp.o"

# Weak absolute definitions of the peripheral map, for a program linked with
# its own script instead of ug24.ld.  The stock script's assignments override
# them, so with it this member is never extracted.
"$BIN/llvm-mc" -triple=ug24-unknown-elf -filetype=obj \
    "$ROOT/ug24-runtime/ug24_platform.s" -o "$TMP/ug24_platform.o"

build_variant libug24.a
build_variant libug24-base.a -mcpu=ug24-base

"$BIN/llvm-mc" -triple=ug24-unknown-elf -filetype=obj \
    "$ROOT/ug24-runtime/crt0.s" -o "$OUT/crt0.o"

cp "$ROOT/ug24-runtime/ug24.ld" "$OUT/ug24.ld"

# Headers go where the clang driver adds them to the include path.
mkdir -p "$OUT/include"
cp "$ROOT/ug24-runtime/include/"*.h "$OUT/include/"

echo "uG24 runtime installed in $OUT"
