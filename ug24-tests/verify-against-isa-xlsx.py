#!/usr/bin/env python3
"""Check the assembler's encodings against the vendor ISA spreadsheet.

This is the one check in the tree that does not trust anything the toolchain
believes about itself.  It reads "Copy of uG24xx1616uP_ISA.xlsx" -- the
document the backend was written from -- rebuilds each instruction's 16-bit
word from the bit columns, and compares that with what llvm-mc emits.  The
simulator, the runtime and the test suite are all out of the loop: if the
compiler has drifted from the reference document, this says so.

    ug24-tests/verify-against-isa-xlsx.py [--mc PATH] [--xlsx PATH]

The spreadsheet lays each instruction out as a row of bit columns: column I is
bit 15 and column X is bit 0, and a cell occupies bits from its own column
down to just above the next occupied one.  Rows whose displacement the sheet
cannot pin to a fixed operand -- the PC-relative branches and the two-word
jumps -- are checked separately in the second half, against the reading of the
displacement that specification query D2 settled.
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ap = argparse.ArgumentParser()
ap.add_argument('--mc', help='path to llvm-mc')
ap.add_argument('--xlsx', default=os.path.join(ROOT, 'Copy of uG24xx1616uP_ISA.xlsx'))
args = ap.parse_args()

MC = args.mc
if not MC:
    for c in (os.environ.get('UG24_BUILD', ''), os.path.join(ROOT, 'build-ug24'),
              os.path.join(ROOT, '..', 'build-ug24')):
        if c and os.path.isfile(os.path.join(c, 'bin', 'llvm-mc')):
            MC = os.path.join(c, 'bin', 'llvm-mc')
            break
if not MC or not os.path.isfile(MC):
    sys.exit('no llvm-mc found; pass --mc or set UG24_BUILD')
OBJCOPY = os.path.join(os.path.dirname(MC), 'llvm-objcopy')

if not os.path.isfile(args.xlsx):
    sys.exit('no ISA spreadsheet at %s' % args.xlsx)

WORK = tempfile.mkdtemp(prefix='ug24-isa-')
subprocess.run(['unzip', '-q', '-o', args.xlsx, '-d', WORK], check=True)
XLSX = WORK

NS = '{http://schemas.openxmlformats.org/spreadsheetml/2006/main}'

ss = [''.join(x.text or '' for x in si.iter(NS + 't'))
      for si in ET.parse(XLSX + '/xl/sharedStrings.xml').getroot()]

def col_to_bit(col):
    return 15 - (ord(col) - ord('I'))

rows = []
for row in ET.parse(XLSX + '/xl/worksheets/sheet2.xml').getroot().iter(NS + 'row'):
    n = int(row.get('r'))
    if n < 2:
        continue
    cells = {}
    for c in row.iter(NS + 'c'):
        col = ''.join(ch for ch in c.get('r') if ch.isalpha())
        v = c.find(NS + 'v')
        if v is None:
            continue
        cells[col] = ss[int(v.text)] if c.get('t') == 's' else v.text
    if 'A' in cells:
        rows.append((n, cells))

# Operand values to test with, and how each is spelled in our assembly.
VALUES = {'Rd': 1, 'Rs': 2, 'Rs1': 1, 'Rs2': 2, 'Xd': 4, 'Xs': 4,
          'i8': 0x5A, 'i4': 5, 'i3': 3, 'i7': 0x25}
ASM_OPERAND = {'Rd': 'r1', 'Rs': 'r2', 'Rs1': 'r1', 'Rs2': 'r2',
               'Xd': 'w', 'Xs': 'w', 'i8': '90', 'i4': '5',
               'i3': '4',        # LSL/LSR/... encode amount - 1
               'i7': '37'}
# INC/DEC encode amount - 1 in their i4 field, so the assembly says 6 for 5.
INC_DEC_ASM = '6'

def expected_word(cells, overrides):
    """Rebuild the 16-bit word from the spreadsheet row."""
    occupied = sorted((col_to_bit(c), cells[c]) for c in cells
                      if len(c) == 1 and 'I' <= c <= 'X')
    occupied.reverse()                      # high bit first
    word, covered, fields = 0, 0, {}
    for i, (bit, text) in enumerate(occupied):
        low = occupied[i + 1][0] + 1 if i + 1 < len(occupied) else 0
        width = bit - low + 1
        clean = text.replace('_', '')
        if set(clean) <= {'0', '1'}:
            if len(clean) != width:
                return None, 'literal %r does not fill bits %d..%d' % (text, bit, low)
            value = int(clean, 2)
        else:
            if text not in overrides:
                return None, 'unknown field %r' % text
            value = overrides[text]
            if value >= (1 << width):
                return None, 'field %s=%d does not fit %d bits' % (text, value, width)
            fields[text] = (bit, low, width)
        word |= value << low
        covered |= ((1 << width) - 1) << low
    if covered != 0xFFFF:
        return None, 'bits unaccounted for: %04x' % (0xFFFF ^ covered)
    return word, fields

def our_asm(mnemonic):
    """The spreadsheet's syntax, translated to ours where they differ."""
    op, _, args = mnemonic.partition(' ')
    op = op.lower()
    parts = [a.strip() for a in args.split(',')] if args else []
    out = []
    for a in parts:
        if a in ASM_OPERAND:
            out.append(INC_DEC_ASM if (a == 'i4' and op in ('inc', 'dec'))
                       else ASM_OPERAND[a])
        else:
            out.append(a.lower())           # PC, RA, PSW, SP
    if op in ('ld', 'st'):                  # we bracket the displacement
        out = [x if not x.isdigit() else '[%s]' % x for x in out]
    return op + ('\t' + ', '.join(out) if out else '')

def assemble(text):
    p = subprocess.run([MC, '-triple=ug24-unknown-none-eabi', '-show-encoding'],
                       input=text + '\n', capture_output=True, text=True)
    if p.returncode != 0:
        return None, p.stderr.strip().splitlines()[0] if p.stderr else 'failed'
    m = re.search(r'encoding: \[([^\]]+)\]', p.stdout)
    if not m:
        return None, 'no encoding in output'
    b = [int(x, 16) for x in m.group(1).replace('0x', '').split(',')]
    return b[0] | (b[1] << 8), None        # little endian

ok = bad = skipped = 0
problems = []
for n, cells in rows:
    mnemonic = cells['A']
    if 'i10' in str(cells.get('I', '')) or 'a16' in mnemonic:
        skipped += 1                        # PC-relative / 2-word: checked separately
        continue
    overrides = dict(VALUES)
    if mnemonic.startswith(('INC', 'DEC')):
        overrides['i4'] = 5                 # amount 6 encodes as 5
    word, info = expected_word(cells, overrides)
    if word is None:
        skipped += 1
        problems.append('%-18s SKIP  %s' % (mnemonic, info))
        continue
    text = our_asm(mnemonic)
    got, err = assemble(text)
    if got is None:
        bad += 1
        problems.append('%-18s FAIL  %-22s %s' % (mnemonic, text.replace('\t', ' '), err))
    elif got != word:
        bad += 1
        problems.append('%-18s DIFF  %-22s sheet=%04x mc=%04x'
                        % (mnemonic, text.replace('\t', ' '), word, got))
    else:
        ok += 1

for p in problems:
    print(p)
print('encodings:  matched %d    mismatched %d    deferred %d' % (ok, bad, skipped))
total_bad = bad

FUNC = {'beq': 0b0000, 'bne': 0b0001, 'blt': 0b0010, 'ble': 0b0011,
        'bgt': 0b0100, 'bge': 0b0101, 'bz':  0b0110, 'bnz': 0b0111,
        'bc':  0b1000, 'bnc': 0b1001, 'bps': 0b1010, 'bns': 0b1011,
        'jr':  0b1100, 'ljr': 0b1101}

def assemble(text):
    p = subprocess.run([MC, '-triple=ug24-unknown-none-eabi', '-filetype=obj',
                        '-o', os.path.join(WORK, 'br.o')], input=text + '\n',
                       capture_output=True, text=True)
    if p.returncode:
        return None, p.stderr.strip().splitlines()[0]
    d = subprocess.run([OBJCOPY,
                        '-O', 'binary', '--only-section=.text', os.path.join(WORK, 'br.o'),
                        os.path.join(WORK, 'br.bin')], capture_output=True, text=True)
    if d.returncode:
        return None, d.stderr.strip()
    return open(os.path.join(WORK, 'br.bin'), 'rb').read(), None

bad = ok = 0   # branch tallies

# --- branches: one instruction word forward, and one backward -------------
for mn, func in FUNC.items():
    for label, words, disp in (('forward', 1, 1), ('backward', -1, -1)):
        if disp > 0:
            src = '  %s .Lt\n  nop\n.Lt:\n  nop\n' % mn
            at = 0
        else:
            src = '.Lt:\n  nop\n  %s .Lt\n  nop\n' % mn
            at = 2
        data, err = assemble('  .text\n' + src)
        if data is None:
            print('%-5s %-8s FAIL  %s' % (mn, label, err)); bad += 1; continue
        word = data[at] | (data[at + 1] << 8)
        # target - (address of the next instruction), in instruction words
        want_disp = disp if disp > 0 else -2
        expect = ((want_disp & 0x3FF) << 6) | (func << 2) | 0b10
        if word != expect:
            print('%-5s %-8s DIFF  sheet+D2=%04x  mc=%04x' % (mn, label, expect, word)); bad += 1
        else:
            ok += 1

# --- JA / LJA: 2 words, opcode then the absolute address ------------------
for mn, q in (('ja', 0b1000), ('lja', 0b1001)):
    data, err = assemble('  .text\n  %s 0x1234\n' % mn)
    if data is None:
        print('%-5s %-8s FAIL  %s' % (mn, 'abs', err)); bad += 1; continue
    w0 = data[0] | (data[1] << 8)
    w1 = data[2] | (data[3] << 8)
    expect0 = (q << 4) | 0b0000
    if w0 != expect0 or w1 != 0x1234:
        print('%-5s %-8s DIFF  sheet=%04x,1234  mc=%04x,%04x'
              % (mn, 'abs', expect0, w0, w1)); bad += 1
    else:
        ok += 1

print('branches:   matched %d    mismatched %d' % (ok, bad))
shutil.rmtree(WORK, ignore_errors=True)
total_bad += bad
print()
print('THE ASSEMBLER MATCHES THE SPREADSHEET' if total_bad == 0
      else '%d ENCODINGS DISAGREE WITH THE SPREADSHEET' % total_bad)
sys.exit(1 if total_bad else 0)
