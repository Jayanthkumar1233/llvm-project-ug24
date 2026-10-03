# uG24 — questions still open with the hardware team

As of 1 October 2026, after the answer document `uG24_questions&Ans.docx` and the
handwritten meeting notes of the same day.

The detailed version, written for the hardware team and the one to send them, is
at <https://claude.ai/code/artifact/3d3905a9-e44e-4de3-b3ca-f6c3b7fbca06>. This
file is the tracked summary: what is open, and where each answer lands in the
tree when it arrives.

## How the two sources rank

`uG24_questions&Ans.docx` is **authoritative**, including for the whole C ABI.
The meeting notes are supplementary: good for hardware behaviour, intent and
timelines, not for the ABI. Where the notes describe arguments and returns
running from R0 up to R8 and the document says R0 to R3, we follow the document.
The notes appear to reason from a 16-bit machine, in which one register holds an
`int`; on this part an `int` needs a pair, so "up to R8" would be five pairs of
arguments rather than two.

## Blocking

| # | Question | Lands in |
| ---: | :--- | :--- |
| 1 | Which PC does `MOV Xd, PC` read — the `MOV` itself, or the next instruction? | `UG24ExpandPseudo.cpp`, the `SequenceBytes` constant in the indirect-call expansion |
| 1b | In `JI`/`LJI Xs, i7`, is `{Xs, i7}` a concatenation or an addition? | `UG24InstrInfo.td` patterns, `UG24ExpandPseudo.cpp`, the simulator |
| 2 | Where are peripherals decoded, is there a UART, does it have a ready bit? (query G5) | `ug24-runtime/ug24.ld`, and everything downstream reads the symbols |

Question 1 is the sharper of the two, and it has **three** plausible answers,
not two:

| `MOV Xd, PC` yields | Constant |
| :--- | ---: |
| the address of the `MOV` itself | **16** — what we emit |
| the next instruction | 14 |
| two instructions ahead, as ARM does | 12 |

Note on the third: ARM A32 reads PC+8 with 4-byte instructions, Thumb PC+4 with
2-byte instructions — both are *two instructions ahead*, not one, so "follow ARM"
on a 2-byte-instruction machine means `current + 4`. That row is not a neutral
alternative: fetch runs two stages ahead of execute in the uG24's 4-stage pipe
too, so `current + 4` is what a raw fetch pointer yields if the PC read path is
just the existing register. It is the answer that happens by default, which is
how ARM got PC+8 without choosing it and then could not change it. Our recommendation, if the
choice is open, is the address of the `MOV` itself: it is what AArch64 and RISC-V
chose without legacy to carry, it is the only reading whose meaning survives a
pipeline redesign, and it is what we already assume.

Measured, by patching
our own simulator to read PC+2, the wrong constant does not crash or hang — it
returns a wrong value and exits reporting success:

| | `twice(21)` | `square(7)` | Exit |
| :--- | ---: | ---: | ---: |
| PC reads the `MOV`'s own address | 42 | 49 | 0 |
| PC reads the next instruction | 0 | 0 | 0 |

**The spreadsheet probably already answers it, our way.** It gives the link value
for every linking instruction, and each displacement equals that instruction's
own length in words:

| Instruction | Length | Spreadsheet |
| :--- | :--- | :--- |
| `JR i10` | 1 word | `PC <- PC + 1 + i10` |
| `LJR i10` | 1 word | `RA <- PC + 1` |
| `LJA a16` | 2 words | `RA <- PC + 2` |
| `LJI Xs, i7` | 1 word | `RA <- PC + 1` |

`RA` always lands on the instruction after the current one, which holds only if
`PC` means *the address of the current instruction*. If `PC` were the next
instruction, `LJR` would need `RA <- PC` and `LJA` `RA <- PC + 1`; ARM-style
two-ahead would need `RA <- PC - 1`. Since `MOV Xd, PC` is specified in the same
table as `Xd <- PC`, the consistent reading gives the current instruction's
address — constant 16, which is what we emit. Confirmation still wanted, because
this is an inference from notation and because RTL and documents can diverge, but
the question is no longer open-ended.

Our simulator shares the compiler's assumption, so our own tests cannot catch
it. `ug24-tests/handoff/src/06_indirect.c` exists for the independently written
simulator to settle it — a `volatile` function pointer the optimiser cannot
devirtualise, so the sequence really is executed.

Note that `03_control` does **not** serve this purpose, despite calling through
a function-pointer array: at `-Os` the optimiser resolves that array into direct
calls, and no program in the kit contained the indirect sequence until
`06_indirect` was added.

**Question 1b may dissolve question 1 entirely.** The spreadsheet gives
`LJI Xs, i7` as "Link and Jump to register Indirect address", `RA <- PC + 1` and
`PC <- {Xs, i7}`. The link half is exactly what an indirect call needs — the
hardware sets the return address. We read `{Xs, i7}` as a concatenation,
`(Xs & 0xFF80) | i7`, which makes `LJI` useless for a function pointer because
the low 7 bits of the target must be an assemble-time constant. But the spec
calls these "Unconditional Indirect jumps upto +/- 256B", and a signed *range* is
the language of an offset, not a concatenation. If it is `Xs + i7`, then
`LJI Xs, 0` is a one-instruction call-indirect, the eight-instruction sequence
goes away, and nothing in the compiler reads PC by hand any more. Neither `JI`
nor `LJI` is currently selected by the backend, so there is no correctness risk
today — only eight instructions per indirect call that may be unnecessary.

## ROM and RAM

**The 64 KB is the whole address space, not RAM.** One 16-bit address bus, so
code, data and peripherals share 65,536 addresses — "Implements 16-bit address
bus i.e. 64KB Total Addressable Memory". Our earlier flat 64 KB linker region was
a placeholder because nobody had told us where the boundary was; it never claimed
64 KB of RAM. The confirmed 32 KB + 32 KB fills that space **exactly**, with
nothing spare, which is precisely why the console of question 2 has nowhere to go.

| # | Question |
| ---: | :--- |
| 3 | Where do ITCM and DTCM sit in a map that is already full, and at what sizes? Which configurable stack size (1/2/4/8 KB) should we assume? |
| 4 | Is there a part or configuration with more than 32 KB of ROM? |
| 5 | Is all 32 KB of RAM usable, and where does the stack start given the peripherals of question 2? |
| 6 | Is ROM writable at run time, or write-protected after boot? |

Why 3 is asked that way: the spec lists TCM as configurable at 0/1/2/4/8/16 KB,
so **no TCM option is 32 KB**. The spec also says firmware "may reside in the
ITCM or an on-chip ROM/RAM or some external memory", so we read the 32/32 split
as ROM and RAM over AHB rather than as TCM — but a configured TCM then has to
overlay part of the same 64 KB, and we need to know where.

Why 4 is asked: the largest acceptance-suite program is 30,888 bytes at `-O0`
against a 32,768-byte region — 5.7% spare. About 15 KB of that is `printf`'s own
decimal conversion for soft float and 64-bit integers, not the test.

Worth knowing about question 2: the spec calls the configuration of these blocks
"SYSTEM IMPLEMENTATION DEFINED" and names the memory and the interrupt controller
as SoC blocks around the core, not parts of it. The peripheral map may therefore
not be a core-specification question at all, in which case we need whoever owns
the SoC memory map.

## Needed to finish

| # | Question | Lands in |
| ---: | :--- | :--- |
| 7 | The interrupt model: vector table, what hardware pushes, how a handler returns, which PSW bits gate what, is there a controller, can a handler be interrupted? (F1–F4) | `UG24ExpandPseudo.cpp`, `crt0.s`, `ug24.h` |
| 8 | What is the reset PC? (we assume `0x0000`) | `crt0.s`, the simulator |
| 9 | The notes say the part never sleeps — how should a program that returns from `main` end, and what is `WFI`? | `crt0.s`, `ug24_stdlib.c`, the simulator |
| 10 | What does your tooling expect to read out of an ELF — plain symbols, or DWARF? Other output formats? | new work; no DWARF is emitted today |

## Confirmation only — the document contradicts itself

| # | Point | What we implement |
| ---: | :--- | :--- |
| 11 | `Offset = S + A - P + 1` yields `+1` for a self-branch, where the same document says `i10 = -1` | `target - P - 1` in instruction words, which the spreadsheet's branch encodings confirm |
| 12 | `PUSH` called post-decrement, but the worked example beside it shows pre-decrement | pre-decrement, full descending, `SP` on the last occupied byte |

## Closed by these two sources — do not re-ask

Pipeline interlocks and load-use delay (no gap needed between loads); reset SP
set by boot code with the stack top in ROM — the spec separately calls PC and SP
reset-strapped, and both hold, since `crt0` always loads SP and the strapped
value only matters before it runs; what the startup code is responsible for; `NOP` = `0000_0000`; `JA` byte addressing; little-endian fetch and `PUSH`;
`R14` as `DPTR0`'s low byte; branch displacement in instruction words; the type
sizes; the alignments; `e_machine = 0xBA51`; the relocation numbering; the triple
`ug24-unknown-elf`; the descending stack.

Still ours, and uncovered by either source: `double` is IEEE **single**
precision, 32 bits, as on AVR.

Answered but worth pinning to a date: the executable model, which the notes put
at one to six weeks. It would replace every assumption above with a measurement,
and it is the only way to settle question 1 beyond doubt.
