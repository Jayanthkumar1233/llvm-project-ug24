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

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef signed long s32;
typedef unsigned long long u64;
typedef signed long long s64;

#define EXP_BIAS   127
#define EXP_MAX    255
#define MANT_BITS  23
#define MANT_MASK  0x007fffffUL
#define HIDDEN_BIT 0x00800000UL

typedef struct {
  u8 sign;
  short exp;    // unbiased; EXP_MAX - EXP_BIAS marks inf/NaN
  u32 mant;     // with the hidden bit in place, i.e. 1.f scaled by 2^23
  u8 is_zero;
  u8 is_inf;
  u8 is_nan;
} Parts;

static Parts unpack(u32 bits) {
  Parts p;
  unsigned raw = (unsigned)((bits >> MANT_BITS) & 0xff);

  p.sign = (u8)((bits >> 31) & 1);
  p.mant = bits & MANT_MASK;
  p.is_zero = p.is_inf = p.is_nan = 0;

  if (raw == 0) {
    if (p.mant == 0) {
      p.is_zero = 1;
      p.exp = 0;
    } else {
      // Subnormal: no hidden bit, and the exponent is that of the smallest
      // normal rather than zero.
      p.exp = (short)(1 - EXP_BIAS);
    }
  } else if (raw == EXP_MAX) {
    p.exp = (short)(EXP_MAX - EXP_BIAS);
    if (p.mant)
      p.is_nan = 1;
    else
      p.is_inf = 1;
  } else {
    p.exp = (short)((short)raw - EXP_BIAS);
    p.mant |= HIDDEN_BIT;
  }
  return p;
}

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
// Decimal formatting for printf
//===----------------------------------------------------------------------===//
//
// This lives here, in the floating-point runtime, rather than in ug24_stdio.c,
// so that a program which never touches a float does not pay for it.  The
// formatter and the arithmetic it calls are one archive member: the linker
// pulls it in exactly when the program already needs soft float, and printf's
// weak stub covers the case where it does not.
//
// The one program this gets wrong is one that prints a floating-point
// *constant* and does no arithmetic at all, which needs no soft float and so
// leaves the stub in place.  Link that with -Wl,-u,__ug24_format_float.
//
// Accuracy: the digits are produced by repeated multiplication and division
// by ten in single precision, so the last of more than about seven
// significant digits may be off by one.  A float carries no more than that.

#define FORMAT_MAX_PRECISION 17

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
// a million instructions that way, and printing a handful of floats ran the
// simulator past its instruction limit.  Subtraction is at most nine steps
// per digit, and 64-bit compare and subtract are expanded inline by the
// backend, so this needs no helper at all -- which also keeps the 64-bit
// division out of any program that only prints floats.
static const u64 kPowersOfTen[] = {
  10000000000000000000ULL, 1000000000000000000ULL, 100000000000000000ULL,
  10000000000000000ULL,    1000000000000000ULL,    100000000000000ULL,
  10000000000000ULL,       1000000000000ULL,       100000000000ULL,
  10000000000ULL,          1000000000ULL,          100000000ULL,
  10000000ULL,             1000000ULL,             100000ULL,
  10000ULL,                1000ULL,                100ULL,
  10ULL,                   1ULL,
};

static int format_u64(char *out, u64 value) {
  int len = 0;
  unsigned i;
  int leading = 1;

  for (i = 0; i < sizeof kPowersOfTen / sizeof kPowersOfTen[0]; i++) {
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
    out[len++] = '0';          // the value was zero
  return len;
}

// %g prints no trailing zeros in its fraction, and no trailing point once
// they are gone.  Called while \p len still ends at the last digit: for %e
// that is before the exponent marker has been appended.
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

static float power_of_ten(int exponent) {
  float result = 1.0f;
  while (exponent-- > 0)
    result *= 10.0f;
  return result;
}

/// Format the IEEE single in \p bits into \p out, which must have room for 48
/// characters.  \p conv is one of f F e E g G; \p precision is -1 for the
/// default.  Returns the number of characters written.
int __ug24_format_float(char *out, u32 bits, int precision, char conv) {
  union { u32 u; float f; } cvt;
  float value, scaled;
  int len = 0, negative, exponent = 0, i;
  char lower = (char)(conv | 0x20);
  int strip_zeros = 0;

  cvt.u = bits;
  value = cvt.f;
  negative = (int)((bits >> 31) & 1);

  {
    Parts p = unpack(bits);
    if (p.is_nan)
      return copy_literal(out, (conv & 0x20) ? "nan" : "NAN");
    if (p.is_inf) {
      if (negative)
        out[len++] = '-';
      return len + copy_literal(out + len, (conv & 0x20) ? "inf" : "INF");
    }
  }

  if (precision < 0)
    precision = 6;
  if (precision > FORMAT_MAX_PRECISION)
    precision = FORMAT_MAX_PRECISION;

  scaled = negative ? -value : value;

  // Find the decimal exponent, which %e needs outright and %g needs in order
  // to choose a style.  Only those two: this loop is the one place the
  // formatter divides floats, and %f is much the most common conversion, so
  // it is worth keeping it off this path.
  if (lower != 'f' && scaled != 0.0f) {
    while (scaled >= 10.0f) { scaled /= 10.0f; exponent++; }
    while (scaled < 1.0f)   { scaled *= 10.0f; exponent--; }
  }

  if (lower == 'g') {
    // C's rule: %e when the exponent is below -4 or not less than the
    // precision, %f otherwise.  A precision of 0 is treated as 1.
    if (precision == 0)
      precision = 1;
    if (exponent < -4 || exponent >= precision) {
      lower = 'e';
      precision--;                 // %e counts digits after the point
    } else {
      lower = 'f';
      precision -= exponent + 1;
      if (precision < 0)
        precision = 0;
    }
    strip_zeros = 1;               // %g prints no trailing zeros
  }

  if (negative)
    out[len++] = '-';

  if (lower == 'e') {
    int digit;

    // Round at the last digit that will be printed.
    scaled += 0.5f / power_of_ten(precision);
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
        out[len++] = (char)('0' + digit);
        scaled -= (float)digit;
      }
    }

    if (strip_zeros)
      len = trim_zeros(out, len);

    out[len++] = (conv & 0x20) ? 'e' : 'E';
    out[len++] = exponent < 0 ? '-' : '+';
    {
      int magnitude = exponent < 0 ? -exponent : exponent;
      if (magnitude < 10)
        out[len++] = '0';
      len += format_u64(out + len, (u64)(u32)magnitude);
    }
    return len;
  }

  // %f, from the mantissa and the exponent rather than from float
  // arithmetic.
  //
  // A float is exactly mant x 2^(exp-23).  Splitting that at the binary point
  // gives an integer part and a binary fraction, and the fraction fits in 32
  // bits for every value with digits worth printing -- so shifts recover it
  // exactly, and multiplying it by ten recovers its decimal digits exactly
  // too.  The earlier version did this by repeatedly multiplying the float by
  // ten, which accumulated its own error: 1144.22f came out as 1144.219970
  // where the value rounds to 1144.219971.
  {
    Parts p = unpack(bits);
    int shift = p.exp - MANT_BITS;   // value = p.mant * 2^shift
    u64 integer_part = 0;
    u32 fraction = 0;                // the fraction below the point, x 2^32
    char digits[FORMAT_MAX_PRECISION];
    int last_parity;

    // A 24-bit mantissa shifted left by more than 40 no longer fits a 64-bit
    // integer part.  A float that large has no digits left to lose anyway,
    // so it goes out in %e style.
    if (!p.is_zero && shift > 40)
      return len + __ug24_format_float(out + len, bits & 0x7fffffffUL,
                                       precision, (conv & 0x20) ? 'e' : 'E');

    if (p.is_zero) {
      // nothing to split
    } else if (shift >= 0) {
      integer_part = (u64)p.mant << shift;
    } else {
      unsigned r = (unsigned)(-shift);

      integer_part = r < 24 ? (u64)(p.mant >> r) : 0;

      // The fraction is the low r bits of the mantissa, scaled so that its
      // most significant bit lands in bit 31.  Each branch avoids a shift by
      // 32 or more, which is undefined.
      if (r < 32)
        fraction = (p.mant & (((u32)1 << r) - 1)) << (32 - r);
      else if (r == 32)
        fraction = p.mant;
      else if (r < 56)
        fraction = p.mant >> (r - 32);
      // Beyond that the whole value is under 2^-33 and every printed digit
      // is zero.
    }

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
        integer_part++;            // the carry ran off the top of the fraction
    }

    len += format_u64(out + len, integer_part);

    if (precision > 0) {
      out[len++] = '.';
      for (i = 0; i < precision; i++)
        out[len++] = digits[i];
    }
  }

  if (strip_zeros)
    len = trim_zeros(out, len);
  return len;
}
