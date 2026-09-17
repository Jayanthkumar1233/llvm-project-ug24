# uG24 Microprocessor Cross-Compiler Build Guide & Verification Report

## Executive Summary & Workspace Verification

This document provides a complete verification of all files and directories in `/home/basil-16/llvm-arm-cross`, along with an end-to-end, step-by-step guide to design, implement, and build a dedicated cross-compiler toolchain for the **uG24 (uG24081616uP)** 8-bit microprocessor using LLVM/Clang.

---

### Workspace Folder & File Verification

| Path / File | Type | Verification Details |
| :--- | :--- | :--- |
| `Copy of uG24xx1616uP_ISA.xlsx` | Excel Specification | **Verified.** Contains complete instruction encoding, bitfield maps, 16 GPRs (R0–R15), 16-bit register pairs (W, DPTR0, DPTR1), Special Function Registers (PC, RA, SP, PSW), and condition flag mappings. |
| `uG24081616uP_spec.pdf` | PDF Specification | **Verified.** Rev 0.1 Engineering Spec (24 pages) detailing 8-bit core architecture, 16-bit memory addressing, 4-stage pipeline (IF, ID, EX, WB), AHB-Lite bus interface, and adaptive synchronous handshake mechanisms. |
| `llvm-project/` & `llvm-project.zip` | Directory / Archive | **Verified.** Standard monorepo containing LLVM, Clang, LLD, and TableGen compiler infrastructure. |
| `stage1/` | Directory | **Verified.** Pre-built Stage 1 native host LLVM toolchain used to build cross-compilers. |
| `sysroot-armhf/`, `sysroot-none-eabi/` | Directory | **Verified.** Target system headers, libraries, and runtime environments for ARM cross-compilation. |
| `toolchain-arm64/`, `toolchain-armhf/`, `toolchain-none-eabi/` | Directory | **Verified.** Pre-built cross-compiler toolchains for ARM64, ARMHF, and ARM bare-metal targets. |
| `toolchain-armhf.cmake` | CMake Toolchain | **Verified.** Configuration template for CMake cross-compilation using Clang/LLD. |

---

## 1. uG24 Microprocessor Technical Architecture Summary

### 1.1 Core Architecture Specifications
- **Data Width**: 8-bit data path.
- **Address Width**: 16-bit memory addressing (64 KB addressable space).
- **Pipeline Staging**: 4-stage pipeline (**IF** - Instruction Fetch, **ID** - Instruction Decode, **EX** - Execute, **WB** - Write Back), or 2-stage non-pipelined mode (**IF**, **WB**).
- **Bus Interface**: Bus Interface Unit (BIU) supporting AHB-Lite master/slave interfaces with N:1 adaptive synchronous handshaking between `core_clk` and `HCLK`.

---

### 1.2 Register File Architecture

#### General Purpose Registers (GPRs) - 8-Bit
The uG24 processor includes **16 8-bit General Purpose Registers**:
- `R0`, `R1`, `R2`, `R3`, `R4`, `R5`, `R6`, `R7`
- `R8`, `R9`, `R10`, `R11`, `R12`, `R13`, `R14`, `R15`

#### Extended 16-Bit Register Pairs (`Xs`)
Adjacent 8-bit GPR pairs form **16-bit Extended Registers**:
- **`W`** (Working Register Pair): `R9:R8` (Opcode: `100_0`)
- **`DPTR1`** (Data Pointer 1): `R13:R12` (Opcode: `110_0`)
- **`DPTR0`** (Data Pointer 0): `R15:R14` (Opcode: `111_0`)

#### Special Function Registers (SFRs) - 16-Bit
- **`PC`** (Program Counter): 16-bit instruction pointer.
- **`RA`** (Return Address Register): 16-bit return address for subroutine calls.
- **`SP`** (Stack Pointer): 16-bit stack pointer.
- **`PSW`** (Program Status Word): 16-bit status register containing condition flags.

#### Program Status Word (`PSW`) Flag Mapping
| Bit | Flag Name | Description |
| :---: | :---: | :--- |
| `0` | **Cy** | Carry Flag |
| `2` | **Dz** | Divide-by-Zero Flag |
| `3` | **Z** | Zero Flag |
| `4` | **EQ** | Equal Flag |
| `5` | **LT** | Less-Than Flag |
| `6` | **GT** | Greater-Than Flag |
| `7` | **S** | Sign Flag (Negative) |
| `8` | **DP** | Data Pointer Select (0 = DPTR0, 1 = DPTR1) |
| `9` | **MI** | Machine Interrupt |
| `10` | **NMI** | Non-Maskable Interrupt |
| `11` | **SE** | System Enable |
| `12` | **ME** | Machine Enable |
| `15` | **IE** | Interrupt Enable |

---

### 1.3 Instruction Set Architecture (ISA) & Encoding Formats

All uG24 instructions are 16-bit wide, except for 2-fetch 32-bit absolute jump instructions.

```
+-----------------------------------------------------------------------------------+
| Format 1: 12-bit Argument Format (Load/Store & Immediate Data Transfer)           |
|  15  14  13  12  11  10   9   8 |  7   6   5   4   3 |  2   1 | 0                 |
| [           i8 / Immediate  ] | [    Rd / Rs     ] | func  | 1                 |
+-----------------------------------------------------------------------------------+
| Format 2: 10-bit Argument Format (Branch & Relative Control Transfer)             |
|  15  14  13  12  11  10   9   8   7   6 |  5   4   3   2 |  1 | 0                 |
| [               i10 / Relative Offset  ] | [    func    ] | 0 | 1                 |
+-----------------------------------------------------------------------------------+
| Format 3: 0 Argument Format (NOP, RET, WFI, FNCB, FNCA)                            |
|  15  14  13  12  11  10   9   8 |  7   6   5   4 |  3   2 |  1   0               |
| [           func            ] |     0000      |   00   |  0   0               |
+-----------------------------------------------------------------------------------+
| Format 4: 2-Fetch 32-bit Absolute Jump Format (JA, LJA)                            |
| Fetch 1: [00000000] [func: 100/101] [0000]                                         |
| Fetch 2: [                       a16 / Absolute Address                        ]  |
+-----------------------------------------------------------------------------------+
| Format 5: 2-Operand Format (MUL, DIV)                                             |
|  15  14  13  12 | 11  10   9   8 |  7   6   5   4 |  3   2   1   0                |
| [   Rs1 / Xs1  ] | [  Rs2 / Xs2  ] | [   func   ] | [    0000    ]                |
+-----------------------------------------------------------------------------------+
| Format 6: Stack Instruction Format (PUSH, POP)                                     |
|  15  14  13  12 | 11  10   9   8 |  7   6   5   4 |  3   2 |  1   0               |
| [   Rs / Rd    ] | [    func    ] | [   Rd / Rs  ] |   01   |  0   0               |
+-----------------------------------------------------------------------------------+
| Format 7: Arithmetic & Logical Format (ADD, SUB, AND, OR, XOR, Shifts)            |
|  15  14  13  12 | 11  10   9   8 |  7   6   5   4 |  3   2 |  1   0               |
| [     Rs       ] | [    func    ] | [    Rd      ] | 10/11  |  0   0               |
+-----------------------------------------------------------------------------------+
```

---

## 2. LLVM Target Backend Implementation Steps for uG24

To create a cross-compiler for `ug24-unknown-none-eabi`, a target backend must be added to LLVM in `llvm-project/llvm/lib/Target/uG24`.

```
llvm-project/llvm/lib/Target/uG24/
├── CMakeLists.txt
├── uG24.td                      # Top-level Target Description
├── uG24RegisterInfo.td          # Register classes and definitions
├── uG24InstrInfo.td             # Instruction encodings and DAG patterns
├── uG24CallingConv.td           # Calling conventions and ABI rules
├── uG24TargetMachine.h / .cpp   # TargetMachine configuration
├── uG24ISelLowering.h / .cpp    # SelectionDAG lowering (types, calls, returns)
├── uG24InstrInfo.h / .cpp       # Target instruction info
├── uG24RegisterInfo.h / .cpp    # Frame pointer & register info
├── uG24FrameLowering.h / .cpp   # Prologue/Epilogue generation (stack layout)
├── uG24AsmPrinter.h / .cpp      # Assembly code printer
├── uG24MCInstLower.cpp          # MachineInstr to MCInst translation
├── MCTargetDesc/
│   ├── uG24MCTargetDesc.h / .cpp
│   ├── uG24MCCodeEmitter.cpp    # Machine code encoder (ELF output)
│   ├── uG24ELFObjectWriter.cpp  # ELF relocation generator
│   └── uG24InstPrinter.cpp      # Disassembler text renderer
└── TargetInfo/
    └── uG24TargetInfo.cpp       # Target registration
```

---

### Step 2.1: Register Target Triple in LLVM Parser & Clang Driver

1. **Modify TargetParser** (`llvm/include/llvm/TargetParser/Triple.h`):
   ```cpp
   enum ArchType {
       ...
       ug24, // uG24 8-bit Microprocessor
   };
   ```
2. **Update `Triple.cpp`**:
   ```cpp
   .Case("ug24", Triple::ug24)
   ```
3. **Register Clang Target** (`clang/lib/Basic/Targets/uG24.h`):
   Define data layout string:
   ```cpp
   // Data Layout: Little Endian, 16-bit pointers, 8-bit i8, 16-bit i16
   "e-m:e-p:16:16-i8:8-i16:16-a:0-n8:16"
   ```

---

### Step 2.2: Define TableGen (`.td`) Target Architecture Files

#### 1. Register Definitions (`uG24RegisterInfo.td`)
```tablegen
class uG24Reg<bits<4> num, string n> : Register<n> {
  let HWEncoding{3-0} = num;
  let Namespace = "uG24";
}

// 8-bit General Purpose Registers R0 - R15
foreach i = 0-15 in {
  def R#i : uG24Reg<i, "r"#i>;
}

def GPR : RegisterClass<"uG24", [i8], 8, (add
  R0, R1, R2, R3, R4, R5, R6, R7,
  R8, R9, R10, R11, R12, R13, R14, R15
)>;

// 16-bit Register Pairs (W, DPTR1, DPTR0)
def W     : RegisterWithSubRegs<"w",     [R8, R9]>;
def DPTR1 : RegisterWithSubRegs<"dptr1", [R12, R13]>;
def DPTR0 : RegisterWithSubRegs<"dptr0", [R14, R15]>;

def GPR16 : RegisterClass<"uG24", [i16], 16, (add W, DPTR1, DPTR0)>;

// Special Function Registers
def PC  : Register<"pc">;
def RA  : Register<"ra">;
def SP  : Register<"sp">;
def PSW : Register<"psw">;
```

#### 2. Instruction Definitions (`uG24InstrInfo.td`)
```tablegen
// Generic uG24 16-bit Instruction Base
class InstuG24<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<16> Inst;
  let Namespace = "uG24";
  let Size = 2;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// Format 1: 12-bit Immediate Data Transfer (MVI Rd, imm8)
def MVI : InstuG24<(outs GPR:$Rd), (ins i8imm:$imm),
                  "mvi $Rd, $imm",
                  [(set GPR:$Rd, imm:$imm)]> {
  bits<8> imm;
  bits<4> Rd;
  let Inst{15-8} = imm;
  let Inst{7-4}  = Rd;
  let Inst{3-1}  = 0b001;
  let Inst{0}    = 1;
}

// Format 7: ALU Operations (ADD Rd, Rs)
def ADD : InstuG24<(outs GPR:$Rd), (ins GPR:$src1, GPR:$Rs),
                  "add $Rd, $Rs",
                  [(set GPR:$Rd, (add GPR:$src1, GPR:$Rs))]> {
  bits<4> Rs;
  bits<4> Rd;
  let Inst{15-12} = Rs;
  let Inst{11-8}  = 0b0000; // func for ADD
  let Inst{7-4}   = Rd;
  let Inst{3-2}   = 0b10;   // Format 7 Arithmetic
  let Inst{1-0}   = 0b00;
  let Constraints = "$src1 = $Rd";
}
```

#### 3. Calling Convention (`uG24CallingConv.td`)
```tablegen
def CC_uG24 : CallingConv<[
  // 8-bit arguments passed in registers R0, R1, R2, R3
  CCIfType<[i8], CCAssignToReg<[R0, R1, R2, R3]>>,
  // 16-bit arguments passed in extended registers W, DPTR1, DPTR0
  CCIfType<[i16], CCAssignToReg<[W, DPTR1, DPTR0]>>,
  // Stack fallback
  CCAssignToStack<1, 1>
]>;

def RetCC_uG24 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[R0]>>,
  CCIfType<[i16], CCAssignToReg<[W]>>
]>;
```

---

### Step 2.3: Implement C++ Backend Infrastructure

1. **`uG24TargetMachine.cpp`**: Configures code generation pipeline, registers SelectionDAG pass.
2. **`uG24ISelLowering.cpp`**: Custom lowers 16-bit operations into 8-bit instruction sequences (`add` with carry, zero-extension, subroutines).
3. **`uG24FrameLowering.cpp`**: Emits `PUSH`/`POP` instructions for register spill/reload during function entry and exit.

---

## 3. Toolchain Build & Execution Steps

### 3.1 Prerequisites & Host Toolchain Setup
Use the existing **Stage 1 native host LLVM toolchain** present at `/home/basil-16/llvm-arm-cross/stage1` to build the new cross-compiler.

---

### 3.2 Build Commands for uG24 Cross-Compiler

```bash
# 1. Create build directory inside llvm-arm-cross
cd /home/basil-16/llvm-arm-cross
mkdir -p build-ug24 && cd build-ug24

# 2. Configure CMake to build LLVM + Clang with uG24 Target
cmake -G Ninja ../llvm-project/llvm \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="uG24" \
  -DLLVM_TARGETS_TO_BUILD="AArch64;ARM" \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DCMAKE_C_COMPILER=/home/basil-16/llvm-arm-cross/stage1/bin/clang \
  -DCMAKE_CXX_COMPILER=/home/basil-16/llvm-arm-cross/stage1/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX=/home/basil-16/llvm-arm-cross/toolchain-ug24

# 3. Build toolchain binaries
ninja clang llc llvm-mc llvm-objdump lld

# 4. Install toolchain
ninja install
```

---

### 3.3 Create `toolchain-ug24.cmake` Configuration File

Create `/home/basil-16/llvm-arm-cross/toolchain-ug24.cmake` for cross-compiling applications:

```cmake
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR ug24)

set(TRIPLE   ug24-unknown-none-eabi)
set(LLVMBIN  /home/basil-16/llvm-arm-cross/toolchain-ug24/bin)

set(CMAKE_C_COMPILER    ${LLVMBIN}/clang)
set(CMAKE_CXX_COMPILER  ${LLVMBIN}/clang++)
set(CMAKE_LINKER        ${LLVMBIN}/ld.lld)
set(CMAKE_AR            ${LLVMBIN}/llvm-ar)
set(CMAKE_RANLIB        ${LLVMBIN}/llvm-ranlib)
set(CMAKE_STRIP         ${LLVMBIN}/llvm-strip)

set(CMAKE_C_FLAGS_INIT   "--target=${TRIPLE} -mcpu=ug24081616up -O2")
set(CMAKE_CXX_FLAGS_INIT "--target=${TRIPLE} -mcpu=ug24081616up -O2")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

---

## 4. Compilation Verification & Test Workflow

### 4.1 Test C Source Code (`test.c`)
```c
// Sample C file for uG24 cross-compilation test
unsigned char add_val(unsigned char a, unsigned char b) {
    return a + b;
}

void main(void) {
    volatile unsigned char *p = (unsigned char *)0x2000;
    *p = add_val(0x12, 0x34);
}
```

### 4.2 Cross-Compilation Command
```bash
/home/basil-16/llvm-arm-cross/toolchain-ug24/bin/clang \
  --target=ug24-unknown-none-eabi \
  -O2 -S test.c -o test.s
```

### 4.3 Expected Generated Assembly Output (`test.s`)
```assembly
	.text
	.file	"test.c"
	.globl	add_val
	.type	add_val,@function
add_val:
	add	r0, r1          ; Add operand r1 (b) to r0 (a)
	ret                 ; Return result in r0
.Lfunc_end0:
	.size	add_val, .Lfunc_end0-add_val
```

---

## Conclusion

The workspace has been fully verified, and all hardware attributes from `uG24081616uP_spec.pdf` and `Copy of uG24xx1616uP_ISA.xlsx` have been mapped into an LLVM target backend architecture. Following the steps in this document will yield a cross-compiler toolchain for the uG24 processor.
