# Building the uG24 cross compiler on a new machine

Everything needed to go from a bare Linux machine to a working uG24 toolchain,
and nothing else. This repository also carries the remains of some ARM
cross-compiler work; **none of it is required** and none of it appears below.

Every command here was verified on the machine this was written on. Where a
figure is quoted (sizes, versions, timings) it was measured, not estimated.

---

## 1. What you are building

| Component | What it is | Where it ends up |
| :--- | :--- | :--- |
| `clang`, `lld`, `llc`, `llvm-mc` | the compiler, linker and assembler, with the uG24 backend built in | `build-ug24/bin/` |
| `libug24.a`, `libug24-base.a`, `crt0.o`, `ug24.ld`, headers | the C runtime, startup code and linker script | `build-ug24/lib/ug24/` |
| `ug24sim` | the instruction-set simulator | `ug24-sim/ug24sim` |

The uG24 backend is **in-tree**: `llvm/lib/Target/UG24`, and `UG24` is listed in
`LLVM_ALL_TARGETS` in `llvm/CMakeLists.txt`. So this is an ordinary LLVM build
with one target selected — there is no patch to apply and no out-of-tree plugin.

---

## 2. Host requirements

### Software

| Tool | Version used | Minimum |
| :--- | :--- | :--- |
| CMake | 3.28.3 | 3.20 |
| Ninja | 1.11.1 | any |
| Python | 3.12.3 | 3.8 |
| C/C++ compiler | GCC 13.3.0 | GCC 7 / Clang 5 |

On Ubuntu or Debian:

```bash
sudo apt install build-essential cmake ninja-build python3 git
```

**A host clang is not needed.** The existing build tree in this repository was
configured against a locally built `stage1/bin/clang++`, which was an artefact of
the earlier ARM work. A fresh uG24-only configure with the system GCC succeeds —
verified. Ignore `stage1/` entirely.

### Hardware

| | Measured here | Recommended |
| :--- | :--- | :--- |
| Cores | 16 | 4 or more |
| RAM | 7 GB | 8 GB, or see the link-jobs note below |
| Disk, source | 1.5 GB (`llvm` 926 MB, `clang` 432 MB, `lld` 21 MB) | |
| Disk, build tree | **5.1 GB** (`bin/` alone is 1.8 GB) | |
| Disk, total free | | **12 GB** |

**The RAM note matters.** Linking LLVM takes well over 1 GB per linker process.
With 16 cores and 7 GB of RAM, Ninja's default parallelism will start enough
simultaneous links to exhaust memory and the build dies with the compiler being
killed, which looks like a toolchain bug and is not one. `LLVM_PARALLEL_LINK_JOBS`
below prevents it.

---

## 3. Get the source

```bash
git clone <this-repository> llvm-arm-cross
cd llvm-arm-cross/llvm-project
```

Everything from here is run from `llvm-project/` unless stated otherwise.

---

## 4. Configure

```bash
cmake -S llvm -B ../build-ug24 -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_TARGETS_TO_BUILD=UG24 \
      -DLLVM_ENABLE_PROJECTS="clang;lld" \
      -DLLVM_ENABLE_ASSERTIONS=OFF \
      -DLLVM_PARALLEL_LINK_JOBS=2
```

Taking each flag in turn, because the wrong value in any of them wastes an hour:

- **`-DLLVM_TARGETS_TO_BUILD=UG24`** — the whole point. Just `UG24`, nothing else.
  The build tree already in this repository was configured with
  `UG24;AArch64;ARM`, which builds two large backends nobody needs here and is
  the single biggest saving available.
- **`-DLLVM_ENABLE_PROJECTS="clang;lld"`** — `clang` is the compiler; `lld` is the
  linker, and the driver invokes it by default for this target. Both are
  required. Do not add `lldb`, `compiler-rt`, `libcxx` or anything else: the
  uG24 runtime in this repository replaces what `compiler-rt` would provide.
- **`-DCMAKE_BUILD_TYPE=Release`** — a `Debug` build of LLVM is roughly ten times
  the disk and markedly slower to run.
- **`-DLLVM_ENABLE_ASSERTIONS=OFF`** — turn this `ON` only when working on the
  backend itself; it makes the compiler slower but catches internal errors early.
- **`-DLLVM_PARALLEL_LINK_JOBS=2`** — the memory guard described above. On a
  machine with 32 GB or more it can be dropped.

Configure takes about 40 seconds and writes a 50 MB build directory. If it fails,
it fails here — read the error before building, because the build itself is long.

---

## 5. Build

```bash
ninja -C ../build-ug24 clang lld llc llvm-mc \
      llvm-objdump llvm-readelf llvm-size llvm-nm \
      llvm-ar llvm-strip llvm-objcopy
```

All eleven are valid targets in a UG24-only configuration — verified. Naming them
explicitly rather than running a bare `ninja` avoids building the hundreds of
other tools in the tree that nothing here uses.

Expect **30 to 90 minutes** depending on core count, and watch the disk rather
than the clock: this is the step that needs the 5 GB.

`clang` and `lld` are the two that matter. The rest are inspection tools the test
scripts and the cross-check kit use — `llvm-objdump` to disassemble,
`llvm-readelf` to look at symbols and segments, `llvm-size` to measure ROM use.

---

## 6. Install the runtime, simulator and tests

One script does all three:

```bash
./ug24-setup.sh
```

It expects the build tree at `build-ug24/` beside the repository or inside it;
`UG24_BUILD=/path/to/build-ug24 ./ug24-setup.sh` overrides that. What it does:

1. **`ug24-runtime/build-runtime.sh`** — compiles the C runtime with the compiler
   you just built and installs it into `build-ug24/lib/ug24/`, which is where the
   clang driver looks. It produces **two** library variants: `libug24.a` for a
   part with the optional multiplier and divider, and `libug24-base.a` for one
   without, because the multiply helpers are themselves compiled and would
   otherwise put a `MUL` back into a `-mcpu=ug24-base` image.
2. **the simulator** — one `cc -O2` of `ug24-sim/ug24sim.c`. No dependencies.
3. **the 40-case test run**, as a smoke test.

Rerun `ug24-runtime/build-runtime.sh` on its own whenever the runtime sources,
the ABI or the data layout change. A stale `libug24.a` produces wrong answers
with no diagnostic, which is the worst failure mode in this project.

---

## 7. Verify

Run all four. Together they are the acceptance contract: if they pass, the
toolchain is good, and if any fails, nothing else is worth doing until it passes.

```bash
ug24-tests/suite/run-suite.sh          # 15 programs x 5 optimisation levels
ug24-tests/run-tests.sh                # 40 execution cases
python3 ug24-tests/verify-against-isa-xlsx.py
```

Expected output, respectively:

```
all programs match at all levels
cases run: 40, failures: 0
THE ASSEMBLER MATCHES THE SPREADSHEET
```

The third is the one worth understanding. It unzips the vendor spreadsheet
`Copy of uG24xx1616uP_ISA.xlsx`, rebuilds every instruction's 16-bit encoding
from the bit columns, and compares that against `llvm-mc` — so it checks the
assembler against the vendor document with the simulator, the runtime and every
expected-output file out of the loop. It should report 53 + 30 matched and zero
mismatched.

The 12 backend unit tests use `lit`:

```bash
../build-ug24/bin/llvm-lit llvm/test/CodeGen/UG24 llvm/test/MC/UG24
```

Note that `llvm-lit` is **not** a standalone ninja target — the build writes it
into `bin/` as part of building `clang`. If it is missing, run
`python3 llvm/utils/lit/lit.py` against the same two directories instead.

---

## 8. Compile something

```bash
cat > hello.c <<'EOF'
#include <stdio.h>
int main(void) { printf("hello from uG24\n"); return 0; }
EOF

../build-ug24/bin/clang --target=ug24-unknown-elf -Os hello.c -o hello.elf
ug24-sim/ug24sim hello.elf --quiet
```

`ug24-unknown-elf` is the triple confirmed by the hardware team. The driver finds
the runtime, the startup file and the linker script by itself — no `-T`, no
`-nostdlib`, no explicit `-l`.

For a part without the optional multiplier and divider:

```bash
../build-ug24/bin/clang --target=ug24-unknown-elf -mcpu=ug24-base -Os hello.c -o hello.elf
```

To see every intermediate stage — IR, assembly, object, ELF, disassembly:

```bash
ug24-tests/ug24-build.sh hello.c
```

---

## 9. What you can ignore in this repository

Present, and irrelevant to the uG24 toolchain:

| Path | What it is |
| :--- | :--- |
| `build-arm64/`, `build-armhf/`, `build-none-eabi/`, `build-stage1/` | ARM-era build trees, git-ignored |
| `build-rt-armhf/`, `build-rt-m4/` | 6,156 files of ARM compiler-rt build output, committed by accident on 17 September 2026 and carrying this machine's absolute paths. Nothing reads them |
| `stage1/`, `toolchain-arm64/`, `toolchain-armhf/`, `toolchain-none-eabi/` | installed ARM toolchains |
| `sysroot-armhf/`, `sysroot-none-eabi/` | ARM sysroots |
| `arm-tests/` | ARM test files |
| `bolt/`, `flang/`, `libc/`, `libcxx/`, `lldb/`, `mlir/`, `polly/`, … | LLVM subprojects this build never enables |

A fresh clone for uG24 work only needs `llvm/`, `clang/`, `lld/`, `cmake/`,
`third-party/`, and the five `ug24-*` directories plus `docs/`.

---

## 10. Where to go next

| Document | What it covers |
| :--- | :--- |
| `docs/uG24-build-guide.md` | using the toolchain day to day, every driver flag, the simulator's options |
| `docs/uG24-assumptions.md` | the ABI, the memory map, and which parts are confirmed by the hardware team against which are still ours |
| `docs/uG24-platform.md` | what a third party needs to run our ELF files: the peripheral map, the symbol names, a loader |
| `docs/uG24-open-questions.md` | the twelve questions still open with the hardware team |
| `docs/handbook/how-it-was-built.html` | how each stage of the toolchain was built, for a reader new to compilers |
