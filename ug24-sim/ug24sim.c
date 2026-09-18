//===-- ug24sim.c - Instruction set simulator for the uG24 ---------------===//
//
// Loads a statically linked uG24 ELF executable into a flat 64 KB memory and
// interprets it.  The decode tables follow "Copy of uG24xx1616uP_ISA.xlsx"
// exactly; where the specification is ambiguous the choice made here is
// called out in a comment and matches what the compiler assumes.
//
// Build:  cc -O2 -o ug24sim ug24sim.c
// Run:    ./ug24sim program.elf [--trace] [--max N]
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define MEM_SIZE 0x10000

// Memory-mapped I/O.  A real uG24 SoC decodes peripherals somewhere in the
// 64 KB space; the simulator models the smallest useful set at the top of
// memory, just below the stack.  ug24-runtime/include/ug24.h and the MEMORY
// block in ug24.ld must agree with these addresses.
#define UART_TX     0xFF00u  // write: emit the byte on the console
#define UART_STATUS 0xFF01u  // read: bit 0 set when the transmitter is ready
#define SIM_EXIT    0xFF02u  // write: stop the simulator with this exit code

// Interrupt controller.  The uG24 PSW reserves IE/ME/SE/NMI/MI, but the
// specification available here does not say where the vector table lives or
// which peripheral owns which source, so the controller below is the model
// documented in docs/uG24-assumptions.md.  Everything in this block is an
// assumption and is expected to change when the vendor answers A1/A6.
#define IRQ_STATUS  0xFF10u  // read: pending sources; write: 1 bits clear
#define IRQ_ENABLE  0xFF11u  // read/write: per-source enable mask
#define IRQ_RAISE   0xFF12u  // write: raise source (bit number) by hand
#define TIMER_LOAD  0xFF13u  // write: instructions between timer ticks, 0=off

#define IRQ_SOURCE_TIMER 0x01u
#define IRQ_SOURCE_SW    0x02u

// Vector slots.  Each holds one LJA, which is four bytes wide.
#define VECTOR_RESET 0x0000u
#define VECTOR_NMI   0x0004u
#define VECTOR_IRQ   0x0008u
#define VECTOR_SWI   0x000Cu

#define MMIO_BASE   0xFF00u
#define MMIO_END    0xFF1Fu

// PSW bit positions, from the uG24 register spreadsheet.
enum {
    PSW_CY = 0,
    PSW_DZ = 2,
    PSW_Z  = 3,
    PSW_EQ = 4,
    PSW_LT = 5,
    PSW_GT = 6,
    PSW_S  = 7,
    PSW_DP = 8,
    PSW_MI = 11,  // a maskable interrupt is being serviced
    PSW_NM = 12,  // a non-maskable interrupt is being serviced
    PSW_SE = 13,  // software interrupt enable
    PSW_ME = 14,  // maskable interrupt enable
    PSW_IE = 15,  // global interrupt enable
};

typedef struct {
    uint8_t  mem[MEM_SIZE];
    uint8_t  r[16];
    uint16_t pc, ra, sp, psw;
    uint64_t cycles;
    int      halted;
    int      exit_code;
    int      trace;

    // The stack window, read from __heap_end and __stack_top in the ELF
    // symbol table.  A stack that grows past either end is a spec exception
    // (SE), and on this part nothing catches it in hardware -- the program
    // just corrupts the heap and carries on.  Checking it here turns silent
    // corruption into a diagnostic.  Both zero means the symbols were absent
    // and the check is skipped.
    uint16_t stack_low, stack_high;
    int      stack_checked;

    // Interrupt controller state.
    uint8_t  irq_pending;
    uint8_t  irq_enable;
    uint8_t  timer_load;     // 0 disables the timer
    uint32_t timer_count;    // instructions until the next tick
    uint64_t irq_taken;      // how many interrupts were delivered
} Core;

// All data accesses go through these so that the peripheral window behaves
// differently from plain memory.  Instruction fetch does not: code cannot be
// executed out of the MMIO region.
static uint8_t mem_read(Core *c, uint16_t addr) {
    if (addr >= MMIO_BASE && addr <= MMIO_END) {
        switch (addr) {
        case UART_STATUS: return 1;   // always ready to accept a byte
        case IRQ_STATUS:  return c->irq_pending;
        case IRQ_ENABLE:  return c->irq_enable;
        case TIMER_LOAD:  return c->timer_load;
        default:          return 0;
        }
    }
    return c->mem[addr];
}

static void mem_write(Core *c, uint16_t addr, uint8_t value) {
    if (addr >= MMIO_BASE && addr <= MMIO_END) {
        switch (addr) {
        case UART_TX:
            fputc(value, stdout);
            fflush(stdout);
            break;
        case SIM_EXIT:
            c->exit_code = value;
            c->halted = 1;
            break;
        case IRQ_STATUS:
            // Write-one-to-clear, which is what a handler does on the way out.
            c->irq_pending &= (uint8_t)~value;
            break;
        case IRQ_ENABLE:
            c->irq_enable = value;
            break;
        case IRQ_RAISE:
            c->irq_pending |= (uint8_t)(1u << (value & 7));
            break;
        case TIMER_LOAD:
            c->timer_load = value;
            c->timer_count = value;
            break;
        default:
            break;
        }
        return;
    }
    c->mem[addr] = value;
}

static uint16_t rd16(Core *c, uint16_t a) {
    return (uint16_t)(c->mem[a] | (c->mem[(uint16_t)(a + 1)] << 8));
}

// The three extended register pairs, indexed by their 3-bit code.
static uint16_t x_get(Core *c, unsigned code) {
    unsigned lo = code * 2;
    return (uint16_t)(c->r[lo] | (c->r[lo + 1] << 8));
}
static void x_set(Core *c, unsigned code, uint16_t v) {
    unsigned lo = code * 2;
    c->r[lo] = (uint8_t)v;
    c->r[lo + 1] = (uint8_t)(v >> 8);
}

static const char *x_name(unsigned code) {
    switch (code) {
    case 4: return "w";
    case 6: return "dptr1";
    case 7: return "dptr0";
    default: return "x?";
    }
}

static void set_bit(Core *c, int bit, int value) {
    if (value)
        c->psw |= (uint16_t)(1u << bit);
    else
        c->psw &= (uint16_t)~(1u << bit);
}
static int get_bit(Core *c, int bit) { return (c->psw >> bit) & 1; }

// Flags common to the arithmetic and logical instructions.
static void set_nz(Core *c, uint8_t result) {
    set_bit(c, PSW_Z, result == 0);
    set_bit(c, PSW_S, (result & 0x80) != 0);
}

// The base address used by LD and ST: PSW.DP selects DPTR1 over DPTR0.
static uint16_t data_base(Core *c) {
    return get_bit(c, PSW_DP) ? x_get(c, 6) : x_get(c, 7);
}

//===----------------------------------------------------------------------===//
// ELF loading
//===----------------------------------------------------------------------===//

#define EM_UG24 0x9240

static int load_elf(Core *c, const char *path, uint16_t *entry) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }

    unsigned char hdr[52];
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
        fprintf(stderr, "%s: too short to be an ELF file\n", path);
        fclose(f); return -1;
    }
    if (memcmp(hdr, "\177ELF", 4) != 0 || hdr[4] != 1 || hdr[5] != 1) {
        fprintf(stderr, "%s: not a 32-bit little-endian ELF file\n", path);
        fclose(f); return -1;
    }

    unsigned machine = hdr[18] | (hdr[19] << 8);
    if (machine != EM_UG24)
        fprintf(stderr, "warning: e_machine is 0x%x, expected 0x%x (uG24)\n",
                machine, EM_UG24);

    uint32_t e_entry  = hdr[24] | (hdr[25]<<8) | (hdr[26]<<16) | ((uint32_t)hdr[27]<<24);
    uint32_t e_phoff  = hdr[28] | (hdr[29]<<8) | (hdr[30]<<16) | ((uint32_t)hdr[31]<<24);
    unsigned e_phentsize = hdr[42] | (hdr[43] << 8);
    unsigned e_phnum     = hdr[44] | (hdr[45] << 8);

    for (unsigned i = 0; i < e_phnum; i++) {
        unsigned char ph[32];
        if (fseek(f, (long)(e_phoff + (uint32_t)i * e_phentsize), SEEK_SET) != 0) break;
        if (fread(ph, 1, sizeof ph, f) != sizeof ph) break;

        uint32_t p_type   = ph[0] | (ph[1]<<8) | (ph[2]<<16) | ((uint32_t)ph[3]<<24);
        uint32_t p_offset = ph[4] | (ph[5]<<8) | (ph[6]<<16) | ((uint32_t)ph[7]<<24);
        uint32_t p_paddr  = ph[12]| (ph[13]<<8)| (ph[14]<<16)| ((uint32_t)ph[15]<<24);
        uint32_t p_filesz = ph[16]| (ph[17]<<8)| (ph[18]<<16)| ((uint32_t)ph[19]<<24);
        uint32_t p_memsz  = ph[20]| (ph[21]<<8)| (ph[22]<<16)| ((uint32_t)ph[23]<<24);

        if (p_type != 1 /* PT_LOAD */) continue;
        if (p_paddr + p_memsz > MEM_SIZE) {
            fprintf(stderr, "segment at 0x%x does not fit in 64 KB\n", p_paddr);
            fclose(f); return -1;
        }
        if (fseek(f, (long)p_offset, SEEK_SET) != 0) break;
        if (p_filesz && fread(c->mem + p_paddr, 1, p_filesz, f) != p_filesz) {
            fprintf(stderr, "short read loading segment at 0x%x\n", p_paddr);
            fclose(f); return -1;
        }
        memset(c->mem + p_paddr + p_filesz, 0, p_memsz - p_filesz);
    }

    // Find __heap_end and __stack_top so that the stack can be bounds-checked.
    uint32_t e_shoff = hdr[32] | (hdr[33]<<8) | (hdr[34]<<16) | ((uint32_t)hdr[35]<<24);
    unsigned e_shentsize = hdr[46] | (hdr[47] << 8);
    unsigned e_shnum     = hdr[48] | (hdr[49] << 8);
    for (unsigned i = 0; i < e_shnum; i++) {
        unsigned char sh[40];
        if (fseek(f, (long)(e_shoff + (uint32_t)i * e_shentsize), SEEK_SET)) break;
        if (fread(sh, 1, sizeof sh, f) != sizeof sh) break;
        uint32_t sh_type = sh[4] | (sh[5]<<8) | (sh[6]<<16) | ((uint32_t)sh[7]<<24);
        if (sh_type != 2 /* SHT_SYMTAB */) continue;

        uint32_t sym_off  = sh[16]| (sh[17]<<8)| (sh[18]<<16)| ((uint32_t)sh[19]<<24);
        uint32_t sym_size = sh[20]| (sh[21]<<8)| (sh[22]<<16)| ((uint32_t)sh[23]<<24);
        uint32_t link     = sh[24]| (sh[25]<<8)| (sh[26]<<16)| ((uint32_t)sh[27]<<24);

        unsigned char strsh[40];
        if (fseek(f, (long)(e_shoff + link * e_shentsize), SEEK_SET)) break;
        if (fread(strsh, 1, sizeof strsh, f) != sizeof strsh) break;
        uint32_t str_off = strsh[16]|(strsh[17]<<8)|(strsh[18]<<16)|((uint32_t)strsh[19]<<24);

        for (uint32_t o = 0; o + 16 <= sym_size; o += 16) {
            unsigned char sym[16];
            if (fseek(f, (long)(sym_off + o), SEEK_SET)) break;
            if (fread(sym, 1, sizeof sym, f) != sizeof sym) break;
            uint32_t name = sym[0] | (sym[1]<<8) | (sym[2]<<16) | ((uint32_t)sym[3]<<24);
            uint32_t val  = sym[4] | (sym[5]<<8) | (sym[6]<<16) | ((uint32_t)sym[7]<<24);
            if (!name) continue;
            char buf[32];
            if (fseek(f, (long)(str_off + name), SEEK_SET)) break;
            size_t got = fread(buf, 1, sizeof buf - 1, f);
            buf[got] = '\0';
            if (!strcmp(buf, "__heap_end"))  { c->stack_low  = (uint16_t)val; }
            else if (!strcmp(buf, "__stack_top")) { c->stack_high = (uint16_t)val; }
        }
        break;
    }
    c->stack_checked = c->stack_low && c->stack_high && c->stack_low < c->stack_high;

    fclose(f);
    *entry = (uint16_t)e_entry;
    return 0;
}

// Called after every instruction that can move SP.
static void check_stack(Core *c) {
    if (!c->stack_checked || c->halted) return;
    if (c->sp < c->stack_low) {
        fprintf(stderr, "stack overflow: sp=%04x fell below __heap_end=%04x\n",
                c->sp, c->stack_low);
        c->halted = 3;
    } else if (c->sp > c->stack_high) {
        fprintf(stderr, "stack underflow: sp=%04x rose above __stack_top=%04x\n",
                c->sp, c->stack_high);
        c->halted = 3;
    }
}

static void trace(Core *c, uint16_t pc, const char *fmt, ...);

//===----------------------------------------------------------------------===//
// Interrupts
//===----------------------------------------------------------------------===//
//
// The model, all of it an assumption pending the vendor's answer to A1/A6:
//
//   * A source becomes pending in IRQ_STATUS.  It is delivered when it is
//     also set in IRQ_ENABLE, and PSW.IE and PSW.ME are both set.
//   * Delivery pushes the interrupted PC and then PSW, clears PSW.IE so the
//     handler is not immediately re-entered, sets PSW.MI, and jumps to the
//     maskable vector.
//   * The handler returns with "pop psw" followed by "pop pc", which is what
//     UG24ExpandPseudo emits in place of RET for an interrupt function.  PSW
//     comes back with IE as it was, so interrupts re-enable on return.
//   * Clearing the source is the handler's job: write its bit to IRQ_STATUS.
//     A source left pending is delivered again as soon as the handler
//     returns, which is how real edge/level confusion shows up, so the test
//     programs clear it.

static void tick_timer(Core *c) {
    if (!c->timer_load)
        return;
    if (--c->timer_count == 0) {
        c->timer_count = c->timer_load;
        c->irq_pending |= IRQ_SOURCE_TIMER;
    }
}

static void deliver_interrupt(Core *c) {
    if (!(c->irq_pending & c->irq_enable))
        return;
    if (!get_bit(c, PSW_IE) || !get_bit(c, PSW_ME))
        return;

    uint16_t saved_psw = c->psw;

    // PC first, then PSW, so that "pop psw; pop pc" unwinds them in order.
    c->sp = (uint16_t)(c->sp - 2);
    c->mem[c->sp] = (uint8_t)c->pc;
    c->mem[(uint16_t)(c->sp + 1)] = (uint8_t)(c->pc >> 8);
    c->sp = (uint16_t)(c->sp - 2);
    c->mem[c->sp] = (uint8_t)saved_psw;
    c->mem[(uint16_t)(c->sp + 1)] = (uint8_t)(saved_psw >> 8);

    set_bit(c, PSW_IE, 0);
    set_bit(c, PSW_MI, 1);
    c->irq_taken++;
    trace(c, c->pc, "interrupt -> %04x (pending=%02x sp=%04x)",
          VECTOR_IRQ, c->irq_pending, c->sp);
    c->pc = VECTOR_IRQ;
}

//===----------------------------------------------------------------------===//
// Execution
//===----------------------------------------------------------------------===//

static void trace(Core *c, uint16_t pc, const char *fmt, ...) {
    if (!c->trace) return;
    va_list ap;
    fprintf(stderr, "%04x: ", pc);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void step(Core *c) {
    uint16_t pc = c->pc;
    uint16_t insn = rd16(c, pc);
    c->pc = (uint16_t)(pc + 2);
    c->cycles++;

    // Format I: Inst{0} == 1.
    if (insn & 1) {
        unsigned func = (insn >> 1) & 0x7;
        if (func == 0x2) { // ST Rs, i8 - the only form with the register high
            unsigned rs = (insn >> 12) & 0xf;
            uint8_t disp = (uint8_t)((insn >> 4) & 0xff);
            uint16_t addr = (uint16_t)(data_base(c) + disp);
            mem_write(c, addr, c->r[rs]);
            trace(c, pc, "st r%u, [%u] -> [%04x]=%02x", rs, disp, addr, c->r[rs]);
            return;
        }

        uint8_t imm = (uint8_t)(insn >> 8);
        unsigned rd = (insn >> 4) & 0xf;
        unsigned res;
        switch (func) {
        case 0x0: { // LD Rd, i8
            uint16_t addr = (uint16_t)(data_base(c) + imm);
            c->r[rd] = mem_read(c, addr);
            trace(c, pc, "ld r%u, [%u] <- [%04x]=%02x", rd, imm, addr, c->r[rd]);
            return;
        }
        case 0x1: // MVI
            c->r[rd] = imm;
            trace(c, pc, "mvi r%u, %u", rd, imm);
            return;
        case 0x3: // ANDI
            c->r[rd] &= imm; set_nz(c, c->r[rd]);
            trace(c, pc, "andi r%u, %u -> %02x", rd, imm, c->r[rd]);
            return;
        case 0x4: // ORI
            c->r[rd] |= imm; set_nz(c, c->r[rd]);
            trace(c, pc, "ori r%u, %u -> %02x", rd, imm, c->r[rd]);
            return;
        case 0x5: // XORI
            c->r[rd] ^= imm; set_nz(c, c->r[rd]);
            trace(c, pc, "xori r%u, %u -> %02x", rd, imm, c->r[rd]);
            return;
        case 0x6: // ADI
            res = (unsigned)c->r[rd] + imm;
            set_bit(c, PSW_CY, res > 0xff);
            c->r[rd] = (uint8_t)res; set_nz(c, c->r[rd]);
            trace(c, pc, "adi r%u, %u -> %02x", rd, imm, c->r[rd]);
            return;
        case 0x7: // SBI
            res = (unsigned)c->r[rd] - imm;
            set_bit(c, PSW_CY, c->r[rd] < imm);
            c->r[rd] = (uint8_t)res; set_nz(c, c->r[rd]);
            trace(c, pc, "sbi r%u, %u -> %02x", rd, imm, c->r[rd]);
            return;
        default:
            break;
        }
    }

    // Format B: Inst{1-0} == 10.
    if ((insn & 3) == 2) {
        unsigned func = (insn >> 2) & 0xf;

        if (func == 0xe || func == 0xf) { // JI / LJI: PC <- {Xs, i7}
            unsigned imm7 = (insn >> 9) & 0x7f;
            unsigned xs = (insn >> 6) & 0x7;
            uint16_t target = (uint16_t)((x_get(c, xs) & 0xff80) | imm7);
            if (func == 0xf) c->ra = c->pc;
            trace(c, pc, "%s %s, %u -> %04x", func == 0xe ? "ji" : "lji",
                  x_name(xs), imm7, target);
            c->pc = target;
            return;
        }

        // The encoded field is a signed count of instruction words applied to
        // the address of the following instruction.
        int32_t words = (int32_t)((insn >> 6) & 0x3ff);
        if (words & 0x200) words -= 0x400;
        uint16_t target = (uint16_t)(c->pc + words * 2);

        int taken;
        const char *name;
        switch (func) {
        case 0x0: taken = get_bit(c, PSW_EQ);  name = "beq"; break;
        case 0x1: taken = !get_bit(c, PSW_EQ); name = "bne"; break;
        case 0x2: taken = get_bit(c, PSW_LT);  name = "blt"; break;
        case 0x3: taken = get_bit(c, PSW_LT) || get_bit(c, PSW_EQ); name = "ble"; break;
        case 0x4: taken = get_bit(c, PSW_GT);  name = "bgt"; break;
        case 0x5: taken = get_bit(c, PSW_GT) || get_bit(c, PSW_EQ); name = "bge"; break;
        case 0x6: taken = get_bit(c, PSW_Z);   name = "bz";  break;
        case 0x7: taken = !get_bit(c, PSW_Z);  name = "bnz"; break;
        case 0x8: taken = get_bit(c, PSW_CY);  name = "bc";  break;
        case 0x9: taken = !get_bit(c, PSW_CY); name = "bnc"; break;
        case 0xa: taken = !get_bit(c, PSW_S);  name = "bps"; break;
        case 0xb: taken = get_bit(c, PSW_S);   name = "bns"; break;
        case 0xc: taken = 1; name = "jr"; break;
        case 0xd: taken = 1; name = "ljr"; c->ra = c->pc; break;
        default:  taken = 0; name = "b?"; break;
        }
        trace(c, pc, "%s %04x %s", name, target, taken ? "(taken)" : "");
        if (taken) c->pc = target;
        return;
    }

    unsigned low4 = insn & 0xf;

    // Format S: Inst{3-0} == 0100.
    if (low4 == 0x4) {
        unsigned func = (insn >> 8) & 0xf;
        unsigned hi   = (insn >> 12) & 0xf;
        unsigned mid  = (insn >> 4) & 0xf;

        switch (func) {
        case 0x0: // MOV Rd, Rs
            c->r[mid] = c->r[hi];
            trace(c, pc, "mov r%u, r%u -> %02x", mid, hi, c->r[mid]);
            return;
        case 0x1: { // MOV Xd, SFR
            unsigned sfr = (insn >> 12) & 0x3;
            unsigned xd = (insn >> 4) & 0x7;
            uint16_t v = sfr == 0 ? pc : sfr == 1 ? c->ra : sfr == 2 ? c->psw : c->sp;
            x_set(c, xd, v);
            trace(c, pc, "mov %s, sfr%u -> %04x", x_name(xd), sfr, v);
            return;
        }
        case 0x2: { // MOV SFR, Xs
            unsigned xs = (insn >> 12) & 0x7;
            unsigned sfr = (insn >> 4) & 0x3;
            uint16_t v = x_get(c, xs);
            if (sfr == 1) c->ra = v;
            else if (sfr == 3) c->sp = v;
            trace(c, pc, "mov sfr%u, %s -> %04x", sfr, x_name(xs), v);
            return;
        }
        // The specification writes PUSH as "store then decrement" and POP as
        // "load then increment", which are not inverses of each other.  The
        // simulator uses the full-descending reading - decrement then store,
        // load then increment - so that a push/pop pair round-trips, and the
        // compiler's frame layout assumes the same.
        case 0x8: // PUSH Rs
            c->sp = (uint16_t)(c->sp - 1);
            c->mem[c->sp] = c->r[hi];
            trace(c, pc, "push r%u (%02x) sp=%04x", hi, c->r[hi], c->sp);
            return;
        case 0x9: { // PUSH SFR (2 bytes, little endian)
            unsigned sfr = (insn >> 12) & 0x3;
            uint16_t v = sfr == 0 ? pc : sfr == 1 ? c->ra : sfr == 2 ? c->psw : c->sp;
            c->sp = (uint16_t)(c->sp - 2);
            c->mem[c->sp] = (uint8_t)v;
            c->mem[(uint16_t)(c->sp + 1)] = (uint8_t)(v >> 8);
            trace(c, pc, "push sfr%u (%04x) sp=%04x", sfr, v, c->sp);
            return;
        }
        case 0xa: // POP Rd
            c->r[mid] = c->mem[c->sp];
            c->sp = (uint16_t)(c->sp + 1);
            trace(c, pc, "pop r%u -> %02x sp=%04x", mid, c->r[mid], c->sp);
            return;
        case 0xb: { // POP SFR (2 bytes, little endian)
            unsigned sfr = (insn >> 4) & 0x3;
            uint16_t v = (uint16_t)(c->mem[c->sp] |
                                    (c->mem[(uint16_t)(c->sp + 1)] << 8));
            c->sp = (uint16_t)(c->sp + 2);
            if (sfr == 0) c->pc = v;
            else if (sfr == 1) c->ra = v;
            else if (sfr == 2) c->psw = v;
            else c->sp = v;
            trace(c, pc, "pop sfr%u -> %04x sp=%04x", sfr, v, c->sp);
            return;
        }
        default:
            break;
        }
    }

    // Formats A and L: Inst{1-0} == 00 with Inst{3-2} selecting the class.
    if ((insn & 3) == 0 && (low4 == 0x8 || low4 == 0xc)) {
        unsigned func = (insn >> 8) & 0xf;
        unsigned rd   = (insn >> 4) & 0xf;
        unsigned hi   = (insn >> 12) & 0xf;
        unsigned res;

        if (low4 == 0x8) { // arithmetic
            switch (func) {
            case 0x0: // ADD
                res = (unsigned)c->r[rd] + c->r[hi];
                set_bit(c, PSW_CY, res > 0xff);
                c->r[rd] = (uint8_t)res; set_nz(c, c->r[rd]);
                trace(c, pc, "add r%u, r%u -> %02x", rd, hi, c->r[rd]);
                return;
            case 0x1: // ADC
                res = (unsigned)c->r[rd] + c->r[hi] + get_bit(c, PSW_CY);
                set_bit(c, PSW_CY, res > 0xff);
                c->r[rd] = (uint8_t)res; set_nz(c, c->r[rd]);
                trace(c, pc, "adc r%u, r%u -> %02x", rd, hi, c->r[rd]);
                return;
            case 0x4: // SUB
                set_bit(c, PSW_CY, c->r[rd] < c->r[hi]);
                c->r[rd] = (uint8_t)(c->r[rd] - c->r[hi]); set_nz(c, c->r[rd]);
                trace(c, pc, "sub r%u, r%u -> %02x", rd, hi, c->r[rd]);
                return;
            case 0x5: { // SBB
                unsigned borrow = (unsigned)get_bit(c, PSW_CY);
                unsigned lhs = c->r[rd], rhs = (unsigned)c->r[hi] + borrow;
                set_bit(c, PSW_CY, lhs < rhs);
                c->r[rd] = (uint8_t)(lhs - rhs); set_nz(c, c->r[rd]);
                trace(c, pc, "sbb r%u, r%u -> %02x", rd, hi, c->r[rd]);
                return;
            }
            case 0x8: // INC Rd, i4 - no flags
                c->r[rd] = (uint8_t)(c->r[rd] + hi + 1);
                trace(c, pc, "inc r%u, %u -> %02x", rd, hi + 1, c->r[rd]);
                return;
            case 0x9: // DEC Rd, i4 - no flags
                c->r[rd] = (uint8_t)(c->r[rd] - (hi + 1));
                trace(c, pc, "dec r%u, %u -> %02x", rd, hi + 1, c->r[rd]);
                return;
            default:
                break;
            }
        } else { // logical
            unsigned amount = ((insn >> 12) & 0x7) + 1;
            switch (func) {
            case 0x0: c->r[rd] &= c->r[hi]; set_nz(c, c->r[rd]);
                trace(c, pc, "and r%u, r%u -> %02x", rd, hi, c->r[rd]); return;
            case 0x2: c->r[rd] |= c->r[hi]; set_nz(c, c->r[rd]);
                trace(c, pc, "or r%u, r%u -> %02x", rd, hi, c->r[rd]); return;
            case 0x4: c->r[rd] ^= c->r[hi]; set_nz(c, c->r[rd]);
                trace(c, pc, "xor r%u, r%u -> %02x", rd, hi, c->r[rd]); return;
            case 0x6: c->r[rd] = (uint8_t)~c->r[rd]; set_nz(c, c->r[rd]);
                trace(c, pc, "not r%u -> %02x", rd, c->r[rd]); return;
            case 0x8: // LSL
                c->r[rd] = amount >= 8 ? 0 : (uint8_t)(c->r[rd] << amount);
                set_nz(c, c->r[rd]);
                trace(c, pc, "lsl r%u, %u -> %02x", rd, amount, c->r[rd]); return;
            case 0x9: // LSR
                c->r[rd] = amount >= 8 ? 0 : (uint8_t)(c->r[rd] >> amount);
                set_nz(c, c->r[rd]);
                trace(c, pc, "lsr r%u, %u -> %02x", rd, amount, c->r[rd]); return;
            case 0xa: { // RSL
                unsigned n = amount & 7;
                c->r[rd] = (uint8_t)((c->r[rd] << n) | (c->r[rd] >> (8 - n)));
                set_nz(c, c->r[rd]);
                trace(c, pc, "rsl r%u, %u -> %02x", rd, amount, c->r[rd]); return;
            }
            case 0xb: { // RSR
                unsigned n = amount & 7;
                c->r[rd] = (uint8_t)((c->r[rd] >> n) | (c->r[rd] << (8 - n)));
                set_nz(c, c->r[rd]);
                trace(c, pc, "rsr r%u, %u -> %02x", rd, amount, c->r[rd]); return;
            }
            case 0xc: { // ASR
                int8_t v = (int8_t)c->r[rd];
                c->r[rd] = (uint8_t)(amount >= 8 ? (v < 0 ? -1 : 0) : (v >> amount));
                set_nz(c, c->r[rd]);
                trace(c, pc, "asr r%u, %u -> %02x", rd, amount, c->r[rd]); return;
            }
            case 0xe: // CLRF i4
                set_bit(c, (int)hi, 0);
                trace(c, pc, "clrf %u", hi); return;
            case 0xf: // INVF i4
                set_bit(c, (int)hi, !get_bit(c, (int)hi));
                trace(c, pc, "invf %u", hi); return;
            default:
                break;
            }
        }
    }

    // Format P: Inst{3-0} == 0000.
    if (low4 == 0x0) {
        unsigned func = (insn >> 4) & 0xf;
        unsigned rs1 = (insn >> 12) & 0xf;
        unsigned rs2 = (insn >> 8) & 0xf;

        switch (func) {
        case 0x0: // machine control - the whole opcode is fixed
            switch (insn >> 8) {
            case 0x00: trace(c, pc, "nop"); return;
            case 0x01: trace(c, pc, "ret -> %04x", c->ra); c->pc = c->ra; return;
            case 0x02: trace(c, pc, "fncb"); return;
            case 0x03: trace(c, pc, "fnca"); return;
            case 0x80: // WFI
                trace(c, pc, "wfi");
                // Wait for an interrupt, rather than halt, only when one can
                // still arrive.  crt0's halt loop reaches this with
                // interrupts off and so stops the core, which is what every
                // program that never enables them relies on.
                if (get_bit(c, PSW_IE) && get_bit(c, PSW_ME) &&
                    c->irq_enable && (c->timer_load || c->irq_pending))
                    c->pc = pc;   // stay here until delivery moves PC
                else
                    c->halted = 1;
                return;
            default: break;
            }
            break;

        case 0x2: { // SWAP Rs1, Rs2
            uint8_t t = c->r[rs1]; c->r[rs1] = c->r[rs2]; c->r[rs2] = t;
            trace(c, pc, "swap r%u, r%u", rs1, rs2);
            return;
        }
        case 0x3: { // SWAP Xs, SFR
            unsigned xs = (insn >> 12) & 0x7;
            unsigned sfr = (insn >> 8) & 0x3;
            uint16_t xv = x_get(c, xs);
            if (sfr == 1) { x_set(c, xs, c->ra); c->ra = xv; }
            else if (sfr == 3) { x_set(c, xs, c->sp); c->sp = xv; }
            trace(c, pc, "swap %s, sfr%u", x_name(xs), sfr);
            return;
        }
        case 0x4: { // MUL Rs1, Rs2 -> W
            uint16_t product = (uint16_t)((unsigned)c->r[rs1] * c->r[rs2]);
            x_set(c, 4, product);
            trace(c, pc, "mul r%u, r%u -> w=%04x", rs1, rs2, product);
            return;
        }
        case 0x5: { // DIV Rs1, Rs2 -> W.lo = quotient, W.hi = remainder
            if (c->r[rs2] == 0) {
                set_bit(c, PSW_DZ, 1);
                x_set(c, 4, 0);
            } else {
                uint8_t q = (uint8_t)(c->r[rs1] / c->r[rs2]);
                uint8_t r = (uint8_t)(c->r[rs1] % c->r[rs2]);
                set_bit(c, PSW_DZ, 0);
                x_set(c, 4, (uint16_t)(q | (r << 8)));
            }
            trace(c, pc, "div r%u, r%u -> w=%04x", rs1, rs2, x_get(c, 4));
            return;
        }
        case 0x6: { // CMP Rs1, Rs2 - unsigned ordering plus the borrow flag
            uint8_t a = c->r[rs1], b = c->r[rs2];
            set_bit(c, PSW_EQ, a == b);
            set_bit(c, PSW_LT, a < b);
            set_bit(c, PSW_GT, a > b);
            set_bit(c, PSW_CY, a < b);
            set_bit(c, PSW_Z, (uint8_t)(a - b) == 0);
            set_bit(c, PSW_S, ((uint8_t)(a - b) & 0x80) != 0);
            trace(c, pc, "cmp r%u(%02x), r%u(%02x)", rs1, a, rs2, b);
            return;
        }
        case 0x8: { // JA a16
            uint16_t target = rd16(c, (uint16_t)(pc + 2));
            c->pc = target;
            trace(c, pc, "ja %04x", target);
            return;
        }
        case 0x9: { // LJA a16
            uint16_t target = rd16(c, (uint16_t)(pc + 2));
            c->ra = (uint16_t)(pc + 4);
            c->pc = target;
            trace(c, pc, "lja %04x (ra=%04x)", target, c->ra);
            return;
        }
        default:
            break;
        }
    }

    fprintf(stderr, "%04x: unimplemented instruction %04x\n", pc, insn);
    c->halted = 2;
}

//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
    const char *path = NULL;
    uint64_t max_steps = 20000000;
    int trace_on = 0, dump = 0, quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--trace")) trace_on = 1;
        else if (!strcmp(argv[i], "--dump")) dump = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--max") && i + 1 < argc)
            max_steps = strtoull(argv[++i], NULL, 0);
        else if (argv[i][0] == '-') {
            fprintf(stderr,
                    "usage: %s program.elf [--trace] [--dump] [--quiet] "
                    "[--max N]\n", argv[0]);
            return 2;
        } else path = argv[i];
    }
    if (!path) {
        fprintf(stderr,
                "usage: %s program.elf [--trace] [--dump] [--quiet] "
                "[--max N]\n", argv[0]);
        return 2;
    }

    Core *c = calloc(1, sizeof *c);
    if (!c) { perror("calloc"); return 1; }
    c->trace = trace_on;

    uint16_t entry = 0;
    if (load_elf(c, path, &entry) != 0) { free(c); return 1; }
    c->pc = entry;
    // A real uG24 takes its reset SP from the strapped i_reset_sp pin.  The
    // closest thing available here is __stack_top from the image, which is
    // what crt0 loads anyway; 0xfffe is the fallback when the symbol is
    // missing.  Seeding it this way also keeps the bounds check below from
    // firing on the instructions before crt0 has set SP.
    c->sp = c->stack_high ? c->stack_high : 0xfffe;

    while (!c->halted && c->cycles < max_steps) {
        step(c);
        check_stack(c);
        // The timer counts retired instructions, which is the only clock this
        // simulator has.  Delivery happens between instructions, so a handler
        // never starts in the middle of one.
        tick_timer(c);
        if (!c->halted)
            deliver_interrupt(c);
    }

    if (!c->halted)
        fprintf(stderr, "stopped after %llu instructions without halting\n",
                (unsigned long long)c->cycles);

    // --quiet leaves only what the program itself printed.
    if (!quiet) {
        printf("halted after %llu instructions\n", (unsigned long long)c->cycles);
        printf("pc=%04x sp=%04x ra=%04x psw=%04x\n", c->pc, c->sp, c->ra, c->psw);
        if (c->irq_taken)
            printf("interrupts taken: %llu\n", (unsigned long long)c->irq_taken);
        // A byte-sized return lands in R0; anything 16-bit comes back in W.
        printf("return value: r0=%u  w=%u\n", c->r[0],
               (unsigned)(c->r[8] | (c->r[9] << 8)));
        for (int i = 0; i < 16; i++)
            printf("r%-2d=%02x%s", i, c->r[i], (i % 8 == 7) ? "\n" : " ");
    }

    if (dump) {
        FILE *o = fopen("ug24-memory.bin", "wb");
        if (o) { fwrite(c->mem, 1, MEM_SIZE, o); fclose(o); }
    }

    // 3: hit an instruction the ISA does not define.  4: the stack left its
    // window.  Otherwise the value the program returned.
    int status = c->halted == 2 ? 3 : c->halted == 3 ? 4 : c->exit_code;
    free(c);
    return status;
}
