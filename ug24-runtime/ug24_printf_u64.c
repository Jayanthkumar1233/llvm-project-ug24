//===-- ug24_printf_u64.c - 64-bit digits for printf ----------------------===//
//
// The digits of a 64-bit value, for printf's %lld, %llu, %llx and %llo.  Its
// own archive member so that the cost lands on programs that ask for it: a
// program can replace it in one line (see the bottom of this file) and get the
// space back, in the same way as the float formatter in ug24_printf_float.c.
//
// Nothing here divides and nothing here shifts a 64-bit value by a variable
// amount.  Both are library calls on a target whose widest register pair is 16
// bits, and both cost more than every other line of this file together.
//
//===----------------------------------------------------------------------===//

typedef unsigned long      u32;
typedef unsigned long long u64;

int __ug24_u64_to_decimal(char *out, u64 value);

/// Digits of \p value in \p base, **least** significant first, which is the
/// order printf wants for right-aligned output.  Returns how many.
int __ug24_format_u64(char *out, u64 value, unsigned base,
                      const char *alphabet) {
  if (base == 10) {
    // The shared helper writes most significant first, so reverse in place.
    char msb_first[20];
    int count = __ug24_u64_to_decimal(msb_first, value);
    for (int i = 0; i < count; i++)
      out[i] = msb_first[count - 1 - i];
    return count;
  }

  // Octal and hex on 32-bit halves, with constant shift amounts, so that the
  // whole loop stays inline: a 64-bit shift by a variable is __lshrdi3.
  u32 lo = (u32)value, hi = (u32)(value >> 32);
  int length = 0;

  if (base == 16) {
    do {
      out[length++] = alphabet[(unsigned)lo & 15];
      lo = (lo >> 4) | (hi << 28);
      hi >>= 4;
    } while (lo | hi);
    return length;
  }

  do {
    out[length++] = alphabet[(unsigned)lo & 7];
    lo = (lo >> 3) | (hi << 29);
    hi >>= 3;
  } while (lo | hi);
  return length;
}

// To drop this from a program that never prints a 64-bit value, define the
// symbol yourself and the archive member is never extracted:
//
//     int __ug24_format_u64(char *out, unsigned long long value,
//                           unsigned base, const char *alphabet)
//     { (void)value; (void)base; (void)alphabet; out[0] = '?'; return 1; }
