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
- 32-bit values in `W` and `DPTR1`, 64-bit values in `W`, `DPTR1`, `P0` and
  `P1`. A return value has nowhere to spill to, so four pairs is the limit;
  anything wider, and every aggregate, is returned through a hidden pointer
  the caller passes as a first argument.
- Aggregate arguments are passed by reference, with the caller owning the
  copy. The uG24 has no block move, so pushing a struct a byte at a time at
  every call site costs more code than the copy the callee would have made.

`clang/lib/CodeGen/Targets/UG24.cpp` is where this is written down on the
Clang side; `UG24CallingConv.td` assigns the registers on the backend side.

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
- **Frame pointer:** `P3` (`R7:R6`), and only in a function that needs one —
  one with a variable-length array, an `alloca`, or a taken frame address.
  Everything else addresses its frame from `SP` and leaves `P3` to the
  allocator. `UG24FrameLowering::hasFP` is the single place that decides.
  The frame pointer is set to the stack pointer *after* the locals are
  allocated, so that every frame offset is a non-negative displacement: `LD`
  and `ST` have no signed form.

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
calls, provided by `ug24-runtime/`:

| File | Helpers |
| :--- | :--- |
| `ug24_builtins.c` | 8-, 16- and 32-bit shift, multiply, divide, remainder; `mem*`/`str*` |
| `ug24_int64.c` | `__muldi3`, `__udivdi3`, `__umoddi3`, `__divdi3`, `__moddi3`, the 64-bit shifts, `__cmpdi2`, `__ucmpdi2` |
| `ug24_float.c` | the single-precision set: `__addsf3`, `__subsf3`, `__mulsf3`, `__divsf3`, the six comparisons, `__unordsf2`, and the conversions to and from 32- and 64-bit integers |
| `ug24_setjmp.s` | `setjmp`, `longjmp` |

`double` and `long double` are IEEE **single** on this target, the same choice
AVR makes, so the double-precision half of the soft-float library does not
exist and is never asked for: there is no `__adddf3` and no `__extendsfdf2`.

Rounding is IEEE round-to-nearest, ties to even. `printf`'s `%f`, `%e` and
`%g` produce their digits by repeated
multiplication by ten in single precision, so beyond about seven significant
digits the last one may be off by one — which is all the precision a `float`
carries anyway.

`%a` is not implemented; it prints `<fp?>` and consumes its argument.

The `printf` float formatter lives in `ug24_float.c` rather than in
`ug24_stdio.c`, and `ug24_stdio.c` declares it **weak and does not define
it**. An undefined weak symbol does not make the linker pull a member out of
`libug24.a`, so a program that never does any floating-point arithmetic
leaves it null and `%f` prints `<fp?>`; a program that does any at all has
already pulled `ug24_float.c` in for `__addsf3` and the symbol resolves.

That matters because the difference is not small. `printf("Hello ug24\n")`
is 300 bytes. The same program with a `%f` and a float to put through it is
about 25 KB, once the soft-float library, the 64-bit division behind the
decimal conversion and the 4.5 KB formatter are all linked. On a 64 KB part
that is worth not paying for by default.

The gap is a program that prints a floating-point *constant* and does no
arithmetic, where the optimiser has folded everything away before the linker
sees it. Link that with `-Wl,-u,__ug24_format_float`: an explicit undefined
symbol is a strong one and does pull the member in.

### 3.9 Optional multiplier and divider
`MUL` and `DIV` are optional blocks in the SoC configuration. They are present
on the part this toolchain was written against, so the `mul` and `div`
subtarget features default to on; `-mcpu=ug24-base`, or
`-Xclang -target-feature -Xclang -mul`, describes a part without them, and the
same operations then become calls to the runtime helpers. Those helpers are
shift-and-add loops and need no hardware block of their own, so the runtime
library does not have to be rebuilt to match. `__UG24_HAS_MUL__` and
`__UG24_HAS_DIV__` are defined when the blocks are present.

With the feature off, `mul` is not a recognised instruction in assembly
either — the `AssemblerPredicate` rejects it rather than encoding something
the part cannot execute.

---

## 4. Interrupts

**Everything in this section is an assumption.** The `PSW` layout names five
interrupt bits — `IE` (15), `ME` (14), `SE` (13), `NMI` (12), `MI` (11) — but
the documents to hand do not say where the vector table lives, what the
hardware pushes on entry, or which peripheral owns which source. Specification
queries A1 and A6 ask for exactly that. Until they are answered, the toolchain,
the runtime and the simulator implement the model below, which uses only
instructions and `PSW` bits the specification does define.

### Vector table
Four slots at address `0x0000`, each one `LJA`, which is four bytes wide. The
table is a run of jumps rather than a table of addresses, so the core simply
starts executing at slot 0 out of reset — which is what the existing "reset
`PC` is the base of `.text`" assumption already required.

| Address | Source | Symbol |
| :--- | :--- | :--- |
| `0x0000` | reset | `_start` |
| `0x0004` | non-maskable | `__ug24_nmi` |
| `0x0008` | maskable | `__ug24_irq` |
| `0x000C` | software | `__ug24_swi` |

The three handler symbols are **weak** definitions in `crt0.s` that undo what
the hardware pushed and resume, so an enabled source with no handler loses the
interrupt rather than running off into whatever follows. Defining a function
with one of those names replaces the default.

### Entry and exit
Taking an interrupt pushes the interrupted `PC`, then `PSW`, clears `PSW.IE`
so the handler is not immediately re-entered, sets `PSW.MI`, and jumps to the
vector. The handler returns with `POP PSW` followed by `POP PC`; `PSW` comes
back with `IE` as it was, so interrupts re-enable on return. There is no
`RETI` instruction in the ISA and none is invented.

### Writing a handler
`__attribute__((interrupt))` on a `void f(void)`:

```c
#include <ug24.h>

static volatile unsigned ticks;

__attribute__((interrupt)) void __ug24_irq(void) {
    ticks++;
    UG24_IRQ_STATUS = UG24_IRQ_TIMER;   /* clear the source */
}
```

The attribute changes two things. The prologue preserves every register the
handler writes, caller-saved ones included — a handler preempts code that
expects all of them back — and `R11` and `DPTR0` with them, which are reserved
and so invisible to the generic callee-saved machinery. `UG24ExpandPseudo`
then replaces `RET` with the `POP PSW` / `POP PC` pair. A handler that calls
another function saves everything, since it cannot know what the callee
writes.

### Controller registers
The simulator models a small controller in the MMIO page. Like the rest of
this section it is a placeholder for whatever the SoC actually decodes.

| Address | Register |
| :--- | :--- |
| `0xFF10` | `IRQ_STATUS` — read pending sources, write 1-bits to clear |
| `0xFF11` | `IRQ_ENABLE` — per-source enable mask |
| `0xFF12` | `IRQ_RAISE` — write a source number to raise it by hand |
| `0xFF13` | `TIMER_LOAD` — instructions between timer ticks, 0 disables |

Source bit 0 is the timer and bit 1 is software. `ug24_enable_interrupts()`
in `ug24.h` sets `PSW.IE` and `PSW.ME`; because the ISA has no `SETF`, each
bit is cleared and then inverted.

`WFI` waits rather than halts when an interrupt can still arrive, and halts
otherwise — which is what `crt0`'s halt loop, reached with interrupts off,
relies on.

---

## 5. Memory map

The reset `PC` and `SP` are strapped inputs on real hardware, and the
instruction/data TCM sizes are per-SoC parameters. `ug24-runtime/ug24.ld`
therefore lays out one flat 64 KB image — text from address 0, then rodata,
data and bss, with the stack growing down from `0xFFFE`. Adjust the `MEMORY`
block to match the part being targeted.
