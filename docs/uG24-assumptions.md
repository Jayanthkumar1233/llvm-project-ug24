# uG24 Compiler Design Assumptions & Spec Discrepancies

**Target processor:** uG24 (uG24081616uP / uG24xx1616uP) 8-bit microprocessor
**Sources:** `uG24081616uP_spec.pdf` (Rev 0.1), `Copy of uG24xx1616uP_ISA.xlsx`,
and `uG24_questions&Ans.docx` — the hardware team's confirmed answers, received
1 October 2026.

Everything below was a decision the compiler had to make because the source
documents do not specify it, or specify it ambiguously. Each one is
implemented in exactly one place, which is what made the confirmed answers
cheap to adopt. Sections now marked **confirmed** are no longer ours to change;
what is left unmarked is still an assumption.

### What the confirmed answers changed

Of the ten hardware answers, eight confirmed what was already implemented: the
byte order of `PUSH RA` and of instruction fetch, `R14` as the low byte of
`DPTR0`, `JA` taking a byte address, the full-descending stack, the branch
displacement counted in instruction words with `i10 = -1` for a self-branch,
and the type sizes and alignments.

The ABI answers did not. The argument and return registers, the caller- and
callee-saved split, the reserved registers, the variadic rule, the ELF machine
number, the relocation numbering and the memory map all moved. Two of those
changes exposed real bugs that the old arrangement had been hiding:

* **The soft-float comparison helpers returned the wrong width.** LLVM's
  default for `__ltsf2` and friends is a 32-bit result, while the runtime
  returns C `int`, which is 16 bits here. The caller was testing the sign of a
  register pair the callee never wrote. It passed before by luck;
  `getCmpLibcallReturnType` now says `i16`, as AVR's and MSP430's do.
* **`setjmp` saved the wrong registers.** Its buffer held `R4`-`R7` and `R10`,
  the old callee-saved set. It now holds `R6`-`R11`.

---

## 1. Target triple and binary format

**Confirmed**, except for the floating-point formats.

| Parameter | Value | Where it lives |
| :--- | :--- | :--- |
| Target triple | `ug24-unknown-elf` | `llvm/lib/TargetParser/Triple.cpp` |
| Data layout | `e-m:e-p:16:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-n8:16-S8` | `UG24TargetMachine.cpp`, `clang/lib/Basic/Targets/UG24.h` |
| ELF machine number | `EM_UG24 = 0xBA51` | `llvm/include/llvm/BinaryFormat/ELF.h` |
| C types | `char` 8, `short` 16, `int` 16, `long` 32, `long long` 64, pointer 16 | `clang/lib/Basic/Targets/UG24.h` |
| Alignment | Data byte-aligned, code 2-byte aligned | same |
| `float`/`double` | Both 32-bit IEEE single; emulated in software | same |

`0xBA51` is the number the hardware team chose, from the two they offered. The
other was 250 (`0x00FA`), which was not taken because values up to about 250 are
assigned by the generic ELF registry, so a stock `readelf` could one day report
a uG24 object as some unrelated architecture. `0xBA51` is outside that range and
cannot collide. It is still not registered: anyone reading these objects has to
be told, which is what `docs/uG24-platform.md` is for.

**`float` and `double` are the one part of this table the answers do not
cover.** `double` being IEEE single, like AVR's, is still ours.

---

## 2. C ABI

**Confirmed.** The specification defines the register file and the
`LJR`/`LJA`/`RET` instructions but no C ABI; the answers supply one.

### Argument passing
- `R0`, `R1`, `R2`, `R3` — four bytes in all, and everything past them on the
  stack.
- A 16-bit argument or pointer occupies a pair: `X0` = `R1:R0`, then
  `X1` = `R3:R2`.
- The pairs are even-aligned, because that is what the pair registers can
  express. A `char` followed by an `int` therefore lands in `R0` and then
  `R3:R2`, leaving `R1` unused. The answers do not say whether byte arguments
  pack, so this is the one detail of argument passing still ours.
- A value wider than four bytes is legalised into 16-bit pieces, so it can
  start in the pairs and continue on the stack.
- Stack arguments are written by the caller into a reserved area at the bottom
  of its own frame, so the callee finds argument *k* at `SP_incoming + offset`.

### Return values
- 8 bits in `R0`.
- 16 bits in `X0` (`R1:R0`).
- 32 bits in `X0:X1` (`R0`-`R3`).
- **Anything wider, and every aggregate, through a hidden pointer** the caller
  passes in `R0:R1`. The answers name the hidden pointer for structures over
  four bytes and stop at 32 bits for scalars; applying the same rule to a
  `long long` is the one reading that keeps the two consistent, and a return
  value has nowhere to spill in any case.
- Aggregate *arguments* are passed by reference, with the caller owning the
  copy. The uG24 has no block move, so pushing a struct a byte at a time at
  every call site costs more code than the copy the callee would have made.

`clang/lib/CodeGen/Targets/UG24.cpp` decides direct against indirect;
`UG24CallingConv.td` assigns the registers.

Clang's generic code would otherwise widen an `i8` return to the register type
of `i32`, which on this target is `i16`; `UG24TargetLowering::getTypeForExtReturn`
overrides that so definitions and call sites agree.

### Variadic arguments
Fixed arguments go in `R0`-`R3` as usual; the variadic ones always go on the
stack even when a register is free. The callee knows only its declared
parameters, so this is the one arrangement in which both sides agree on where
the variadic block begins — immediately after the fixed arguments, as one
contiguous run that `va_arg` can walk.

Whether an argument is variadic is a property of the argument rather than of
the call, and a TableGen predicate cannot see it, so `UG24TargetLowering::
LowerCall` analyses the arguments one at a time and sends the variadic ones
through `CC_UG24_VarArg`.

### Register roles
- **Caller-saved:** `R0`-`R5`, and `RA`.
- **Callee-saved:** `R6`-`R11`.
- **Reserved, never allocated:**
  - `R13:R12` (`DPTR1`) — reserved by the ABI. With `PSW.DP` kept clear it is
    not the base register the hardware selects, so the backend uses `R12` as
    the scratch byte that `ADC`/`SBB` need to hold the high half of a 16-bit
    constant, and `DPTR1` as the scratch pair for moving a special function
    register. If `DPTR1` is ever wanted as a second live base pointer, that is
    the thing to revisit.
  - `R15:R14` (`DPTR0`) — the base address register. `LD` and `ST` take their
    base from `DPTR0` (when `PSW.DP` is clear) rather than from an encoded
    operand, so dedicating the pair avoids shuffling a pointer into place
    around every memory access.
  - `PC`, `RA`, `PSW`, `SP`.

Reserving `DPTR1` bought back `R11`, which used to be the scratch byte and cost
the whole `P5` pair with it. All six ordinary pairs — `P0` to `P5` — are
allocatable now, which is more 16-bit registers than the previous arrangement
had, not fewer. What did shrink is the caller-saved half: three pairs where
there were four, so a function with four live 16-bit values borrows a
callee-saved pair and gives it back.

- **Frame pointer:** `P3` (`R7:R6`), and only in a function that needs one —
  one with a variable-length array, an `alloca`, or a taken frame address.
  Everything else addresses its frame from `SP` and leaves `P3` to the
  allocator. `UG24FrameLowering::hasFP` is the single place that decides.
  The frame pointer is set to the stack pointer *after* the locals are
  allocated, so that every frame offset is a non-negative displacement: `LD`
  and `ST` have no signed form.

### Stack layout
Full descending, `SP` pointing at the last occupied byte, in this order from
the caller's end: incoming arguments, saved `RA`, saved frame pointer where
there is one, the callee-saved registers, locals and spills, and outgoing
arguments at `SP`.

### Return address
`RA` is a single register with no hardware stack, so a function that makes any
call must preserve it. The prologue of a non-leaf function does `PUSH RA` and
the epilogue does `POP RA`. Incoming stack arguments therefore sit two bytes
above the frame, which `UG24RegisterInfo::eliminateFrameIndex` accounts for.

### Relocations
**Confirmed**, with one addition.

| Number | Name | Meaning |
| ---: | :--- | :--- |
| 0 | `R_UG2408_NONE` | no-op |
| 1 | `R_UG2408_16` | `write16le(S + A)` — a 16-bit datum, and the second halfword of a `JA`/`LJA`, which are the same operation |
| 2 | `R_UG2408_PCREL10` | signed 10-bit instruction-word displacement in `Inst{15-6}` |
| 3 | `R_UG2408_8` | `write8(S + A)` |
| 4 | `R_UG2408_LO8` | `write8((S + A) & 0xFF)` |
| 5 | `R_UG2408_HI8` | `write8(((S + A) >> 8) & 0xFF)` |
| 6 | `R_UG2408_32` | **local extension:** a 32-bit datum, which `.long symbol` needs and nothing in the confirmed set can express |

The confirmed note for `PCREL10` gives `Offset = S + A - P + 1`, which does not
agree with the same document's answer that a self-branch needs `i10 = -1`:
with `S + A = P` that formula yields `+1`. The hardware behaviour
`PC <- PC + 1 + i10` makes the displacement `target - P - 1` in instruction
words, which is what the assembler and the linker both implement and what the
spreadsheet's branch encodings verify. **Worth confirming with the hardware
team which of their two statements they meant**; the code follows the prose, not
the formula.

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

### 3.2 Branch displacement units — **confirmed**
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

### 3.4 `PUSH` / `POP` direction — **confirmed**
The spec writes `PUSH: Rs -> [SP]` then `SP <- SP - 1`, and
`POP: Rd <- [SP]` then `SP <- SP + 1`. As written these are not inverses — a
push followed by a pop would read the wrong byte.

**Assumption, since confirmed:** the stack is full-descending, i.e. `PUSH`
decrements then stores and `POP` loads then increments, so that `SP` always
points at the most recently pushed byte. Multi-byte pushes store little-endian.

The confirmed answers say "Compiler ABI: Configured as Pre-decrement (Full
Descending), where `SP` points to the last occupied byte", and their worked
example for `PUSH RA` agrees exactly: the low byte of `RA` lands at `SP-2`, the
high byte at `SP-1`, and the new `SP` points at the low byte. The same answer
also describes the hardware as post-decrement on `PUSH` and post-increment on
`POP`, which is not a working stack — a push followed by a pop would read a byte
that was never written — so it is read as a slip, contradicted by the example
beside it. **Worth confirming**, but nothing in the implementation changes
either way: it already does what the example shows.

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

Rounding is IEEE round-to-nearest, ties to even.

`printf`'s `%f` produces its digits by exact integer arithmetic on the
mantissa and the exponent, and agrees with a hosted `printf` digit for digit
at any precision. `%e` and `%g` still normalise by repeated multiplication by
ten in single precision, so beyond about seven significant digits their last
digit may be off by one — which is all the precision a `float` carries
anyway.

`%a` is not implemented; it prints `<fp?>` and consumes its argument.

The `printf` decimal conversion is split across two files, and the split is a
linking decision rather than a tidiness one.

`ug24_printf_float.c` holds `%f`, and the `%f` half of `%g`, converted from
the bit pattern by **integer arithmetic alone** — a float is exactly
`mant × 2^(exp-23)`, so shifts recover the integer part and a 32-bit binary
fraction, and multiplying that fraction by ten recovers its decimal digits.
Nothing there calls into soft float, so `ug24_stdio.c` references it normally
and it is linked with `printf` unconditionally. Its digits agree with a hosted
`printf` at any precision.

`ug24_float.c` holds the arithmetic and `%e`, which genuinely needs it:
finding the decimal exponent of an arbitrary float means dividing by ten until
it is in range, and there is no integer shortcut across the whole exponent
range. `%e` is reached through a **weak** symbol, so a program that does no
floating-point arithmetic links neither it nor the library under it. Such a
program printing `%e` gets `<fp?>`; printing `%f` gets its digits.

The cost of that default, measured:

| Program | Size |
| :--- | ---: |
| `printf("Hello ug24\n")` — optimised to `puts`, no formatter at all | 300 bytes |
| `printf("%d\n", n)` | 17,144 bytes |
| the same, opting out of the 64-bit formatter | 16,232 bytes |
| the same, opting out of the float formatter | 11,480 bytes |
| the same, opting out of both | 9,342 bytes |
| `printf("%f\n", x)` | 17,160 bytes |

Measured with `llvm-size` at `-Os`. Two defaults are folded into those
numbers, and both were chosen the same way: a conversion that silently prints
the wrong answer is worse than a program that is larger than it needed to be.

**Opting out** of either needs no flag and no compiler support. Define the
symbol yourself and the archive member is never extracted.

```c
int __ug24_format_float(char *out, unsigned long bits, int precision, char conv)
{ (void)bits; (void)precision; (void)conv; out[0] = '?'; return 1; }

int __ug24_format_u64(char *out, unsigned long long value, unsigned base,
                      const char *alphabet)
{ (void)value; (void)base; (void)alphabet; out[0] = '?'; return 1; }
```

The 64-bit conversions are the newer of the two. `%lld` used to read four
bytes of an eight-byte argument: the value printed truncated to 32 bits, and
the four bytes left on the stack desynchronised every later argument in the
same call, so `printf("%lld %d", big, 77)` printed a wrong number followed by
a wrong `int`. `%zu` had the mirror-image fault — `size_t` is 16 bits here, and
reading a 32-bit `long` for it consumed two bytes too many. Both are fixed, and
`ug24-tests/suite/t12_int64.c` now checks the conversions and the argument
alignment behind them against glibc.

Neither formatter divides. `__udivdi3` exists, but one call is on the order of
thirty thousand instructions, and a twenty-digit number would need twenty of
them; subtracting powers of ten costs at most nine subtractions per digit.

The peripheral symbols of §5 add about 270 bytes of symbol table to every
image, which is neither loaded nor executed.

### A rejected design, recorded because it was implemented first

The case that forced this arrangement is a program whose floating-point
arithmetic the optimiser folds away:

```c
printf("%f\n", 879 * 9 / 50.0f + 52);   /* one constant by link time */
```

Nothing at link time distinguishes that from a program that never had a float
in it, so the first fix put the decision in the **compiler**: `UG24AsmPrinter`
noticed a floating-point value reaching a variadic call — still visible in the
IR — and emitted an undefined reference to `__ug24_format_float`.

It worked, it was precise, and it was wrong. It hardcoded a private symbol of
one particular C library into the code generator, and made the compiler's
output depend on how `printf` happens to be built. Libcalls such as
`__mulhi3` are not a precedent: those exist because the *instruction set*
cannot multiply, which is the code generator's business. How `printf` formats
a float is not.

It was removed. The backend contains no reference to any runtime symbol, and
`ug24-tests/verify-against-isa-xlsx.py` checks the assembler against the
vendor spreadsheet with the runtime and the simulator out of the loop.

### 3.9 Optional multiplier and divider
`MUL` and `DIV` are optional blocks in the SoC configuration. They are present
on the part this toolchain was written against, so the `mul` and `div`
subtarget features default to on; `-mcpu=ug24-base`, or
`-Xclang -target-feature -Xclang -mul`, describes a part without them, and the
same operations then become calls to the runtime helpers.

**The runtime library has to be rebuilt to match, and this file used to claim
otherwise.** The helpers are shift-and-add loops that need no hardware block,
which is why the claim looked right — but `__mulqi3`, `__mulhi3` and `__mulsi3`
are themselves compiled, and compiled for the default CPU they use `MUL` for
their inner byte product. A `-mcpu=ug24-base` program that multiplied anything
therefore still had a `MUL` in its image, from the library rather than from its
own code, and nothing diagnosed it: the flag did what it said for the program
and the archive quietly undid it.

`build-runtime.sh` now builds both variants, `libug24.a` and `libug24-base.a`,
and the Clang driver links the second when `-mcpu=ug24-base` is in effect
(`clang/lib/Driver/ToolChains/UG24.cpp`). A base-CPU image now contains no `MUL`
or `DIV` anywhere, which `ug24-tests/handoff/make-handoff.sh` checks by
disassembling what it builds.

This was found because a separately written simulator that does not implement
`MUL` reported it — by hand-patching `__mulsi3` in one of our ELF files to call a
software multiply, which was correct for that call site and is not a thing anyone
should have to do.

`__UG24_HAS_MUL__` and `__UG24_HAS_DIV__` are defined when the blocks are
present.

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

### The transmit handshake is self-limiting
`UART_STATUS` bit 0 is this project's invention along with the rest of the
peripheral page, so a loader that does not model it reads zero. The runtime used
to wait for that bit unconditionally, which turned an unimplemented register into
a program that ran for ever and printed nothing — the worst failure mode
available, because it is indistinguishable from a miscompiled program. A
separately written simulator hit exactly this and spent its effort patching our
ELF files rather than reporting it.

`ug24-runtime/ug24_uart.c` now owns the one place a byte reaches the console and
settles the question once, on the first byte, while the transmitter is idle and
the answer is least ambiguous: ready within 4,096 reads means flow control works
and every later byte waits as long as it takes; never ready means nothing
implements the register, so stop asking and write the byte. Output then appears
either way, and hardware that answers on the first read pays nothing. Routing
both `<stdio.h>` and the `ug24_put*` helpers through one function also took
about 2 KB out of every image that prints.

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

**Confirmed**, apart from the peripheral page.

| Region | Range | Holds |
| :--- | :--- | :--- |
| ROM / Flash | `0x0000`-`0x7FFF` | the vector table, `.text`, `.rodata` |
| RAM / SRAM | `0x8000`-`0xFEFF` | `.data`, `.bss`, the heap, the stack |
| Peripherals | `0xFF00`-`0xFFFF` | **assumed**; query G5 |

The confirmed answer gives RAM as `0x8000`-`0xFFFF` with the stack top at
`0xFFFF`, which leaves the memory-mapped console nowhere to live. Query G5 — the
peripheral map — is still unanswered, so the top page is held out of RAM as
before and `__stack_top` is `0xFEFE`. Every peripheral address is published as a
symbol in each image, so when G5 is answered only `ug24-runtime/ug24.ld`
changes; see [uG24-platform.md](uG24-platform.md).

Two consequences worth knowing:

**`.data` now has a load address distinct from its run address** — the initial
contents travel in ROM next to `.text`, and `crt0` copies them into RAM before
`main`. The copy loop was a no-op while the map was one flat region. A loader
that goes by section name rather than by `p_paddr` prints garbage from an
initialised variable and nothing else wrong, which is why the cross-check kit
has a program for exactly that.

**32 KB of ROM is a real ceiling, though nothing is against it today.** When the
split first landed, the two largest programs in the acceptance suite went over
it at some optimisation levels — `t13_float` reached 32,904 bytes at `-O2`
against a 32,768-byte region — because both printf formatters are linked by
default. Consolidating the transmit handshake into one function (§4) gave about
2 KB back and the worst case is now 30,778 bytes, so the suite builds the default
configuration at every level with roughly 2 KB spare. Worth watching: 4.7 KB of
float formatter is 15% of this ROM where it was 7% of the old flat 64 KB, and the
one-line opt-out below is what a program that never prints a float should use.

### The peripheral page is not derivable from the source documents

The top page, `0xFF00-0xFFFF`, is held out of the linker's `MEM` region for
memory-mapped peripherals. Which byte in it is the console is a *platform*
decision: the core has none, so neither the specification nor the spreadsheet
says, and an independently written simulator cannot derive it. Query G5 asks
for the real map.

Rather than leaving that to be agreed out of band, the linker script publishes
the map as absolute symbols in every image — `__ug24_uart_tx`,
`__ug24_sim_exit` and the rest — and `ug24.h`, the runtime and `ug24sim` all
read it from there instead of from a literal. Moving the `MMIO` region moves
all of them together. The contract, and a loader that honours it, are in
[uG24-platform.md](uG24-platform.md); that document is also the one to hand to
anyone writing a second simulator.
