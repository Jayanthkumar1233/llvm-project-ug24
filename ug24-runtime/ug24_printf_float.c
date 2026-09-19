//===-- ug24_printf_float.c - printf's decimal conversion -----------------===//
//
// %f, and the %f half of %g, converted from the bit pattern by integer
// arithmetic alone.
//
// This is a separate translation unit from ug24_float.c on purpose, and the
// reason is a linking one.  A float is exactly mant x 2^(exp-23), so the
// digits of %f can be recovered with shifts and a multiply by ten and no
// floating-point arithmetic at all -- which means this file can be linked
// with printf unconditionally, at its own small cost, without dragging in the
// soft-float library behind it.
//
// %e and %E genuinely need that library: finding the decimal exponent of an
// arbitrary float means dividing by ten until it is in range.  They are
// delegated to __ug24_format_float_exp in ug24_float.c, declared weak here,
// so a program that never does floating-point arithmetic does not link it.
// Such a program printing %e gets "<fp?>"; printing %f gets its digits.
//
// The alternative -- having the compiler notice a float reaching a variadic
// call and ask the linker for the formatter -- was implemented and removed.
// The code generator has no business knowing how printf is built.
//
//===----------------------------------------------------------------------===//

#include "ug24_float_bits.h"

#define FORMAT_MAX_PRECISION 17

// Provided by ug24_float.c, which a program only links if it does
// floating-point arithmetic.  Null otherwise, and the caller says "<fp?>".
__attribute__((weak)) int __ug24_format_float_exp(char *out, u32 bits,
                                                  int precision, int upper,
                                                  int strip_zeros);

static int copy_literal(char *out, const char *text) {
  int n = 0;
  while (text[n]) {
    out[n] = text[n];
    n++;
  }
  return n;
}

// Digits of \p value, most significant first, by repeated subtraction rather
// than by division.
//
// "value % 10" and "value /= 10" on a 64-bit value are two calls into
// __udivdi3, which is restoring division -- 64 iterations over four limbs,
// about 32,000 instructions each.  Eight digits of an integer part cost half
// a million instructions that way.  Subtraction is at most nine steps per
// digit, and 64-bit compare and subtract are expanded inline by the backend,
// so this needs no helper at all.
static const u64 kPowersOfTen[] = {
  10000000000000000000ULL, 1000000000000000000ULL, 100000000000000000ULL,
  10000000000000000ULL,    1000000000000000ULL,    100000000000000ULL,
  10000000000000ULL,       1000000000000ULL,       100000000000ULL,
  10000000000ULL,          1000000000ULL,          100000000ULL,
  10000000ULL,             1000000ULL,             100000ULL,
  10000ULL,                1000ULL,                100ULL,
  10ULL,                   1ULL,
};

#define POWERS_OF_TEN (sizeof kPowersOfTen / sizeof kPowersOfTen[0])

static int format_u64(char *out, u64 value) {
  int len = 0, leading = 1;
  unsigned i;

  for (i = 0; i < POWERS_OF_TEN; i++) {
    u64 power = kPowersOfTen[i];
    u8 digit = 0;

    while (value >= power) {
      value -= power;
      digit++;
    }
    if (digit)
      leading = 0;
    if (!leading)
      out[len++] = (char)('0' + digit);
  }

  if (len == 0)
    out[len++] = '0';            // the value was zero
  return len;
}

/// Number of decimal digits in \p value, which is also where its decimal
/// exponent sits: 1144 has four digits, so its exponent is 3.
static int decimal_digits(u64 value) {
  int n = 1;
  unsigned i;

  for (i = 0; i < POWERS_OF_TEN; i++)
    if (value >= kPowersOfTen[i]) {
      n = (int)(POWERS_OF_TEN - i);
      break;
    }
  return n;
}

// Multiply a binary fraction -- a value scaled by 2^32, so 0.5 is
// 0x80000000 -- by ten, returning the decimal digit that carried out of the
// top.  Done on 16-bit halves so that nothing here needs a 64-bit
// intermediate, and exactly, so the digits it produces are the digits of the
// float rather than of an approximation to it.
static u8 mul10(u32 *fraction) {
  u32 value = *fraction;
  u32 low = (value & 0xffffUL) * 10;
  u32 high = (value >> 16) * 10 + (low >> 16);

  *fraction = ((high & 0xffffUL) << 16) | (low & 0xffffUL);
  return (u8)(high >> 16);
}

/// Split \p p at the binary point.  Returns 0 when the value is outside the
/// range this can represent, in which case the caller falls back to %e.
static int split(Parts p, u64 *integer_part, u32 *fraction) {
  int shift = p.exp - MANT_BITS;   // value = p.mant * 2^shift

  *integer_part = 0;
  *fraction = 0;

  if (p.is_zero)
    return 1;

  // A 24-bit mantissa shifted left by more than 40 no longer fits a 64-bit
  // integer part.
  if (shift > 40)
    return 0;

  if (shift >= 0) {
    *integer_part = (u64)p.mant << shift;
    return 1;
  }

  {
    unsigned r = (unsigned)(-shift);

    *integer_part = r < 24 ? (u64)(p.mant >> r) : 0;

    // The fraction is the low r bits of the mantissa, scaled so that its most
    // significant bit lands in bit 31.  Each branch avoids a shift by 32 or
    // more, which is undefined.
    if (r < 32)
      *fraction = (p.mant & (((u32)1 << r) - 1)) << (32 - r);
    else if (r == 32)
      *fraction = p.mant;
    else if (r < 56)
      *fraction = p.mant >> (r - 32);
    // Beyond that the whole value is under 2^-33.  It is not zero, but this
    // representation cannot hold it, so the caller uses %e.
  }

  return *integer_part != 0 || *fraction != 0;
}

/// The decimal exponent of a value already split, which %g needs to choose a
/// style: 1144.2 is 3, 0.00123 is -3.
static int decimal_exponent(u64 integer_part, u32 fraction) {
  int k;

  if (integer_part)
    return decimal_digits(integer_part) - 1;

  for (k = 1; k <= 45; k++)
    if (mul10(&fraction))
      return -k;
  return 0;                        // the value was zero
}

/// Write the magnitude in %f style.  \p out must have room for 48 characters.
static int format_fixed(char *out, u64 integer_part, u32 fraction,
                        int precision) {
  char digits[FORMAT_MAX_PRECISION];
  int len = 0, i, last_parity;

  for (i = 0; i < precision; i++)
    digits[i] = (char)('0' + mul10(&fraction));

  // Round to nearest, ties to even -- the rule the arithmetic uses, and the
  // rule a hosted printf uses, so the two agree digit for digit.
  last_parity = precision > 0 ? (digits[precision - 1] - '0')
                              : (int)(integer_part & 1);
  if (fraction > 0x80000000UL ||
      (fraction == 0x80000000UL && (last_parity & 1))) {
    for (i = precision - 1; i >= 0; i--) {
      if (digits[i] != '9') { digits[i]++; break; }
      digits[i] = '0';
    }
    if (i < 0)
      integer_part++;              // the carry ran off the top of the fraction
  }

  len += format_u64(out + len, integer_part);

  if (precision > 0) {
    out[len++] = '.';
    for (i = 0; i < precision; i++)
      out[len++] = digits[i];
  }
  return len;
}

// %g prints no trailing zeros in its fraction, and no trailing point once
// they are gone.
static int trim_zeros(char *out, int len) {
  int end = len, i;
  int has_point = 0;

  for (i = 0; i < end; i++)
    if (out[i] == '.')
      has_point = 1;
  if (!has_point)
    return len;

  while (end > 0 && out[end - 1] == '0')
    end--;
  if (end > 0 && out[end - 1] == '.')
    end--;
  return end;
}

/// Format the IEEE single in \p bits into \p out, which must have room for 48
/// characters.  \p conv is one of f F e E g G; \p precision is -1 for the
/// default.  Returns the number of characters written.
int __ug24_format_float(char *out, u32 bits, int precision, char conv) {
  Parts p = unpack(bits);
  int negative = (int)((bits >> 31) & 1);
  int lower = conv | 0x20;
  int upper = !(conv & 0x20);
  int len = 0, strip_zeros = 0;
  u64 integer_part;
  u32 fraction;

  if (p.is_nan)
    return copy_literal(out, upper ? "NAN" : "nan");
  if (p.is_inf) {
    if (negative)
      out[len++] = '-';
    return len + copy_literal(out + len, upper ? "INF" : "inf");
  }

  if (precision < 0)
    precision = 6;
  if (precision > FORMAT_MAX_PRECISION)
    precision = FORMAT_MAX_PRECISION;

  // %e always needs the exponent machinery; %f and %g may not.
  if (lower != 'e' && split(p, &integer_part, &fraction)) {
    if (lower == 'g') {
      // C's rule: %e when the exponent is below -4 or not less than the
      // precision, %f otherwise.  A precision of 0 is treated as 1.
      int exponent = decimal_exponent(integer_part, fraction);

      if (precision == 0)
        precision = 1;
      if (exponent < -4 || exponent >= precision)
        goto exponential;          // %g that wants %e style
      precision -= exponent + 1;
      if (precision < 0)
        precision = 0;
      strip_zeros = 1;
    }

    if (negative)
      out[len++] = '-';
    len += format_fixed(out + len, integer_part, fraction, precision);
    return strip_zeros ? trim_zeros(out, len) : len;
  }

exponential:
  // %e, and anything whose magnitude the fixed-point path cannot hold.
  if (!__ug24_format_float_exp) {
    // The program does no floating-point arithmetic, so the soft-float
    // library -- and with it the exponent conversion -- was never linked.
    return copy_literal(out, "<fp?>");
  }
  if (negative)
    out[len++] = '-';
  return len + __ug24_format_float_exp(out + len, bits & 0x7fffffffUL,
                                       precision, upper, lower == 'g');
}
