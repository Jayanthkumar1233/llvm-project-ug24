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
| 2 | Where are peripherals decoded, is there a UART, does it have a ready bit? (query G5) | `ug24-runtime/ug24.ld`, and everything downstream reads the symbols |

Question 1 is the sharper of the two: the constant is 16 if `MOV Xd, PC` reads
its own address and 14 if it reads the next instruction, and the wrong value
makes every call through a function pointer return into the middle of an
instruction. Our simulator shares the assumption, so our tests cannot catch it.
The independently written simulator can: `03_control` in the cross-check kit
calls through a function-pointer array.

## ROM and RAM

| # | Question |
| ---: | :--- |
| 3 | Are 32 KB ROM and 32 KB RAM fixed for this part, or configurable (the TCM parameters)? |
| 4 | Is there a part or configuration with more than 32 KB of ROM? |
| 5 | Is all 32 KB of RAM usable, and where does the stack start given the peripherals of question 2? |
| 6 | Is ROM writable at run time, or write-protected after boot? |

Why 4 is asked: the largest acceptance-suite program is 30,888 bytes at `-O0`
against a 32,768-byte region — 5.7% spare. About 15 KB of that is `printf`'s own
decimal conversion for soft float and 64-bit integers, not the test.

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
set by boot code with the stack top in ROM; what the startup code is responsible
for; `NOP` = `0000_0000`; `JA` byte addressing; little-endian fetch and `PUSH`;
`R14` as `DPTR0`'s low byte; branch displacement in instruction words; the type
sizes; the alignments; `e_machine = 0xBA51`; the relocation numbering; the triple
`ug24-unknown-elf`; the descending stack.

Still ours, and uncovered by either source: `double` is IEEE **single**
precision, 32 bits, as on AVR.

Answered but worth pinning to a date: the executable model, which the notes put
at one to six weeks. It would replace every assumption above with a measurement,
and it is the only way to settle question 1 beyond doubt.
