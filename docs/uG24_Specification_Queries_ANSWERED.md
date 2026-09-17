**OPEN QUERIES — COMPILER TEAM TO SILICON TEAM**

**uG24 Specification Queries — with compiler-team answers filled in**

| Revision | Rev 4 — answers added by the working toolchain |
| :---- | :---- |
| **Sources checked** | `Copy of uG24xx1616uP_ISA.xlsx`; `uG24081616uP_spec.pdf` Rev 0.1 |
| **Answer status** | 14 resolved from the specification · 17 answered by toolchain decision (need sign-off) · 20 still hardware-only · 3 query defects |
| **Answered by** | Working LLVM 17 backend, assembler, linker, runtime and instruction-set simulator for `ug24-unknown-none-eabi` |

---

**HOW TO READ THE ANSWERS**

**RESOLVED FROM SPEC** — Verified directly against the ISA spreadsheet or a
numbered table in the PDF. Quoted below. No hardware input needed.

**TOOLCHAIN DECISION** — The specification is silent. The compiler chose an
answer, implemented it, and it produces working programs in simulation.
**This is evidence, not confirmation.** Silicon still has to sign it off, and
where our choice is wrong the generated code will be wrong.

**STILL OPEN** — Cannot be answered by a compiler at all. Hardware only.

**QUERY DEFECT** — The query's premise does not match the source documents.

---

**CORRECTIONS TO THE QUERY DOCUMENT ITSELF**

Three items should be amended before this goes to the silicon team, because
sending an incorrect premise wastes their time and costs the compiler team
credibility.

1. **READ THIS FIRST — the 0x40000000 UART.** No file named
   `hello_world_ug24.c` exists in this repository, and no source file anywhere
   in the tree references `0x40000000`. If that sample came from a different
   machine, cite it precisely; if it cannot be produced, drop the paragraph.
   The 64 KB question can be asked on its own merits as A1.

2. **D7 — Tables 3.3 and 3.4 are not empty.** Both have content in
   Rev 0.1. Table 3.3 gives `[15:6] i10, [5:2] func, [1:0] = 0b10`, and
   Table 3.4 gives `[15:9] i7, [8:6] Xs, [5:2] func`. That is exactly the
   reading the query derives, so the query is unnecessary — downgrade it to a
   note or delete it.

3. **H1 — the citation does not exist.** Neither the PSW sheet nor the PDF
   contains any statement about which instructions affect Z, or any exclusion
   list naming CLRF, INVF and CMP. The *question* is real, blocking, and worth
   asking; the sentence attributing it to "the PSW sheet" must be removed or
   the query will be answered with "we never wrote that".

**Unverifiable references to a 16-bit part.** `uG24161616uP`, `LDB` and `CMPI`
appear nowhere in either supplied document, and the spreadsheet contains only
the `uG24081616uP` sheets. Queries A4, E2 and H3 lean on them. PDF §3.1.1 does
say "refer uG24081616uP_ISA sheet **of** the uG24xx1616uP_ISA.xlsx", implying
other sheets exist somewhere, so these are plausible rather than invented — but
they cannot be checked here and should be marked as such.

---

# **A. Memory and address space**

## **A1 What is the actual 64 KB memory map? BLOCKING**

**ANSWER — STILL OPEN.** A compiler cannot supply this.

What the toolchain currently assumes, which the linker script
`ug24-runtime/ug24.ld` makes easy to change: one flat 64 KB image with `.text`
from `0x0000`, then `.rodata`, `.data`, `.bss`; the stack growing down from
`0xFEFE`; and a simulator-only peripheral window at `0xFF00–0xFFFF`. **All of
this is placeholder.** Please supply the real region bases and sizes, and the
real UART address.

## **A2 Are ITCM and DTCM one address space or two? ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

§1.1 lists the TCMs as "Memory-mapped, single-cycle" — verified verbatim. The
backend is built on a single flat 64 KB space with one pointer type and no LLVM
address spaces, and compiles and runs correctly on that basis. Please confirm.

## **A3 Can the core read data operands from ITCM? ANSWERED**

**ANSWER — TOOLCHAIN DECISION, needs confirmation.**

We place string literals and `const` data in the same flat image as code and
read them with `LD` through `DPTR0`. This works in simulation. Nothing in the
specification forbids it, but nothing states it either. **If ITCM turns out not
to be data-readable, every `const` object must be copied to DTCM at startup** —
a change to the linker script and `crt0.s`, not to the compiler.

## **A4 Is DPTR a byte address, and are load offsets scaled? IMPORTANT**

**ANSWER — RESOLVED FROM SPEC for the 8-bit part.**

The ISA sheet gives `LD Rd, i8` as `Rd ← [DPTR + i8]` — no scaling. DPTR is a
byte address. The backend implements byte addressing throughout and structure
field offsets come out correct in test.

The `LD Rd, i7` scaled form referenced in the query belongs to a 16-bit part
that does not appear in either supplied document. Split that half of the
question out or drop it.

## **A5 What are the alignment rules? IMPORTANT**

**ANSWER — STILL OPEN.**

The toolchain takes the conservative position: `-a:8 -S8` in the DataLayout,
i.e. **everything byte-aligned, no alignment requirement assumed**. That is
safe under any answer, but if the core does require even alignment for 16-bit
accesses we are currently free to generate misaligned ones. Please state the
requirement and what happens on violation — ME exception, rotation, or
truncation.

## **A6 Which TCM configuration will the target SoC use? IMPORTANT**

**ANSWER — STILL OPEN.** Needed to fix the linker script regions and the
default stack reservation.

---

# **B. Data pointers and the DPTR protocol**

## **B1 Are DPTR0 and DPTR1 the only way to address memory? ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

§1.1: "Supports 2 16-bit Data Pointers and 8 16-bit Instruction Pointers through
GPRs", with "Data Pointer selection is embedded in the PSW SFR" — both verified
verbatim. The ISA sheet lists no `[Rn]` form. The backend is built on exactly
this and generates working code.

## **B2 Confirm the DPTR register mapping. ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct for the 8-bit part.**

The `uG24081616uP_Registers` sheet gives code `110` → R12/R13 and code `111` →
R14/R15, labelled DPTR1 and DPTR0. So DPTR1 is X6 and DPTR0 is X7, addressed as
3-bit codes 6 and 7. Implemented and verified by the encoding tests.

## **B3 Are the DPTRs the same storage as those GPRs? ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

"through GPRs" settles it. The backend models the pairs as register-aliasing
sub-registers of R12–R15 and **reserves R14/R15 (DPTR0) permanently**. R12/R13
(DPTR1) are left allocatable as an ordinary 16-bit pair — see B4.

## **B4 Is there a recommended DPTR usage convention? BLOCKING**

**ANSWER — TOOLCHAIN DECISION, needs sign-off.**

We reserve **DPTR0 as the sole memory base register**. Every load and store goes
through it, and `PSW.DP` is cleared once at reset and again in each function
prologue so the selection never changes. DPTR1 is left in the general allocation
pool as a normal 16-bit register pair.

Rationale: alternating between two pointers costs a `PSW` write of unknown
latency (B5), whereas reloading one pointer costs two `MVI`s of known cost.
**If you tell us `PSW.DP` switching is cheap and hazard-free, a two-pointer
convention would measurably reduce reload traffic** and we would revisit this.

## **B5 What does switching PSW.DP cost? IMPORTANT**

**ANSWER — STILL OPEN.** The toolchain currently avoids the question entirely by
never switching (see B4). An answer would unlock a real optimisation.

## **B6 How is a DPTR loaded, and is there a use hazard? IMPORTANT**

**ANSWER — TOOLCHAIN DECISION on the first half; STILL OPEN on the second.**

We load DPTR0 by writing R14 and R15 with `MVI`, or by copying a pair into them
with two `MOV`s, and then issue the dependent `LD`/`ST` **immediately, with no
separation**. If a use hazard exists this is wrong everywhere. Please state the
required separation, if any.

---

# **C. Immediates and sign conventions**

## **C1 Are the i8 immediates signed or unsigned? BLOCKING**

**ANSWER — STILL OPEN. Neither document states it.**

The toolchain treats the field as **unsigned with wraparound**: the encoder
masks to 8 bits, so `x - 1` is emitted as `ADI Rd, 255`. Arithmetic results are
correct in simulation under that reading, but the simulator implements our
assumption, so this is not independent evidence. **This one genuinely needs an
answer.**

## **C2 Confirm the immediate-move variants. CONFIRM**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

`MVI Rd, i8` fills the whole 8-bit register; the ISA sheet lists no wider
immediate move. Loading a 16-bit constant takes two `MVI`s, one per half — that
is exactly the idiom the compiler emits, including for addresses, where the two
halves carry `R_UG24_LO8` and `R_UG24_HI8` relocations.

## **C3 Is the load/store displacement signed? IMPORTANT**

**ANSWER — STILL OPEN.**

The toolchain treats it as **unsigned, 0–255 bytes**, and adds any larger
displacement into DPTR0 before the access. If the field is signed the reachable
window is −128…+127 and some of our folded offsets are out of range.

## **C4 Confirm the INC/DEC bias. ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

§1.1: "Implements separate Increment & Decrement instructions with operation
size of 1-16" — verified verbatim, and consistent with the sheet's
`Rd ← Rd ± (i4 + 1)`. The sheet's Comments column says "No Flags are affected"
for both.

**This mattered in practice.** Our first `crt0.s` used `INC` to walk a pointer
and branched on the carry afterwards; because `INC` leaves the flags alone, the
branch tested a stale flag and the startup loop erased the program. The fix was
to use `ADI`, which does set carry.

## **C5 Confirm the shift-amount bias and range. CONFIRM**

**ANSWER — RESOLVED FROM SPEC for the 8-bit part. Your reading is correct.**

The sheet gives `SHIFT = (i3 + 1)`, so 1–8 with shift-by-zero unrepresentable.
The encoder emits `amount − 1`; a shift of zero is folded away before selection
and amounts above 8 become a constant-folded sequence or a runtime call.

## **C6 Do 16-bit shifts operate on 16 bits? WITHDRAWN**

**ANSWER — WITHDRAWAL CORRECT, and the note attached to it matters.**

Your observation that `ASR` is an 8-bit operation is right and has a
consequence we hit directly: sign-extending a byte to a pair needs an explicit
`ASR Rd, 7` to produce the 0x00/0xFF fill byte. Note it must be **7, not 8** —
a shift by the full register width is undefined at the IR level and LLVM folds
it to poison, which silently left an uninitialised high byte until we caught it.

## **C7 Which instructions use the Rs-form of the 12-bit format? CONFIRM**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct and complete.**

Verified against the actual tables in Rev 0.1:

- **Table 3.1** (Rd/Xd form): `[15:8] i8`, `[7:4] Rd/Xd`, `[3:1] func`, `[0] = 1`
- **Table 3.2** (Rs/Xs form): `[15:12] Rs/Xs`, `[11:4] i8`, `[3:1] func`, `[0] = 1`

`ST Rs, i8` is the only instruction using the Rs form; `LD`, `MVI`, `ADI`, `SBI`,
`ANDI`, `ORI` and `XORI` all use the Rd form. Our backend implements the split
and `ST` encodes as `0x2085` for `st r2, [8]`, matching the sheet.

---

# **D. Control transfer and branch encoding**

## **D1 Is PC an instruction-word counter or a byte counter? ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

§1.1: "Conditional & Unconditional PC-relative jumps upto +/- 1KB" — verified
verbatim. A signed 10-bit field reaching ±1 KB only works at two bytes per
instruction, so **PC counts instruction words**. The assembler, linker and
simulator all halve the byte displacement, and branch targets resolve correctly
end to end.

## **D2 Is the 10-bit branch displacement signed? ANSWERED**

**ANSWER — RESOLVED FROM SPEC for the encoding. One gap on our side.**

Two's complement, ±1 KB. Implemented; out-of-range branches are diagnosed with
"branch target out of range" rather than silently truncated.

**We have not implemented branch relaxation.** A function whose body exceeds
±1024 bytes between a branch and its target will fail to assemble instead of
being rewritten into a two-instruction sequence. That is a known compiler gap,
not a specification question.

## **D3 In a 2-fetch instruction, which word is at the lower address? ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

**Table 3.6** in Rev 0.1 gives the opcode word as `[15:8] = 0`, `[7:4] func`,
`[3:0] = 0`, with `a16` on the following row, and the accompanying text states
"func[3:1] is 0b100". So the **opcode word is at the lower address**, `JA` is
func `1000` and `LJA` is func `1001`.

Our encoder emits opcode-first and the linker patches `a16` at byte offset 2 via
`R_UG24_ABS16`. Verified: `lja` assembles to `90 00 <lo> <hi>`.

## **D4 Confirm the link-register offsets. ANSWERED**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct**, and it corroborates D1.

## **D5 What exactly does {Xs, i7} mean? BLOCKING**

**ANSWER — STILL OPEN. This remains genuinely blocking, and we agree with your
reading of the contradiction.**

The ISA sheet says `PC ← {Xs, i7}` — a concatenation. §1.1 says "Unconditional
Indirect jumps upto +/- 256B", which describes a signed offset from a base, not
a concatenation. Both cannot be right.

Our backend currently **does not select `JI`/`LJI` at all**; they assemble and
simulate but no C construct generates them. Indirect calls, function pointers,
virtual dispatch and jump tables therefore all expand to compare-and-branch
chains. Answering this would let us implement them properly.

For the record, our simulator implements the concatenation reading as
`PC ← (Xs & 0xFF80) | i7`, which is a guess.

## **D6 Confirm the indirect-jump base register restriction. WITHDRAWN**

**ANSWER — WITHDRAWAL CORRECT.** The 8-bit part uses a 3-bit `Xs` field
(Table 3.4), which can name any of the eight pairs.

## **D7 Tables 3.3 and 3.4 are empty. CONFIRM**

**ANSWER — QUERY DEFECT. The tables are not empty, and your derived reading is
correct.**

Rev 0.1 gives:

- **Table 3.3** (immediate form): `[15:6] i10`, `[5:2] func`, `[1:0] = 0b10`
- **Table 3.4** (Xs form): `[15:9] i7`, `[8:6] Xs`, `[5:2] func`

Both match what the query derives from the sheet, and both match our
implementation. Please delete this query rather than sending it.

---

# **E. Stack and calling convention**

## **E1 Does SP point at the next free slot? CONFIRM**

**ANSWER — STILL OPEN, and the specification is self-inconsistent here.**

As written, `PUSH` is store-then-decrement and `POP` is load-then-increment.
Those are not inverses — a push followed by a pop reads the wrong byte.

The toolchain assumes **full-descending**: decrement then store, load then
increment, so SP always addresses the most recently pushed byte, and multi-byte
pushes are little-endian. `PUSH RA` / `POP RA` round-trip correctly under that
reading and function calls work. Please confirm or correct.

## **E2 Confirm the mixed stack slot sizes. CONFIRM**

**ANSWER — RESOLVED FROM SPEC for the 8-bit part. Your reading is correct.**

`PUSH Rs` moves SP by 1; `PUSH PC`, `PUSH RA` and `PUSH PSW` move it by 2. The
16-bit part half of the query is unverifiable here.

## **E3 What bounds the stack exception? IMPORTANT**

**ANSWER — STILL OPEN.** No stack-probe code is generated. Functions with large
frames will run off the stack silently if there is no limit register.

## **E4 Do you have a preferred calling convention? BLOCKING**

**ANSWER — TOOLCHAIN DECISION. This is our proposal for sign-off.**

| | Registers |
| :---- | :---- |
| 8-bit arguments | R0–R3, then stack |
| 16-bit arguments and pointers | W (R9:R8), then DPTR1 (R13:R12), then stack |
| 8-bit return | R0 |
| 16-bit return | W |
| Callee-saved | R4, R5, R6, R7, R10 |
| Caller-saved | R0–R3, R8, R9, R12, R13, RA |
| Reserved | R11 (expansion temporary), R14/R15 (DPTR0 memory base) |

On your specific question — **pointer arguments are passed in an ordinary
register pair, not in a DPTR**, and the callee copies into DPTR0 when it
dereferences. This follows from B4: with DPTR0 pinned as the only base register,
passing a pointer in it would mean every call clobbers the caller's base.

R11 is reserved because `ADC` and `SBB` take a register operand, so adding a
16-bit constant needs somewhere to materialise the high byte.

**This ABI is implemented and working, but nothing is written against it yet.
It is still cheap to change.**

## **E5 Is a frame pointer expected? IMPORTANT**

**ANSWER — TOOLCHAIN DECISION.**

No frame pointer. Every frame slot is addressed from SP, which keeps a register
free. The cost is that a debugger cannot walk the stack — acceptable while there
is no debugger, and worth revisiting if JTAG debug is enabled (K5).

---

# **F. Interrupts and exceptions**

## **F1–F4 STILL OPEN — all four.**

A compiler cannot answer any of these. The toolchain currently emits **no
interrupt support at all**: there is no `__attribute__((interrupt))`, no vector
table in the linker script, and no handler prologue or epilogue. Nothing here
blocks ordinary C compilation, but nothing here works either.

The four answers map directly onto four pieces of work: F1 → the linker script
vector table; F2 → the handler prologue; F3 → the handler epilogue; F4 → the
flag-clearing sequence inside it.

---

# **G. Multiply, divide and the W register**

## **G1 How wide is W, and which registers is it? BLOCKING**

**ANSWER — RESOLVED FROM SPEC. Your reading is correct.**

The registers sheet gives code `100` → R8/R9, labelled W. So W is the extended
pair X4 = {R9,R8}, 16 bits.

On the divide: the ISA sheet says `W[0] ← floor(Rs1/Rs2)`, `W[1] ← Rs1 % Rs2`.
Both results are in W — **quotient and remainder are the two bytes of W itself**,
not W plus a second pair. That is the natural result for an 8÷8 divide.

## **G2 Confirm the divide result placement. CONFIRM**

**ANSWER — TOOLCHAIN DECISION following from the register sheet, needs
confirmation.**

The registers sheet lists R8 as the low half of W and R9 as the high half, so we
read `W[0] = R8` (quotient) and `W[1] = R9` (remainder). The backend implements
`udiv` as the low sub-register and `urem` as the high one.

**A swap here silently returns one for the other**, and no test we can write
would catch it, because our simulator implements the same assumption. Worth an
explicit confirmation.

## **G3 Does writing R8 or R9 clobber W? BLOCKING**

**ANSWER — RESOLVED FROM SPEC. Yes, they alias.**

"through GPRs" again. The backend models W as a register pair whose
sub-registers are R8 and R9, and marks `MUL`/`DIV` as defining W, so the
allocator knows both bytes are destroyed. This is modelled aliasing rather than
reservation, so R8/R9 stay usable.

## **G4 What is the latency of MUL and DIV? ANSWERED**

**ANSWER — PARTIALLY RESOLVED; the fence question is STILL OPEN.**

§1.1 gives "Configurable support for 1/2/4/8-stage pipelined Multiply" —
verified. Your reading of divide as fixed 8-stage and both as out-of-pipe
matches the text.

**We emit no fence before reading W.** If the core does not interlock, every
multiply and divide we generate is wrong. Please confirm explicitly.

## **G5 Will multiply and divide be present in the target configuration? BLOCKING**

**ANSWER — STILL OPEN, and this now has teeth.**

§1.1: "Configurable implementation of Multiply & Divide instructions" —
verified. The backend **currently emits `MUL` and `DIV` unconditionally**, so a
configuration without them would not run our output at all.

Making this a subtarget flag is straightforward, but we need to know whether to
bother, and at which multiply depth. If the answer is "both present", say so and
we will hard-code it.

---

# **H. Condition flags**

## **H1 Confirm that CMP does not affect Z. BLOCKING**

**ANSWER — QUERY DEFECT in the citation; the question itself is real and STILL
OPEN.**

Neither the PSW sheet nor the PDF states which instructions affect Z, and
neither contains an exclusion list naming CLRF, INVF and CMP. **Please remove
the attribution before sending**, and ask the question plainly: *which
instructions update Z, and does CMP?*

Our backend is safe under either answer: it pairs `CMP` only with `BEQ`/`BNE`
and the ordering branches, and never with `BZ`/`BNZ`.

## **H2 Are LT and GT signed or unsigned? BLOCKING**

**ANSWER — STILL OPEN. Neither document states it.**

The toolchain assumes **unsigned**, and — importantly — is built so that the
answer does not change correctness. Signed comparisons flip the sign bit of both
operands (`x ^ 0x80`, or `^ 0x8000` for a pair) and then use the unsigned
ordering, which yields the signed result either way. If the hardware turns out
to compare signed, the flip becomes redundant and we delete two instructions per
signed comparison; nothing breaks.

So this is no longer blocking for us — but it is worth an answer, because that
redundancy is on the hottest path in compiled code.

## **H3 Does CMP affect the carry flag? IMPORTANT**

**ANSWER — STILL OPEN.** Our simulator sets Cy to the borrow of `Rs1 − Rs2`, but
that is our assumption, and the backend does not depend on it. The `CMPI`
half of the query concerns a part not in these documents.

## **H4 Is S a plain copy of the result's sign bit? CONFIRM**

**ANSWER — STILL OPEN, low impact.** The backend never generates `BPS`/`BNS`, so
nothing depends on it today.

---

# **I. Pipeline and hazards**

## **I1 Is the pipeline fully interlocked? BLOCKING**

**ANSWER — STILL OPEN. This is the largest unquantified risk in the toolchain.**

The backend **assumes a fully interlocked pipeline** and inserts no fences and
no padding anywhere. If the core does not interlock, the generated code is
wrong in a way that will show up as intermittent, data-dependent failures that
look nothing like compiler bugs.

## **I2 Is there a load-use delay? BLOCKING**

**ANSWER — STILL OPEN.** We emit `LD` followed immediately by a use of the loaded
register throughout — it is the single most common pattern in the output. If a
delay slot exists, essentially every function we generate is broken.

## **I3 Which PC does MOV Xd, PC read? IMPORTANT**

**ANSWER — STILL OPEN, no current impact.** The backend does not generate
`MOV Xd, PC`; all addressing is absolute via `lo8`/`hi8` relocations.

---

# **J. Reset and boot**

## **J1 What will the board strap for reset PC and SP? BLOCKING**

**ANSWER — STILL OPEN.**

One correction to the query's note: it says the C startup code does *not*
initialise SP because hardware sets it. **Our `crt0.s` currently does set SP**,
from the `__stack_top` symbol in the linker script, because we have no strapped
value to rely on. If the board really does strap SP, that instruction should be
removed — please confirm which.

The entry point is currently placed at `0x0000`.

## **J2 Is any set-up required before the first memory access? IMPORTANT**

**ANSWER — STILL OPEN.** `crt0.s` currently does only three things before
touching memory: set SP, clear `PSW.DP`, and zero `.bss`. If TCMs need enabling
or wait states programming, that has to come first.

---

# **K. Product scope**

## **K1 Which variant ships? ANSWERED**

**ANSWER — CONFIRMED, and it matches what we built.**

The backend targets `uG24081616uP` only, as `ug24-unknown-none-eabi`. Sixteen
8-bit GPRs with 16-bit values in pairs.

Your remark that AVR rather than MSP430 is the right in-tree model is correct
and is what the implementation follows: 16-bit arithmetic expands into byte
sequences with `ADD`/`ADC`, and displacement addressing is restricted to a
pointer pair exactly as AVR restricts it to X/Y/Z.

## **K2 C only, or C++ as well? IMPORTANT**

**ANSWER — DECISION NEEDED FROM PRODUCT, not from silicon.**

C works today. C++ has not been attempted and would need a runtime library port
on a part with 64 KB of address space. Our recommendation is **C only** unless
there is a specific requirement.

## **K3 Freestanding, or a hosted C library? IMPORTANT**

**ANSWER — PARTIALLY ANSWERED BY THE TOOLCHAIN.**

We ship freestanding, with a small in-house support library `libug24.a`
providing what the code generator actually emits: 16-bit shifts, multiply,
divide and modulo, `memcpy`/`memset`/`memmove`, and console output.

The query's note that "the archives in the prototype sysroot are empty" no
longer applies to this build — the library is populated and linked into every
program.

**Still missing:** 32-bit and floating-point helpers. Code using `long`, `float`
or `double` fails at link time with an undefined symbol. These can be written in
C and compiled with this toolchain; it is work, not a blocker.

## **K4 Can we get an executable model to test against? BLOCKING**

**ANSWER — RESOLVED. We built one.**

`ug24-sim/ug24sim.c` is an instruction-set simulator covering the full
instruction set, with a memory-mapped UART, memory dumping and instruction
tracing. It loads a linked uG24 ELF and runs it.

The query calls this the highest-value item on the list, and that assessment was
correct — it changed the project. Compiled programs now run and are checked
against expected values: 40 end-to-end cases covering arithmetic, comparisons,
loops, calls, recursion, arrays, pointers, structs and stack arguments, passing
at every optimisation level.

**One caveat that matters.** The simulator implements *our* reading of the
specification. Where the specification is ambiguous — E1, C1, H2, I1, I2 — the
simulator agrees with the compiler by construction, so those tests cannot
detect a wrong assumption. It replaces "we have never run anything" with "we run
correctly against our own interpretation". **RTL or silicon is still needed to
validate the interpretation**, and we would still value access.

## **K5 Which configurable features will be enabled? IMPORTANT**

**ANSWER — STILL OPEN.**

Of the configurable items listed, three affect code generation and would each
need a subtarget flag: multiply/divide presence (G5), the fence instructions,
and `HAS_PIPELINES` — which, as the query notes, also changes the answers in
section I. If the set is fixed for the product, say so and we hard-code it.

---

**SUMMARY OF WHAT WOULD UNBLOCK THE MOST WORK**

If the silicon team can only answer a handful, these five change the most:

1. **I1 and I2** — pipeline interlocking and load-use delay. Everything we
   generate assumes both are safe. Wrong answers mean intermittent failures
   across all output.
2. **A1** — the memory map. Every placeholder in the linker script depends on
   it.
3. **C1** — i8 signedness. Affects the correctness of ordinary arithmetic.
4. **E1** — stack direction. Affects every frame layout.
5. **G5** — will MUL and DIV exist. We currently emit them unconditionally.

D5 remains blocking for function pointers specifically, and the F-section
answers are needed before any interrupt handler can be written in C.
