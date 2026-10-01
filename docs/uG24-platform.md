# The uG24 platform contract

Everything an independently written loader, simulator or board-support package
has to know that **is not in the ISA specification or the encoding
spreadsheet**, and how to get it out of the ELF file instead of agreeing it by
e-mail.

---

## 1. Why a second simulator cannot find the console

`uG24081616uP_spec.pdf` and `Copy of uG24xx1616uP_ISA.xlsx` describe a
*processor core*: sixteen registers, eight pairs, the PSW bits, and the
sixteen-bit encoding of every instruction. The toolchain is checked against
them directly — `ug24-tests/verify-against-isa-xlsx.py` rebuilds all 69
encodings from the spreadsheet's bit columns and compares them with
`llvm-mc`.

Neither document mentions a UART, because the core does not contain one. A
`ST R0, [addr]` to a peripheral is an ordinary store as far as the core is
concerned; what answers at that address is decided by the SoC that wraps the
core, and that decision lives in a different document than the two above.

So a simulator written from these two documents alone **cannot** derive where
console output goes. It has no way to. If it prints nothing when running a
program that clearly executes, the peripheral map is the first thing to
suspect, not the instruction set.

The useful corollary: when a second, independently written simulator runs
these ELF files correctly *once the console address is supplied*, everything
the two documents do determine — the encodings, the register file, the flags,
the ABI, the ELF layout — has been cross-checked by a second implementation.
The missing address is a gap in the paperwork, not a defect in either
simulator.

## 2. The map

This is the map `ug24-runtime/ug24.ld` uses. It is **this project's choice**,
not the vendor's: specification query G5 asks for the real one and has not been
answered. Treat it as a default that can move (see §3).

| Address | Register | Direction | Meaning |
| :--- | :--- | :--- | :--- |
| `0xFF00` | `UART_TX` | write | transmit this byte on the console |
| `0xFF01` | `UART_STATUS` | read | bit 0 set when the transmitter will accept a byte |
| `0xFF02` | `SIM_EXIT` | write | stop the core; the byte is the exit status |
| `0xFF10` | `IRQ_STATUS` | read / write | pending sources; writing a 1 bit clears that source |
| `0xFF11` | `IRQ_ENABLE` | read / write | per-source enable mask |
| `0xFF12` | `IRQ_RAISE` | write | raise source *n* by hand, *n* in bits 0-2 |
| `0xFF13` | `TIMER_LOAD` | write | instructions between timer ticks; 0 disables |

Source bit 0 is the timer, bit 1 is software. The whole page `0xFF00-0xFFFF`
is reserved for peripherals and kept out of the linker's `MEM` region, so no
program object is ever placed there.

## 3. Getting it out of the ELF instead

Since the address is a platform decision rather than an architectural fact, the
linker publishes it. Every register above is an **absolute symbol in the
image's symbol table**, and each one is an offset from the linker script's
`MMIO` region rather than a literal:

| Symbol | Value in a stock image |
| :--- | :--- |
| `__mmio_base` | `0xFF00` |
| `__mmio_end` | `0xFFFF` |
| `__ug24_uart_tx` | `0xFF00` |
| `__ug24_uart_status` | `0xFF01` |
| `__ug24_sim_exit` | `0xFF02` |
| `__ug24_irq_status` | `0xFF10` |
| `__ug24_irq_enable` | `0xFF11` |
| `__ug24_irq_raise` | `0xFF12` |
| `__ug24_timer_load` | `0xFF13` |

A loader that reads these needs no agreement with whoever built the image, and
keeps working when an SoC decodes its peripherals somewhere else.

`ug24_platform.s` in the runtime carries the same addresses as **weak** absolute
definitions, for a program linked with its own script (`-T`) rather than
`ug24.ld`. A linker-script assignment overrides them, so with the stock script
they are never used and the archive member is not even extracted. A custom
script that moves the `MMIO` window should copy the assignment block out of
`ug24.ld`; leaving it out means silently falling back to `0xFF00`. `ug24.h`
resolves `UG24_UART_TX` to the same symbols, so the runtime moves with the
script too — changing the two `ORIGIN` lines in `ug24-runtime/ug24.ld` from
`0xFF00` to `0xFE00` relocates the console, the runtime and the simulator
together, with no source change anywhere.

Check what an image asks for:

```sh
llvm-readelf -s prog.elf | grep -E '__ug24_|__mmio_'
ug24-sim/ug24sim prog.elf --quiet --io-map    # what the simulator bound
```

`--io-map` also says whether the values came from the image or from built-in
defaults, which is the fastest way to tell a stripped ELF from a disagreement.

### Reading them in a third-party simulator

The symbols are ordinary `SHT_SYMTAB` entries, so about thirty lines suffice.
`ug24-sim/ug24sim.c` does exactly this in `load_elf` and `bind_periph`; the
shape is:

```c
/* For each Elf32_Sym in SHT_SYMTAB, with `name` from the linked strtab: */
static const struct { const char *name; uint16_t *slot; } map[] = {
    { "__mmio_base",        &io.base        },
    { "__mmio_end",         &io.end         },
    { "__ug24_uart_tx",     &io.uart_tx     },
    { "__ug24_uart_status", &io.uart_status },
    { "__ug24_sim_exit",    &io.sim_exit    },
    { "__ug24_irq_status",  &io.irq_status  },
    { "__ug24_irq_enable",  &io.irq_enable  },
    { "__ug24_irq_raise",   &io.irq_raise   },
    { "__ug24_timer_load",  &io.timer_load  },
};
for (i = 0; i < sizeof map / sizeof map[0]; i++)
    if (!strcmp(name, map[i].name))
        *map[i].slot = (uint16_t)sym.st_value;
```

Then decode against `io.*` rather than against constants. Two cautions:

* **Every symbol is optional.** Keep the table in §2 as the fallback so that an
  image publishing none still runs.
* **`strip` removes the symbol table.** A stripped image carries no map; either
  keep images unstripped for cross-checking, or fall back to §2. (A `SHT_NOTE`
  section would survive stripping and is the obvious next step if stripped
  images ever need to be exchanged. `ug24.ld` currently discards `.note.*`.)

## 4. The rest of the platform, also not in the ISA documents

A loader for these ELF files needs these too, and they are equally
project conventions rather than vendor facts.

| | Value | Where it comes from |
| :--- | :--- | :--- |
| `e_machine` | **`0xBA51`** (`EM_UG24`) | chosen by the hardware team, 1 Oct 2026; not registered with the generic ELF ABI |
| Target triple | `ug24-unknown-elf` | confirmed |
| ELF class / endianness | ELF32, little-endian | confirmed |
| Image base | `0x0000`, page size 1 | `lld/ELF/Arch/UG24.cpp` |
| ROM | `0x0000`-`0x7FFF`, holding the vectors, `.text` and `.rodata` | confirmed |
| RAM | `0x8000`-`0xFEFF`, holding `.data`, `.bss`, the heap and the stack | confirmed, less the peripheral page |
| Entry | `e_entry`, **not always `0x0000`** | `ENTRY(_start)` |
| Vector table | four slots at `0x0000`, one `LJA` each: reset, NMI, maskable, software | **assumption**, query A1/A6 |
| Interrupt entry | hardware pushes PC then PSW; return is `POP PSW` + `POP PC` | **assumption**, query A1/A6 |
| Reset `SP` | strapped on real hardware; the simulator seeds it from `__stack_top` | `ug24.ld`, and `crt0.s` loads it anyway |
| Normal exit | `exit()` writes `SIM_EXIT`; a `main` that returns falls into `wfi` / `jr` | `crt0.s`, `ug24_stdlib.c` |
| Stack window | `__heap_end` .. `__stack_top` (`0xFEFE`), both in the symbol table | `ug24.ld` |

Relocation numbers, also confirmed: 0 `R_UG2408_NONE`, 1 `R_UG2408_16`,
2 `R_UG2408_PCREL10`, 3 `R_UG2408_8`, 4 `R_UG2408_LO8`, 5 `R_UG2408_HI8`, and
6 `R_UG2408_32` as a local extension for `.long symbol`. A loader of executables
does not see these — the linker has already applied them — but anything reading
relocatable objects does.

### Two things that changed on 1 October 2026

`e_machine` **was `0x9240`** and is now `0xBA51`. A simulator that checks it
rejects or warns about every image built after that date until the constant is
updated. The relocation names and numbers changed with it.

The memory map **was one flat 64 KB** and is now split, so `.data` has a load
address in ROM distinct from its run address in RAM. A loader that was getting
away with ignoring `p_paddr` stops working at that point, silently and only for
initialised variables.

Read `e_entry` rather than assuming `0x0000`, and load by `p_paddr` from the
program headers rather than by section — with the confirmed ROM/RAM split,
`.data`'s load and run addresses are now always different.

A core that halts on `wfi` when no interrupt can arrive, and waits when one
still can, matches `crt0`'s halt loop. Halting unconditionally also works for
programs that do not use interrupts; waiting unconditionally hangs at the end
of every program, which is a common first symptom in a fresh simulator.

## 5. What to do about the addresses themselves

The map in §2 is an assumption, recorded as one in
`docs/uG24-assumptions.md`. Three ways out, in increasing order of finality:

1. **Share this file.** Enough for two implementations to agree today.
2. **Read the symbols (§3).** No agreement needed at all, and survives the map
   moving. This is what `ug24sim` does.
3. **Get query G5 answered.** The 1 October 2026 answers confirmed the ROM/RAM
   split but put RAM up to `0xFFFF` with the stack top there, which leaves the
   console nowhere to live, so this is still open. When the hardware team says
   where the SoC decodes its console, change the `MEMORY` block in
   `ug24-runtime/ug24.ld`, rebuild the runtime, and every image and every loader
   that reads the symbols follows.
   Nothing in the compiler changes: the code generator knows nothing about
   peripherals, which is deliberate and is checked by
   `ug24-tests/verify-against-isa-xlsx.py` plus the absence of any runtime
   symbol name in `llvm/lib/Target/UG24`.
