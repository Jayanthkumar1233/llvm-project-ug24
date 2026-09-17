#!/bin/sh
# Install the uG24 runtime, simulator and test scripts after the LLVM build.
#
# Run this from the top of the working directory (the one containing
# llvm-project/ and build-ug24/) once `ninja` has produced build-ug24/bin/clang.
#
#   ./ug24-setup.sh [build-dir]      default build dir: build-ug24
#
set -e

ROOT=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-build-ug24}

[ -x "$ROOT/$BUILD/bin/clang" ] || {
    echo "error: $ROOT/$BUILD/bin/clang not found." >&2
    echo "Build the uG24 toolchain first - see the handbook, section 6." >&2
    exit 1
}

echo "==> building the uG24 runtime (crt0, libug24, linker script, headers)"
"$ROOT/ug24-runtime/build-runtime.sh" "$BUILD"

echo "==> building the simulator"
cc -O2 -Wall -o "$ROOT/ug24-sim/ug24sim" "$ROOT/ug24-sim/ug24sim.c"
echo "    $ROOT/ug24-sim/ug24sim"

echo "==> running the end-to-end test suite"
"$ROOT/ug24-tests/run-tests.sh" "$BUILD"

cat <<DONE

uG24 toolchain ready.

  compile and link   $BUILD/bin/clang --target=ug24-unknown-none-eabi -Os hello.c -o hello.elf
  run                ug24-sim/ug24sim hello.elf --quiet
  all build stages   ug24-tests/ug24-build.sh hello.c
DONE
