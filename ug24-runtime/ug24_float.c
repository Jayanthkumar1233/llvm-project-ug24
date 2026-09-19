//===-- ug24_float.c - Soft floating point for the uG24 -------------------===//
//
// The uG24 has no floating-point unit, so Clang softens every floating-point
// operation into a call to one of the helpers below.  `double` and `long
// double` are both IEEE single on this target (see UG24TargetInfo), so only
// the single-precision half of the usual soft-float library exists: nothing
// ever asks for __adddf3 or __extendsfdf2.
//
// Values arrive and leave as 32-bit patterns because that is what a softened
// f32 is by the time it reaches a libcall.  Taking them apart by hand keeps
// the whole file inside the integer operations the backend can already do.
//
// Rounding is IEEE round-to-nearest, ties to even.
//
//===----------------------------------------------------------------------===//

#include "ug24_float_bits.h"

// Assemble a value from a sign, an unbiased exponent and a mantissa that is
// not yet normalised.  Handles overflow to infinity, gradual underflow and
// rounding in one place so that the arithmetic above it does not have to.
static u32 pack(u8 sign, short exp, u32 mant, u32 sticky) {
  u32 sign_bit = (u32)sign << 31;

  if (mant == 0 && sticky == 0)
    return sign_bit;

  // Normalise up.
  while (mant < HIDDEN_BIT && exp > (short)(1 - EXP_BIAS)) {
    mant <<= 1;
    exp--;
    if (sticky & 0x80000000UL)
      mant |= 1;
    sticky <<= 1;
  }
  // Normalise down.
  while (mant > (HIDDEN_BIT * 2 - 1)) {
    sticky = (sticky >> 1) | (u32)((mant & 1) << 31);
    mant >>= 1;
    exp++;
  }

  // Subnormal: shift the mantissa down until the exponent is representable.
  while (exp < (short)(1 - EXP_BIAS) && mant) {
    sticky = (sticky >> 1) | (u32)((mant & 1) << 31);
    mant >>= 1;
    exp++;
  }

  // Round to nearest, ties to even: round up when the discarded part is
  // more than half, or exactly half with an odd mantissa below it.
  if (sticky > 0x80000000UL ||
      (sticky == 0x80000000UL && (mant & 1))) {
    mant++;
    if (mant > (HIDDEN_BIT * 2 - 1)) {
      mant >>= 1;
      exp++;
    }
  }

  if (exp > EXP_MAX - 1 - EXP_BIAS)
    return sign_bit | ((u32)EXP_MAX << MANT_BITS);   // infinity

  if (mant < HIDDEN_BIT)
    return sign_bit | (mant & MANT_MASK);            // subnormal or zero

  return sign_bit | ((u32)(exp + EXP_BIAS) << MANT_BITS) | (mant & MANT_MASK);
}

#define QUIET_NAN 0x7fc00000UL
#define INFINITY_BITS(sign) (((u32)(sign) << 31) | ((u32)EXP_MAX << MANT_BITS))

//===----------------------------------------------------------------------===//
// Addition and subtraction
//===----------------------------------------------------------------------===//

u32 __addsf3(u32 abits, u32 bbits) {
  Parts a = unpack(abits), b = unpack(bbits);
  short exp;
  u32 sticky = 0;
  u8 sign;

  if (a.is_nan || b.is_nan)
    return QUIET_NAN;
  if (a.is_inf || b.is_inf) {
    if (a.is_inf && b.is_inf && a.sign != b.sign)
      return QUIET_NAN;            // inf - inf
    return a.is_inf ? abits : bbits;
  }
  if (a.is_zero && b.is_zero)
    return (a.sign && b.sign) ? 0x80000000UL : 0;
  if (a.is_zero)
    return bbits;
  if (b.is_zero)
    return abits;

  // Line the exponents up, keeping the bits shifted out as a sticky word so
  // that the rounding in pack() can see them.
  if (a.exp < b.exp) {
    Parts t = a; a = b; b = t;
    u32 tb = abits; abits = bbits; bbits = tb;
  }
  exp = a.exp;
  {
    unsigned shift = (unsigned)(a.exp - b.exp);
    if (shift > 31) {
      sticky = b.mant ? 1 : 0;
      b.mant = 0;
    } else if (shift) {
      sticky = (u32)(b.mant << (32 - shift));
      b.mant >>= shift;
    }
  }

  if (a.sign == b.sign) {
    sign = a.sign;
    a.mant += b.mant;
  } else {
    // Subtract the smaller magnitude from the larger.  The significand is
    // (mant, sticky), a 32-bit integer part with a 32-bit fraction below it,
    // and only b has a fraction -- a was the larger exponent and so was not
    // shifted.  Borrowing that fraction is what the -sticky does.
    if (a.mant > b.mant || (a.mant == b.mant && sticky == 0)) {
      sign = a.sign;
      if (sticky) {
        a.mant--;
        sticky = (u32)0 - sticky;
      }
      a.mant -= b.mant;
    } else {
      // b is the larger, so its fraction survives unchanged.
      sign = b.sign;
      a.mant = b.mant - a.mant;
    }
    if (a.mant == 0 && sticky == 0)
      return 0;                    // exact cancellation is +0
  }

  return pack(sign, exp, a.mant, sticky);
}

u32 __subsf3(u32 abits, u32 bbits) {
  return __addsf3(abits, bbits ^ 0x80000000UL);
}

u32 __negsf2(u32 abits) { return abits ^ 0x80000000UL; }

//===----------------------------------------------------------------------===//
// Multiplication and division
//===----------------------------------------------------------------------===//

u32 __mulsf3(u32 abits, u32 bbits) {
  Parts a = unpack(abits), b = unpack(bbits);
  u8 sign = (u8)(a.sign ^ b.sign);
  u64 product;
  u32 mant, sticky;
  short exp;

  if (a.is_nan || b.is_nan)
    return QUIET_NAN;
  if (a.is_inf || b.is_inf) {
    if (a.is_zero || b.is_zero)
      return QUIET_NAN;            // 0 * inf
    return INFINITY_BITS(sign);
  }
  if (a.is_zero || b.is_zero)
    return (u32)sign << 31;

  // 24 x 24 -> 48 bits.  __muldi3 does the work; keeping it in 64 bits is
  // what lets the low half be tested for stickiness in one piece.
  product = (u64)a.mant * (u64)b.mant;
  exp = (short)(a.exp + b.exp);

  // The product is 1.xx or 1x.xx scaled by 2^46; bring it back to 2^23.
  mant = (u32)(product >> 23);
  sticky = (u32)(product << 9);

  return pack(sign, exp, mant, sticky);
}

u32 __divsf3(u32 abits, u32 bbits) {
  Parts a = unpack(abits), b = unpack(bbits);
  u8 sign = (u8)(a.sign ^ b.sign);
  u32 quotient = 0, remainder;
  short exp;
  int bit;

  if (a.is_nan || b.is_nan)
    return QUIET_NAN;
  if (a.is_inf) {
    if (b.is_inf)
      return QUIET_NAN;            // inf / inf
    return INFINITY_BITS(sign);
  }
  if (b.is_inf)
    return (u32)sign << 31;
  if (b.is_zero) {
    if (a.is_zero)
      return QUIET_NAN;            // 0 / 0
    return INFINITY_BITS(sign);    // x / 0
  }
  if (a.is_zero)
    return (u32)sign << 31;

  exp = (short)(a.exp - b.exp);
  remainder = a.mant;

  // Restoring division, producing one quotient bit per pass.  25 passes: 24
  // for the result and one more to round on.
  for (bit = 24; bit >= 0; bit--) {
    quotient <<= 1;
    if (remainder >= b.mant) {
      remainder -= b.mant;
      quotient |= 1;
    }
    remainder <<= 1;
  }

  // Twenty-five passes leave quotient = a.mant * 2^24 / b.mant, so dropping
  // the low bit gives a mantissa scaled by 2^23 -- which is what pack() reads
  // -- and that low bit is exactly the one to round on.  Anything still in
  // the remainder goes in behind it, so that a tie is recognised as a tie.
  return pack(sign, exp, quotient >> 1,
              (quotient & 1) ? 0x80000000UL | (remainder ? 1 : 0)
                             : (remainder ? 1 : 0));
}

//===----------------------------------------------------------------------===//
// Comparison
//===----------------------------------------------------------------------===//
//
// The libgcc convention: these return a value whose sign answers the
// comparison, and the __?esf2 forms return 1 for unordered so that the
// caller's test fails either way.

static int compare(u32 abits, u32 bbits, int unordered_result) {
  Parts a = unpack(abits), b = unpack(bbits);

  if (a.is_nan || b.is_nan)
    return unordered_result;

  if (a.is_zero && b.is_zero)
    return 0;                      // -0 == +0

  if (a.sign != b.sign)
    return a.sign ? -1 : 1;

  // Same sign: the encoding orders by magnitude, so the raw patterns can be
  // compared directly once the sign bit is masked off.
  {
    u32 am = abits & 0x7fffffffUL, bm = bbits & 0x7fffffffUL;
    int result = am < bm ? -1 : am > bm ? 1 : 0;
    return a.sign ? -result : result;
  }
}

int __eqsf2(u32 a, u32 b) { return compare(a, b, 1); }
int __nesf2(u32 a, u32 b) { return compare(a, b, 1); }
int __ltsf2(u32 a, u32 b) { return compare(a, b, 1); }
int __lesf2(u32 a, u32 b) { return compare(a, b, 1); }
int __gtsf2(u32 a, u32 b) { return compare(a, b, -1); }
int __gesf2(u32 a, u32 b) { return compare(a, b, -1); }

int __unordsf2(u32 abits, u32 bbits) {
  Parts a = unpack(abits), b = unpack(bbits);
  return a.is_nan || b.is_nan;
}

//===----------------------------------------------------------------------===//
// Conversion
//===----------------------------------------------------------------------===//

u32 __floatunsisf(u32 value) {
  if (value == 0)
    return 0;
  // pack() normalises, so the value can be handed over as an integer scaled
  // by 2^0 and left to find its own exponent.
  return pack(0, MANT_BITS, value, 0);
}

u32 __floatsisf(s32 value) {
  if (value < 0)
    return __floatunsisf((u32)(-value)) | 0x80000000UL;
  return __floatunsisf((u32)value);
}

u32 __floatundisf(u64 value) {
  u32 high = (u32)(value >> 32);
  if (high == 0)
    return __floatunsisf((u32)value);
  // Fold the low half into a sticky bit: at this magnitude none of it is
  // representable anyway.
  return pack(0, MANT_BITS + 32, high, (u32)value ? 1 : 0);
}

u32 __floatdisf(s64 value) {
  if (value < 0)
    return __floatundisf((u64)0 - (u64)value) | 0x80000000UL;
  return __floatundisf((u64)value);
}

u32 __fixunssfsi(u32 bits) {
  Parts a = unpack(bits);

  if (a.is_nan || a.is_zero || a.sign)
    return 0;
  if (a.is_inf || a.exp >= 32)
    return 0xffffffffUL;
  if (a.exp < 0)
    return 0;

  // Truncate towards zero, which is what C requires of a conversion.
  if (a.exp >= MANT_BITS)
    return a.mant << (unsigned)(a.exp - MANT_BITS);
  return a.mant >> (unsigned)(MANT_BITS - a.exp);
}

s32 __fixsfsi(u32 bits) {
  Parts a = unpack(bits);
  u32 magnitude;

  if (a.is_nan || a.is_zero)
    return 0;
  if (a.is_inf || a.exp >= 31)
    return a.sign ? (s32)0x80000000UL : (s32)0x7fffffffUL;
  if (a.exp < 0)
    return 0;

  magnitude = a.exp >= MANT_BITS ? a.mant << (unsigned)(a.exp - MANT_BITS)
                                 : a.mant >> (unsigned)(MANT_BITS - a.exp);
  return a.sign ? -(s32)magnitude : (s32)magnitude;
}

u64 __fixunssfdi(u32 bits) {
  Parts a = unpack(bits);

  if (a.is_nan || a.is_zero || a.sign)
    return 0;
  if (a.is_inf || a.exp >= 64)
    return ~(u64)0;
  if (a.exp < 0)
    return 0;

  if (a.exp >= MANT_BITS)
    return (u64)a.mant << (unsigned)(a.exp - MANT_BITS);
  return (u64)(a.mant >> (unsigned)(MANT_BITS - a.exp));
}

s64 __fixsfdi(u32 bits) {
  Parts a = unpack(bits);
  u64 magnitude;

  if (a.is_nan || a.is_zero)
    return 0;
  if (a.is_inf || a.exp >= 63)
    return a.sign ? (s64)((u64)1 << 63) : (s64)(~((u64)1 << 63));
  if (a.exp < 0)
    return 0;

  magnitude = a.exp >= MANT_BITS
                  ? (u64)a.mant << (unsigned)(a.exp - MANT_BITS)
                  : (u64)(a.mant >> (unsigned)(MANT_BITS - a.exp));
  return a.sign ? (s64)((u64)0 - magnitude) : (s64)magnitude;
}


//===----------------------------------------------------------------------===//
// The %e half of printf's decimal conversion
//===----------------------------------------------------------------------===//
//
// This lives here rather than with the rest of the formatter because it is
// the part that genuinely needs floating-point arithmetic: finding the
// decimal exponent of an arbitrary float means dividing it by ten until it is
// in range, and there is no integer shortcut for that across the whole
// exponent range.  Keeping it here means a program that never does any
// floating-point arithmetic links neither this nor the library under it, and
// ug24_printf_float.c -- which does not need either -- can be linked with
// printf unconditionally.
//
// Accuracy: the digits come from repeated multiplication and division by ten
// in single precision, so beyond about seven significant digits the last one
// may be off by one.  %f does not share that limitation; it is exact.

static float power_of_ten_e(int exponent) {
  float result = 1.0f;
  while (exponent-- > 0)
    result *= 10.0f;
  return result;
}

/// Format the magnitude of \p bits as d.ddde+XX.  The caller has already
/// written any minus sign.  \p strip_zeros is set when this is a %g that
/// chose exponential style.
int __ug24_format_float_exp(char *out, u32 bits, int precision, int upper,
                            int strip_zeros) {
  union { u32 u; float f; } cvt;
  float scaled;
  int len = 0, exponent = 0, i, digit;

  cvt.u = bits;
  scaled = cvt.f;

  if (scaled != 0.0f) {
    while (scaled >= 10.0f) { scaled /= 10.0f; exponent++; }
    while (scaled < 1.0f)   { scaled *= 10.0f; exponent--; }
  }

  // Round at the last digit that will be printed.
  scaled += 0.5f / power_of_ten_e(precision);
  if (scaled >= 10.0f) { scaled /= 10.0f; exponent++; }

  digit = (int)scaled;
  out[len++] = (char)('0' + digit);
  scaled -= (float)digit;

  if (precision > 0) {
    out[len++] = '.';
    for (i = 0; i < precision; i++) {
      scaled *= 10.0f;
      digit = (int)scaled;
      if (digit > 9) digit = 9;
      if (digit < 0) digit = 0;
      out[len++] = (char)('0' + digit);
      scaled -= (float)digit;
    }
  }

  if (strip_zeros) {
    while (len > 0 && out[len - 1] == '0')
      len--;
    if (len > 0 && out[len - 1] == '.')
      len--;
  }

  out[len++] = upper ? 'E' : 'e';
  out[len++] = exponent < 0 ? '-' : '+';
  {
    int magnitude = exponent < 0 ? -exponent : exponent;
    if (magnitude < 10)
      out[len++] = '0';
    if (magnitude >= 100) {
      out[len++] = (char)('0' + magnitude / 100);
      magnitude %= 100;
      out[len++] = (char)('0' + magnitude / 10);
    } else if (magnitude >= 10) {
      out[len++] = (char)('0' + magnitude / 10);
    }
    out[len++] = (char)('0' + magnitude % 10);
  }
  return len;
}
