#!/bin/sh
# Build and run a C file with one of the three ARM cross toolchains.
#
#   arm-run.sh armhf     hello.c    32-bit ARM Linux, runs under qemu-arm
#   arm-run.sh arm64     hello.c    64-bit ARM Linux, runs under qemu-aarch64
#   arm-run.sh none-eabi hello.c    bare-metal Cortex-M4, builds only
#
set -e

ROOT=$HOME/llvm-arm-cross
TARGET=$1
SRC=$2
[ -n "$SRC" ] || { echo "usage: $0 {armhf|arm64|none-eabi} prog.c" >&2; exit 2; }
B=${SRC%.c}

case "$TARGET" in
armhf)
    G=/usr/lib/gcc-cross/arm-linux-gnueabihf/13
    "$ROOT/toolchain-armhf/bin/clang" --target=arm-linux-gnueabihf -O2 -static \
        -DARCHNAME='"ARM32 (armhf)"' "$SRC" -o "$B-armhf" -B"$G" -L"$G"
    echo "built $B-armhf"
    qemu-arm "./$B-armhf"
    ;;
arm64)
    G=/usr/lib/gcc-cross/aarch64-linux-gnu/13
    "$ROOT/toolchain-arm64/bin/clang" --target=aarch64-linux-gnu \
        --sysroot=/usr/aarch64-linux-gnu -O2 -static \
        -DARCHNAME='"ARM64 (aarch64)"' "$SRC" -o "$B-arm64" -B"$G" -L"$G"
    echo "built $B-arm64"
    qemu-aarch64 "./$B-arm64"
    ;;
none-eabi)
    # Compile with our clang, then link with arm-none-eabi-gcc, which supplies
    # newlib and the semihosting startup files that our toolchain has not got.
    "$ROOT/toolchain-none-eabi/bin/clang" --target=arm-none-eabi \
        -mcpu=cortex-m4 -mthumb -O2 -DARCHNAME='"ARM none-eabi"' \
        -isystem /usr/lib/arm-none-eabi/include -c "$SRC" -o "$B-m4.o"
    arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb "$B-m4.o" -o "$B-m4.elf" \
        --specs=rdimon.specs 2>/dev/null
    echo "built $B-m4.elf"
    if command -v qemu-system-arm >/dev/null; then
        qemu-system-arm -M mps2-an386 -cpu cortex-m4 -nographic \
            -semihosting -kernel "$B-m4.elf"
    else
        echo "not run: qemu-system-arm is not installed (see notes)"
    fi
    ;;
*)
    echo "unknown target '$TARGET' — use armhf, arm64 or none-eabi" >&2
    exit 2
    ;;
esac
