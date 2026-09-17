# uG24 Microprocessor Machine Description

**Document Version:** 1.0  
**Target Processor:** uG24 (uG24081616uP / uG24xx1616uP) 8-bit Microprocessor  
**Specification Source:** `uG24081616uP_spec.pdf` (Rev 0.1) & `Copy of uG24xx1616uP_ISA.xlsx`  

---

## 1. Architectural Overview

The **uG24** is a custom 8-bit high-performance RISC microprocessor with 16-bit memory addressing capabilities, designed for embedded SoC applications.

### 1.1 Key Hardware Specifications
- **Data Path Width:** 8-bit
- **Memory Address Space:** 16-bit (64 KB addressable program & data space)
- **Endianness:** Little Endian
- **Pipeline Architecture:** Configurable 4-stage pipeline (**IF** - Instruction Fetch, **ID** - Instruction Decode, **EX** - Execute, **WB** - Write Back) or 2-stage non-pipelined mode (**IF**, **WB**).
- **Bus Interface Unit (BIU):** AHB-Lite Master/Slave interface with N:1 adaptive synchronous handshaking.
- **Instruction Length:** 16-bit fixed width for single-fetch instructions; 32-bit for 2-fetch absolute jumps (`JA`, `LJA`).

---

## 2. Register File Architecture

### 2.1 General Purpose Registers (GPRs) - 8-Bit
The uG24 core includes **16 8-bit General Purpose Registers** (`R0` through `R15`):

| Register | Hardware Encoding | Primary Role / Calling Convention Status |
| :--- | :---: | :--- |
| `R0` | `0000` (0) | Argument 1 / Return Value (8-bit) / Scratch |
| `R1` | `0001` (1) | Argument 2 / Scratch |
| `R2` | `0010` (2) | Argument 3 / Scratch |
| `R3` | `0011` (3) | Argument 4 / Scratch |
| `R4` | `0100` (4) | Callee-Saved Preserved Register |
| `R5` | `0101` (5) | Callee-Saved Preserved Register |
| `R6` | `0110` (6) | Callee-Saved Preserved Register |
| `R7` | `0111` (7) | Callee-Saved Preserved Register / Frame Pointer (FP) |
| `R8` | `1000` (8) | Working Register Pair Low (`W.L`) / Scratch |
| `R9` | `1001` (9) | Working Register Pair High (`W.H`) / Scratch |
| `R10` | `1010` (10) | Callee-Saved Preserved Register |
| `R11` | `1011` (11) | Callee-Saved Preserved Register |
| `R12` | `1100` (12) | Data Pointer 1 Low (`DPTR1.L`) |
| `R13` | `1101` (13) | Data Pointer 1 High (`DPTR1.H`) |
| `R14` | `1110` (14) | Data Pointer 0 Low (`DPTR0.L`) |
| `R15` | `1111` (15) | Data Pointer 0 High (`DPTR0.H`) |

### 2.2 Extended 16-bit register pairs (`Xs` / `Xd`)

Three of the eight adjacent GPR pairs can be *named* by the instruction set.
The field that names them is **3 bits wide** and holds the GPR index shifted
right by one — it is *not* the same value as the 4-bit GPR encoding.

Only four instruction forms encode that field: `MOV Xd, SFR`, `MOV SFR, Xs`,
`SWAP Xs, SFR` and `JI`/`LJI`. Ordinary 16-bit arithmetic is synthesised from
byte operations with `ADD`/`ADC`, which works on any adjacent pair, so the
compiler treats all eight pairs as 16-bit registers and restricts only those
four forms to the three below.

| Name | GPRs (high:low) | 3-bit code | Role |
| :--- | :---: | :---: | :--- |
| `W` | `R9:R8` | `100` | Working register, 16-bit return value, `MUL`/`DIV` result |
| `DPTR1` | `R13:R12` | `110` | Data pointer 1 |
| `DPTR0` | `R15:R14` | `111` | Data pointer 0, the default `LD`/`ST` base |

### 2.3 Special function registers

Selected by a 2-bit field.

| Name | Code | Description |
| :--- | :---: | :--- |
| `PC` | `00` | Program counter |
| `RA` | `01` | Return address, written by `LJR`/`LJI`/`LJA` |
| `PSW` | `10` | Program status word |
| `SP` | `11` | Stack pointer |

### 2.4 PSW layout

| Bit | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| Flag | IE | ME | SE | NMI | MI | — | — | DP | S | GT | LT | EQ | Z | Dz | — | Cy |

`DP` (bit 8) selects `DPTR1` instead of `DPTR0` as the base address for `LD`
and `ST`. The compiler keeps it clear.

---

## 3. Instruction encodings

Instructions are 16 bits wide, except `JA` and `LJA`, which take a second
fetch carrying a 16-bit absolute address. The low bits select the encoding
class:

| Selector | Format | Contents |
| :--- | :--- | :--- |
| `Inst{0} == 1` | I | 8-bit immediate data transfer, arithmetic and logic |
| `Inst{1-0} == 10` | B | PC-relative branches and register-indirect jumps |
| `Inst{3-0} == 0000` | P | Two implied operands, machine control, absolute jumps |
| `Inst{3-0} == 0100` | S | Stack operations and register transfers |
| `Inst{3-2} == 10` | A | Register/immediate arithmetic |
| `Inst{3-2} == 11` | L | Register/immediate logic and shifts |

### 3.1 Format I — `Inst{0} = 1`

Layout is `imm8 | Rd | func | 1` with `imm8` in `Inst{15-8}` and `Rd` in
`Inst{7-4}` — **except `ST`**, which puts `Rs` in `Inst{15-12}` and `imm8` in
`Inst{11-4}`.

| `func` | Instruction | Operation |
| :---: | :--- | :--- |
| `000` | `LD Rd, i8` | `Rd <- [DPTR + i8]` |
| `001` | `MVI Rd, i8` | `Rd <- i8` |
| `010` | `ST Rs, i8` | `[DPTR + i8] <- Rs` |
| `011` | `ANDI Rd, i8` | `Rd <- Rd & i8` |
| `100` | `ORI Rd, i8` | `Rd <- Rd \| i8` |
| `101` | `XORI Rd, i8` | `Rd <- Rd ^ i8` |
| `110` | `ADI Rd, i8` | `Rd <- Rd + i8` |
| `111` | `SBI Rd, i8` | `Rd <- Rd - i8` |

`DPTR` is `PSW.DP ? DPTR1 : DPTR0`.

### 3.2 Format A — arithmetic, `Inst{3-2} = 10`

Layout is `Rs | func | Rd | 10 | 00`.

| `func` | Instruction | Notes |
| :---: | :--- | :--- |
| `0000` | `ADD Rd, Rs` | |
| `0001` | `ADC Rd, Rs` | adds `PSW.Cy` |
| `0100` | `SUB Rd, Rs` | |
| `0101` | `SBB Rd, Rs` | subtracts `PSW.Cy` |
| `1000` | `INC Rd, i4` | adds `i4 + 1`; **no flags affected** |
| `1001` | `DEC Rd, i4` | subtracts `i4 + 1`; **no flags affected** |

`INC`/`DEC` leaving the flags alone matters: a multi-byte increment cannot use
them for the carry and has to use `ADI`/`ADC`.

### 3.3 Format L — logic and shifts, `Inst{3-2} = 11`

| `func` | Instruction | Notes |
| :---: | :--- | :--- |
| `0000` | `AND Rd, Rs` | |
| `0010` | `OR Rd, Rs` | |
| `0100` | `XOR Rd, Rs` | |
| `0110` | `NOT Rd` | |
| `1000` | `LSL Rd, i3` | shifts by `i3 + 1`, so 1..8 |
| `1001` | `LSR Rd, i3` | |
| `1010` | `RSL Rd, i3` | rotate left |
| `1011` | `RSR Rd, i3` | rotate right |
| `1100` | `ASR Rd, i3` | |
| `1110` | `CLRF i4` | clears `PSW[i4]` |
| `1111` | `INVF i4` | inverts `PSW[i4]` |

Shift and rotate amounts are encoded as `amount - 1` in `Inst{14-12}`, so the
hardware cannot express a shift by zero. `i4` for `CLRF`/`INVF` sits in
`Inst{15-12}`.

### 3.4 Format P — `Inst{3-0} = 0000`

Layout is `Rs1 | Rs2 | func | 0000`.

| `func` | Instruction | Operation |
| :---: | :--- | :--- |
| `0000` | machine control | see below |
| `0010` | `SWAP Rs1, Rs2` | `Rs1 <-> Rs2` |
| `0011` | `SWAP Xs, SFR` | `Xs` in `Inst{14-12}`, SFR code in `Inst{9-8}` |
| `0100` | `MUL Rs1, Rs2` | `W <- Rs1 * Rs2` |
| `0101` | `DIV Rs1, Rs2` | `W.lo <- Rs1 / Rs2`, `W.hi <- Rs1 % Rs2` |
| `0110` | `CMP Rs1, Rs2` | sets `EQ`, `LT`, `GT` |
| `1000` | `JA a16` | 4 bytes; `PC <- a16` |
| `1001` | `LJA a16` | 4 bytes; `RA <- PC + 2 instructions`, `PC <- a16` |

Machine control instructions are fully specified 16-bit opcodes:

| Encoding | Instruction |
| :--- | :--- |
| `0x0000` | `NOP` |
| `0x0100` | `RET` |
| `0x0200` | `FNCB` |
| `0x0300` | `FNCA` |
| `0x8000` | `WFI` |

### 3.5 Format S — `Inst{3-0} = 0100`

| `func` (`Inst{11-8}`) | Instruction | Field placement |
| :---: | :--- | :--- |
| `0000` | `MOV Rd, Rs` | `Rs` in `{15-12}`, `Rd` in `{7-4}` |
| `0001` | `MOV Xd, SFR` | SFR in `{13-12}`, `Xd` in `{6-4}` |
| `0010` | `MOV SFR, Xs` | `Xs` in `{14-12}`, SFR in `{5-4}` |
| `1000` | `PUSH Rs` | `Rs` in `{15-12}` |
| `1001` | `PUSH SFR` | SFR in `{13-12}`; 2 bytes |
| `1010` | `POP Rd` | `Rd` in `{7-4}` |
| `1011` | `POP SFR` | SFR in `{5-4}`; 2 bytes |

Only `RA` and `SP` are writable through `MOV SFR, Xs`.

### 3.6 Format B — `Inst{1-0} = 10`

Layout is `i10 | func | 10`, with `i10` in `Inst{15-6}`.

| `func` | Instruction | Condition |
| :---: | :--- | :--- |
| `0000` | `BEQ` | `PSW.EQ` |
| `0001` | `BNE` | `!PSW.EQ` |
| `0010` | `BLT` | `PSW.LT` |
| `0011` | `BLE` | `PSW.LT \| PSW.EQ` |
| `0100` | `BGT` | `PSW.GT` |
| `0101` | `BGE` | `PSW.GT \| PSW.EQ` |
| `0110` | `BZ` | `PSW.Z` |
| `0111` | `BNZ` | `!PSW.Z` |
| `1000` | `BC` | `PSW.Cy` |
| `1001` | `BNC` | `!PSW.Cy` |
| `1010` | `BPS` | `!PSW.S` |
| `1011` | `BNS` | `PSW.S` |
| `1100` | `JR` | always |
| `1101` | `LJR` | always; sets `RA` |
| `1110` | `JI Xs, i7` | `PC <- {Xs, i7}` |
| `1111` | `LJI Xs, i7` | as `JI`; sets `RA` |

`JI`/`LJI` replace the displacement with `i7` in `Inst{15-9}` and `Xs` in
`Inst{8-6}`.

The branch displacement is a signed count of 2-byte instruction words taken
from the instruction *after* the branch — see
[uG24-assumptions.md](uG24-assumptions.md) §3.2.

---

## 4. Where this is implemented

| Concern | File |
| :--- | :--- |
| Encodings and formats | `llvm/lib/Target/UG24/UG24InstrFormats.td`, `UG24InstrInfo.td` |
| Register file | `llvm/lib/Target/UG24/UG24RegisterInfo.td` |
| Calling convention | `llvm/lib/Target/UG24/UG24CallingConv.td` |
| Instruction selection | `UG24ISelLowering.cpp`, `UG24ISelDAGToDAG.cpp` |
| 16-bit and frame pseudo expansion | `UG24ExpandPseudo.cpp` |
| Stack frames | `UG24FrameLowering.cpp`, `UG24RegisterInfo.cpp` |
| Object emission, fixups, relocations | `MCTargetDesc/` |
| Linking | `lld/ELF/Arch/UG24.cpp` |
| Clang target and driver | `clang/lib/Basic/Targets/UG24.*`, `clang/lib/Driver/ToolChains/UG24.*` |
| Simulator | `ug24-sim/ug24sim.c` |

The encodings in §3 are covered by `llvm/test/MC/UG24/instructions.s`, which
checks each one against the byte sequence given here.
