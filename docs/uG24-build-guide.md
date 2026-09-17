# Building and Using the uG24 Cross-Compiler

This describes how to build the LLVM/Clang cross-compiler for the **uG24**
(uG24081616uP) 8-bit microprocessor on an x86-64 Linux host, and how to
compile and run programs with it.

Everything runs on x86: the compiler is a normal Linux binary that produces
uG24 code, and `ug24sim` executes that code on the host.

---

## 1. Layout

```
llvm-arm-cross/
├── llvm-project/          LLVM sources, including the UG24 backend
│   └── llvm/lib/Target/UG24/
├── build-ug24/            build directory for the uG24 toolchain
├── ug24-runtime/          crt0.s, ug24_builtins.c, ug24.ld
├── ug24-sim/              ug24sim.c - instruction set simulator
├── ug24-tests/            end-to-end compiler tests
└── docs/                  this guide, the machine description, assumptions
```

---

## 2. Building the toolchain

```bash
cd ~/llvm-arm-cross
cmake -G Ninja -S llvm-project/llvm -B build-ug24 \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="UG24;AArch64;ARM" \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DCMAKE_C_COMPILER="$PWD/stage1/bin/clang" \
  -DCMAKE_CXX_COMPILER="$PWD/stage1/bin/clang++" \
  -DCMAKE_INSTALL_PREFIX="$PWD/toolchain-ug24"
```

```bash
ninja -C build-ug24 clang lld llc llvm-mc llvm-objdump llvm-readobj llvm-nm llvm-ar
```

### Building the uG24 runtime

The compiler needs a startup file, a small helper library and a linker script.
They are installed into `lib/ug24` next to the compiler, which is where the
driver looks for them.

```bash
./ug24-runtime/build-runtime.sh build-ug24
```

### Building the simulator

```bash
cc -O2 -o ug24-sim/ug24sim ug24-sim/ug24sim.c
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

`ug24sim` loads the ELF into a flat 64 KB memory, sets `SP` to `0xFFFE`, and
interprets from the ELF entry point until the core executes `WFI`.

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

The uG24 backend has LLVM regression tests:

```bash
build-ug24/bin/llvm-lit -sv llvm-project/llvm/test/MC/UG24 llvm-project/llvm/test/CodeGen/UG24
```

And an end-to-end test that compiles, links and executes 32 cases covering
arithmetic, comparisons, control flow, calls, arrays, pointers and structs:

```bash
./ug24-tests/run-tests.sh
```

---

## 7. What the toolchain does not do yet

- **32-bit and floating-point arithmetic.** The backend lowers these to
  compiler-rt style calls (`__mulsi3`, `__adddf3`, …) which are not yet
  implemented, so such programs fail to link with an undefined symbol.
- **Signed and 16-bit division.** The hardware `DIV` is 8-bit unsigned only,
  so signed division and all 16-bit division go through the software helpers
  in `ug24-runtime/`.
- **Variable-length stack objects.** `alloca` with a runtime size is rejected.
- **Debug info.** DWARF is not emitted.
- **`JI`/`LJI` indirect jumps** assemble and simulate, but the code generator
  does not select them, so computed gotos and jump tables are expanded into
  compare-and-branch chains.
