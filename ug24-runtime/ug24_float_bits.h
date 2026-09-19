//===-- ug24_float_bits.h - IEEE single layout, shared ---------------------===//
//
// The bit layout of an IEEE single and the routine that takes one apart.
// Private to the runtime: ug24_float.c needs it for the arithmetic and
// ug24_printf_float.c needs it for the decimal conversion, and the two are
// separate translation units on purpose -- see the comment at the top of
// ug24_printf_float.c.
//
//===----------------------------------------------------------------------===//

#ifndef UG24_FLOAT_BITS_H
#define UG24_FLOAT_BITS_H

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

static inline Parts unpack(u32 bits) {
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


#endif // UG24_FLOAT_BITS_H
