# Building and Using the uG24 Cross-Compiler

This describes how to build the LLVM/Clang cross-compiler for the **uG24**
(uG24081616uP) 8-bit microprocessor on an x86-64 Linux host, and how to
compile and run programs with it.

Everything runs on x86: the compiler is a normal Linux binary that produces
uG24 code, and `ug24sim` executes that code on the host.

---

## 1. Layout

Everything lives in one repository, so a clone gives you the compiler, the
runtime it needs, the simulator that runs the result and the tests that prove
it works. Nothing is shipped as a binary: the point is that anyone can build
it.

```
llvm-project/              the repository
├── llvm/lib/Target/UG24/  the backend
├── clang/lib/Basic/Targets/UG24.*      the target description
├── clang/lib/CodeGen/Targets/UG24.cpp  the C ABI
├── clang/lib/Driver/ToolChains/UG24.*  the driver
├── ug24-runtime/          crt0.s, the helper library, ug24.ld, the headers
├── ug24-sim/              ug24sim.c - instruction set simulator
├── ug24-tests/            end-to-end tests and the acceptance suite
├── docs/                  this guide, the machine description, assumptions
├── ug24-setup.sh          builds the runtime and simulator, runs the tests
└── build-ug24/            the build directory, created below
```

---

## 2. Building the toolchain

```bash
cd llvm-project
cmake -G Ninja -S llvm -B build-ug24 \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="UG24;AArch64;ARM" \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DCMAKE_INSTALL_PREFIX="$PWD/toolchain-ug24"
```

`AArch64` and `ARM` are there only because the host tests use them; `UG24`
alone is enough and builds faster. Add
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` if you would rather
not build with GCC — either works.

```bash
ninja -C build-ug24 clang lld llc llvm-mc llvm-objdump llvm-readobj llvm-nm \
  llvm-ar llvm-size llvm-dwarfdump
```

### Everything else, in one step

```bash
./ug24-setup.sh
```

That builds the runtime into `build-ug24/lib/ug24` — the startup file, the
helper library, the linker script and the headers, all where the driver looks
for them — compiles the simulator, and runs the end-to-end tests. Pass a
build directory as its argument, or set `UG24_BUILD`, if the toolchain is not
in `build-ug24`.

The two steps it wraps, if you want them separately:

```bash
./ug24-runtime/build-runtime.sh build-ug24
cc -O2 -o ug24-sim/ug24sim ug24-sim/ug24sim.c
```

### Targeting a part without the optional blocks

The multiplier and the divider are optional in the SoC configuration. They
are present by default; `-mcpu=ug24-base` compiles for a part without them,
and the same operations become calls to the runtime helpers, which are
shift-and-add loops and need no hardware of their own. The runtime library
does not have to be rebuilt to match.

```bash
build-ug24/bin/clang --target=ug24-unknown-none-eabi -mcpu=ug24-base -Os hello.c -o hello.elf
```

---

## 3. Compiling a program

### The short form

The clang driver knows the uG24 target, so one command compiles, links and
produces a bare-metal ELF executable:

```bash
build-ug24/bin/clang --target=ug24-unknown-none-eabi -Os hello.c -o hello.elf
```

This pulls in `crt0.o`, `libug24.a` and the default linker script
`ug24.ld` automatically.

### Stopping earlier

Assembly only:

```bash
build-ug24/bin/clang --target=ug24-unknown-none-eabi -Os -S hello.c -o hello.s
```

Object file only:

```bash
build-ug24/bin/clang --target=ug24-unknown-none-eabi -Os -c hello.c -o hello.o
```

### Assembling hand-written uG24 assembly

```bash
build-ug24/bin/llvm-mc -triple=ug24-unknown-none-eabi -filetype=obj boot.s -o boot.o
```

Add `-show-encoding` to see the machine code for each instruction.

### Linking by hand

```bash
build-ug24/bin/ld.lld -m elf32ug24 -T ug24-runtime/ug24.ld \
  crt0.o hello.o -lug24 -o hello.elf
```

### Inspecting the result

```bash
build-ug24/bin/llvm-objdump -d --triple=ug24-unknown-none-eabi hello.elf
```

```bash
build-ug24/bin/llvm-readobj --file-headers --relocations hello.o
```

---

## 4. Running a program

`ug24sim` loads the ELF into a flat 64 KB memory, seeds `SP` from the
`__stack_top` symbol in the image, and interprets from the ELF entry point
until the core executes `WFI` with no interrupt left to wait for.

```bash
ug24-sim/ug24sim hello.elf
```

It prints the instruction count, the final register state and the return
value — `r0` for a byte-sized return, `w` for a 16-bit one.

Useful options:

| Option | Effect |
| :--- | :--- |
| `--trace` | Print every instruction as it executes, to stderr |
| `--dump` | Write the whole 64 KB memory to `ug24-memory.bin` on exit |
| `--max N` | Stop after N instructions instead of the 20,000,000 default |
| `--quiet` | Print only what the program itself wrote |

The exit status is what the program returned, except for three diagnostics:

| Status | Meaning |
| :--- | :--- |
| 3 | An instruction the ISA does not define |
| 4 | The stack left the window between `__heap_end` and `__stack_top` |
| 2 | Bad command line |

The stack window comes from the symbol table, and every instruction is
checked against it. That turns the classic bare-metal failure — a stack that
grows down into the heap and quietly corrupts it — into a message.

The simulator also models an interrupt controller: a timer, a software
source, and the `PSW` enable bits. Everything about it is an assumption; see
"Interrupts" in [uG24-assumptions.md](uG24-assumptions.md) for what is
assumed and why.

To look at a global after the run, find its address and read the dump:

```bash
build-ug24/bin/llvm-nm hello.elf | grep ' buffer'
```

```bash
xxd -s $((0x1a36)) -l 8 ug24-memory.bin
```

---

## 5. A worked example

```c
unsigned char buffer[8];

static unsigned char fib(unsigned char n) {
    unsigned char a = 0, b = 1, t;
    while (n--) { t = a + b; a = b; b = t; }
    return a;
}

int main(void) {
    for (unsigned char i = 0; i < 8; i++)
        buffer[i] = fib(i);
    return buffer[7];
}
```

```bash
build-ug24/bin/clang --target=ug24-unknown-none-eabi -Os demo.c -o demo.elf && ug24-sim/ug24sim demo.elf --dump
```

The run reports `w=13` (the value of `buffer[7]`), and the dump contains
`00 01 01 02 03 05 08 0d` at `buffer` — the first eight Fibonacci numbers.

---

## 6. Running the test suites

Three of them, from smallest to largest.

**LLVM regression tests** — encodings, instruction selection, frame layout,
the immediate range checks and the optional blocks:

```bash
build-ug24/bin/llvm-lit -sv llvm/test/MC/UG24 llvm/test/CodeGen/UG24
```

**End-to-end cases** — 40 small programs compiled, linked and executed, each
checked against a value computed on the host:

```bash
./ug24-tests/run-tests.sh
```

**The acceptance suite** — fifteen whole programs, each run at `-O0`, `-O1`,
`-O2`, `-Os` and `-O3` and diffed against expected output. Most of the
expected files are generated by compiling the same source with the host
compiler, so the suite is a comparison against a normal C implementation
rather than against itself:

```bash
./ug24-tests/suite/run-suite.sh            # all of them
./ug24-tests/suite/run-suite.sh t13_float  # just one
```

**The spreadsheet check.** Independent of all three: it reads the vendor ISA
spreadsheet, rebuilds each instruction's encoding from the bit columns, and
compares that with what `llvm-mc` emits. Nothing the toolchain believes about
itself is involved — no simulator, no runtime, no expected-output file.

```bash
./ug24-tests/verify-against-isa-xlsx.py
```

| Program | Covers |
| :--- | :--- |
| `t1_types` | integer widths, signedness, conversions |
| `t2_string` | `mem*` and `str*` |
| `t3_printf` | every `printf` conversion and flag |
| `t4_flow` | loops, switches, recursion, function pointers |
| `t5_volatile` | MMIO access patterns |
| `t6_startup` | `.data` copy, `.bss` clear, initialisers |
| `t7_malloc` | `malloc`, `free`, `realloc`, heap exhaustion |
| `t8_stress` | a long mixed workload |
| `t9_language` | every C statement, cross-checked against the host |
| `t10_isa` | the instructions no C construct reaches, via a `.s` companion |
| `t11_vla` | variable-length arrays and `alloca` |
| `t12_int64` | 64-bit multiply, divide, shift and compare |
| `t13_float` | soft float, including `%f`, `%e` and `%g` |
| `t14_setjmp` | `setjmp` / `longjmp`, including out of a VLA frame |
| `t15_interrupt` | interrupt handlers, under the assumed model |

A failing case leaves its output in `ug24-tests/suite/<name><level>.actual`
next to the `.expected` it did not match.

---

## 7. What the toolchain does not do yet

- **Debug info beyond line tables.** DWARF is emitted and `llvm-dwarfdump`
  reads it, but the backend has had no work on variable locations, so a
  debugger would show optimised-away values rather than wrong ones.
- **`JI`/`LJI` indirect jumps** assemble and simulate, but the code generator
  does not select them, so computed gotos and jump tables become
  compare-and-branch chains.
- **Double precision.** `double` and `long double` are IEEE *single* on this
  target, the same choice AVR makes. Code that needs 53 bits of mantissa
  will not get it.
- **`%a`** in `printf` prints `<fp?>`, and so does `%e` in a program that does
  no floating-point arithmetic at all — `%e` needs the soft-float library and
  such a program does not link it. `%f` always works, and agrees with a hosted
  `printf` digit for digit.
- **`printf` costs about 15 KB** once any conversion is used, of which roughly
  6.8 KB is the float formatter. Programs that never print a float can get
  that back by defining `__ug24_format_float` themselves — see §3.8 of
  [uG24-assumptions.md](uG24-assumptions.md).
- **`scanf` and friends.** There is no input device to read from.
- **C++.** The runtime is C only: no `libc++`, no exceptions, no static
  initialisation order support. C is complete through C17 apart from the
  points above; `_Complex`, bit-fields, unions, variable-length arrays,
  `alloca` and `setjmp` all work.

Two things are implemented against **assumptions** rather than against the
specification, because the documents to hand do not answer them. Both are
marked as such in [uG24-assumptions.md](uG24-assumptions.md), and both are
expected to change when the vendor answers specification queries A1, A6 and
G5:

- the **interrupt model** — where the vector table lives, what the hardware
  pushes on entry, and which peripheral owns which source;
- the **peripheral map** — the UART, the exit register and the interrupt
  controller are placed in the top page by this toolchain, not by the SoC.
