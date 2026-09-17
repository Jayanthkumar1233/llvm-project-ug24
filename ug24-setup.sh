#!/bin/sh
# Install the uG24 runtime, simulator and test scripts after the LLVM build.
#
# Run it from the top of this repository once `ninja` has produced a uG24
# clang.  The build tree may be here (build-ug24/) or in the directory above,
# which is where the older layout put it; UG24_BUILD overrides both.
#
#   ./ug24-setup.sh [build-dir]      default build dir: build-ug24
#
set -e

ROOT=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-build-ug24}

if [ -n "$UG24_BUILD" ]; then
    BUILD_DIR=$UG24_BUILD
elif [ -x "$ROOT/$BUILD/bin/clang" ]; then
    BUILD_DIR="$ROOT/$BUILD"
elif [ -x "$ROOT/../$BUILD/bin/clang" ]; then
    BUILD_DIR=$(cd "$ROOT/../$BUILD" && pwd)
else
    echo "error: no uG24 clang in $ROOT/$BUILD/bin or $ROOT/../$BUILD/bin" >&2
    echo "Build the toolchain first - see docs/handbook/build-from-scratch.html," >&2
    echo "or set UG24_BUILD=/path/to/build-ug24." >&2
    exit 1
fi

echo "==> building the uG24 runtime (crt0, libug24, linker script, headers)"
"$ROOT/ug24-runtime/build-runtime.sh" "$BUILD_DIR"

echo "==> building the simulator"
cc -O2 -Wall -o "$ROOT/ug24-sim/ug24sim" "$ROOT/ug24-sim/ug24sim.c"
echo "    $ROOT/ug24-sim/ug24sim"

echo "==> running the end-to-end test suite"
UG24_BUILD="$BUILD_DIR" "$ROOT/ug24-tests/run-tests.sh"

cat <<DONE

uG24 toolchain ready.

  compile and link   $BUILD_DIR/bin/clang --target=ug24-unknown-none-eabi -Os hello.c -o hello.elf
  run                ug24-sim/ug24sim hello.elf --quiet
  all build stages   ug24-tests/ug24-build.sh hello.c
DONE
