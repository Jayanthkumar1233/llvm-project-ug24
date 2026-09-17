# uG24 Compiler Design Assumptions & Spec Discrepancies

**Target processor:** uG24 (uG24081616uP / uG24xx1616uP) 8-bit microprocessor
**Sources:** `uG24081616uP_spec.pdf` (Rev 0.1), `Copy of uG24xx1616uP_ISA.xlsx`

Everything below is a decision the compiler had to make because the source
documents do not specify it, or specify it ambiguously. Each one is
implemented in exactly one place so it is cheap to change once the hardware
team confirms the intended behaviour.

---

## 1. Target triple and binary format

| Parameter | Choice | Where it lives |
| :--- | :--- | :--- |
| Target triple | `ug24-unknown-none-eabi` | `llvm/lib/TargetParser/Triple.cpp` |
| Data layout | `e-m:e-p:16:16-i8:8-i16:8-a:8-n8:16-S8` | `UG24TargetMachine.cpp`, `clang/lib/Basic/Targets/UG24.h` |
| ELF machine number | `EM_UG24 = 0x9240` | `llvm/include/llvm/BinaryFormat/ELF.h` |
| C types | `char` 8, `short` 16, `int` 16, `long` 32, `long long` 64, pointer 16 | `clang/lib/Basic/Targets/UG24.h` |
| Alignment | Everything byte aligned; the hardware has no alignment requirement | same |
| `float`/`double` | Both 32-bit IEEE single; emulated in software | same |

`EM_UG24` is **not** registered with the generic ELF ABI. It is a private
number chosen so that uG24 objects are distinguishable; if the project ever
needs interoperability with third-party tools, this is the value to
renegotiate.

---

## 2. C ABI

The specification defines the register file and the `LJR`/`LJA`/`RET`
instructions but no C ABI, so this one is invented.

### Argument passing
- 8-bit arguments: `R0`, `R1`, `R2`, `R3`, then the stack.
- 16-bit arguments and pointers: `W` (`R9:R8`), then `DPTR1` (`R13:R12`),
  then the stack.
- Stack arguments are written by the caller into a reserved area at the bottom
  of its own frame, so the callee finds argument *k* at `SP_incoming + offset`.

### Return values
- 8-bit values in `R0`.
- 16-bit values in `W`.

Clang's generic code would otherwise widen an `i8` return to the register type
of `i32`, which on this target is `i16`; `UG24TargetLowering::getTypeForExtReturn`
overrides that so definitions and call sites agree.

### Register roles
- **Caller-saved:** `R0`–`R3`, `R8`, `R9` (`W`), `R12`, `R13` (`DPTR1`), `RA`.
- **Callee-saved:** `R4`, `R5`, `R6`, `R7`, `R10`.
- **Reserved, never allocated:**
  - `R11` — expansion temporary. `ADC`/`SBB` take a register, so 16-bit
    arithmetic against a constant needs somewhere to put the high byte.
    The pair `P5` (`R11:R10`) is reserved along with it.
  - `R15:R14` (`DPTR0`) — the base address register. `LD` and `ST` take their
    base from `DPTR0` (when `PSW.DP` is clear) rather than from an encoded
    operand, so dedicating the pair avoids shuffling a pointer into place
    around every memory access.
- There is **no frame pointer**. Every frame slot is addressed from `SP`.

### Return address
`RA` is a single register with no hardware stack, so a function that makes any
call must preserve it. The prologue of a non-leaf function does `PUSH RA` and
the epilogue does `POP RA`. Incoming stack arguments therefore sit two bytes
above the frame, which `UG24RegisterInfo::eliminateFrameIndex` accounts for.

---

## 3. Ambiguities in the source documents

### 3.1 `XORI Rd, i8` — resolved, no conflict
An earlier revision of this document claimed there was no free `func` slot in
Format I for `XORI`. Re-reading the ISA spreadsheet, all eight encodings are
assigned and `XORI` has one of them:

| `func[2:0]` | Instruction |
| :--- | :--- |
| `000` | `LD Rd, i8` |
| `001` | `MVI Rd, i8` |
| `010` | `ST Rs, i8` |
| `011` | `ANDI Rd, i8` |
| `100` | `ORI Rd, i8` |
| `101` | `XORI Rd, i8` |
| `110` | `ADI Rd, i8` |
| `111` | `SBI Rd, i8` |

`XORI` is implemented as a normal instruction. Note that `ST` alone uses a
different field layout from the rest of Format I: the register is in
`Inst{15-12}` and the immediate in `Inst{11-4}`.

### 3.2 Branch displacement units
The spec writes branches as `PC <- PC + 1 + (cond ? i10 : 0)` and `LJA` as
`RA <- PC + 2`, while stating that instructions are 2 bytes and `JA`/`LJA` are
4. Both are only consistent if `+1` means *one instruction word*.

**Assumption:** the encoded `i10` is a signed count of 2-byte instruction
words applied to the address of the instruction *after* the branch. In bytes:

```
target = address_of_branch + 2 + (i10 * 2)
```

This gives a reach of ±1024 instructions. It is implemented in
`UG24AsmBackend::adjustFixupValue`, `lld/ELF/Arch/UG24.cpp` and
`ug24-sim/ug24sim.c` — all three must change together.

`JA`/`LJA` take a 16-bit **byte** address, matching the 64 KB address space.

### 3.3 Signedness of the `CMP` flags
`CMP Rs1, Rs2` is documented as setting `PSW.EQ`, `PSW.LT` and `PSW.GT`
without saying whether the ordering is signed or unsigned.

**Assumption:** the ordering is **unsigned**, and `PSW.Cy` additionally
receives the borrow of `Rs1 - Rs2`.

The compiler never relies on a signed compare: it flips the sign bit of both
operands (`x ^ 0x80`, or `^ 0x8000` for 16-bit values) and then uses the
unsigned ordering, which produces the signed answer either way. So if the
hardware turns out to compare signed, only the sign-flip becomes redundant —
nothing becomes wrong. See `translateCC` in `UG24ISelLowering.cpp`.

### 3.4 `PUSH` / `POP` direction
The spec writes `PUSH: Rs -> [SP]` then `SP <- SP - 1`, and
`POP: Rd <- [SP]` then `SP <- SP + 1`. As written these are not inverses — a
push followed by a pop would read the wrong byte.

**Assumption:** the stack is full-descending, i.e. `PUSH` decrements then
stores and `POP` loads then increments, so that `SP` always points at the most
recently pushed byte. Multi-byte pushes store little-endian.

### 3.5 Extended register encoding width
`W`, `DPTR1` and `DPTR0` appear as `100_0`, `110_0`, `111_0` in some tables
and `100`, `110`, `111` in others.

**Resolved from the encoding diagrams:** the extended-register field is
**3 bits** (`Inst{6-4}` in `MOV Xd, SFR`, `Inst{14-12}` in `MOV SFR, Xs`,
`Inst{8-6}` in `JI`/`LJI`), holding the GPR index shifted right by one:
`W = 100`, `DPTR1 = 110`, `DPTR0 = 111`. This is *not* the same value as the
4-bit GPR encoding, and `UG24RegisterInfo.td` sets `HWEncoding` accordingly.

### 3.6 `PSW.DP` and the data pointer
`LD`/`ST` select `DPTR1` over `DPTR0` based on `PSW.DP`. The compiler always
wants `DPTR0`, so `crt0.s` clears the bit at reset and every function
prologue with a frame re-clears it (`CLRF 8`) so that a stray write to `PSW`
cannot silently redirect every memory access. `PSW.DP` is bit 8, read off the
PSW layout in the register spreadsheet.

### 3.7 Hardware `MUL` / `DIV`
`MUL` and `DIV` write their results to `W` implicitly rather than to an
encoded destination, so they are selected as pseudo instructions carrying an
ordinary destination and split by `UG24ExpandPseudo` into the real
instruction followed by a move out of `W`.

They cover 8-bit multiply, and 8-bit unsigned divide and remainder — `DIV`
leaves the quotient in `W`'s low half and the remainder in its high half,
which is exactly the layout of a register pair. A 16-bit multiply where both
operands are widened bytes uses `MUL` as well.

Everything else (16-bit multiply of genuinely wide values, all signed
division) goes to the software helpers in `ug24-runtime/`.

### 3.8 Software floating point and wide integers
`float`, `double`, `i32` and `i64` arithmetic are lowered to compiler-rt style
calls. `ug24-runtime/ug24_builtins.c` implements the 8- and 16-bit helpers the
backend actually emits; the 32-bit and floating-point helpers are **not yet
provided**, so code using them will fail to link with an undefined symbol
rather than misbehave.

---

## 4. Memory map

The reset `PC` and `SP` are strapped inputs on real hardware, and the
instruction/data TCM sizes are per-SoC parameters. `ug24-runtime/ug24.ld`
therefore lays out one flat 64 KB image — text from address 0, then rodata,
data and bss, with the stack growing down from `0xFFFE`. Adjust the `MEMORY`
block to match the part being targeted.
