//===-- ug24_int64.c - 64-bit runtime helpers for the uG24 ----------------===//
//
// `long long` is 64 bits on this target.  Addition, subtraction, comparison
// and the bitwise operations are expanded inline by the backend on the four
// 16-bit halves; multiply, divide and the variable shifts become calls to the
// helpers below.
//
// Everything works on a union of the value and its four 16-bit limbs, least
// significant first, so that no 64-bit shift is needed to take a value apart
// -- a shift is what these functions are for, and taking one here would call
// straight back into this file.
//
//===----------------------------------------------------------------------===//

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef signed long long s64;

#define LIMBS 4

typedef union {
  u64 v;
  u16 w[LIMBS];
} U64;

//===----------------------------------------------------------------------===//
// Shifts
//===----------------------------------------------------------------------===//

// A shift is a limb rotation plus a shift within a limb.  Done the other way
// round -- as a loop of single-bit shifts -- a shift of 63 would cost 63
// passes over four limbs.

static U64 shift_left(U64 x, unsigned amount) {
  U64 r;
  unsigned limbs = (amount >> 4) & 3;
  unsigned bits = amount & 15;
  int i;

  if (amount >= 64) {
    r.v = 0;
    return r;
  }

  for (i = LIMBS - 1; i >= 0; i--) {
    unsigned src = (unsigned)i - limbs;
    u16 value = (unsigned)i >= limbs ? x.w[src] : 0;
    if (bits && (unsigned)i > limbs)
      value = (u16)((value << bits) | (u16)(x.w[src - 1] >> (16 - bits)));
    else if (bits)
      value = (u16)(value << bits);
    r.w[i] = value;
  }
  return r;
}

static U64 shift_right(U64 x, unsigned amount, u16 fill) {
  U64 r;
  unsigned limbs = (amount >> 4) & 3;
  unsigned bits = amount & 15;
  unsigned i;

  if (amount >= 64) {
    r.w[0] = r.w[1] = r.w[2] = r.w[3] = fill;
    return r;
  }

  for (i = 0; i < LIMBS; i++) {
    unsigned src = i + limbs;
    u16 value = src < LIMBS ? x.w[src] : fill;
    if (bits) {
      u16 next = src + 1 < LIMBS ? x.w[src + 1] : fill;
      value = (u16)((value >> bits) | (u16)(next << (16 - bits)));
    }
    r.w[i] = value;
  }
  return r;
}

u64 __ashldi3(u64 value, int amount) {
  U64 x;
  x.v = value;
  return shift_left(x, (unsigned)amount).v;
}

u64 __lshrdi3(u64 value, int amount) {
  U64 x;
  x.v = value;
  return shift_right(x, (unsigned)amount, 0).v;
}

s64 __ashrdi3(s64 value, int amount) {
  U64 x;
  x.v = (u64)value;
  return (s64)shift_right(x, (unsigned)amount,
                          (x.w[LIMBS - 1] & 0x8000u) ? 0xffffu : 0)
      .v;
}

//===----------------------------------------------------------------------===//
// Multiply
//===----------------------------------------------------------------------===//

u64 __muldi3(u64 a, u64 b) {
  U64 A, B, R;
  unsigned i, j;

  A.v = a;
  B.v = b;
  R.w[0] = R.w[1] = R.w[2] = R.w[3] = 0;

  // Schoolbook, limb by limb.  The widest intermediate is
  // 0xfffe0001 + 0xffff + 0xffff, which is exactly 32 bits, so the partial
  // sum never needs more than the u32 it is held in.
  for (i = 0; i < LIMBS; i++) {
    u16 carry = 0;
    for (j = 0; i + j < LIMBS; j++) {
      u32 t = (u32)R.w[i + j] + (u32)A.w[i] * (u32)B.w[j] + (u32)carry;
      R.w[i + j] = (u16)t;
      carry = (u16)(t >> 16);
    }
  }
  return R.v;
}

//===----------------------------------------------------------------------===//
// Divide
//===----------------------------------------------------------------------===//

static int ge64(const U64 *a, const U64 *b) {
  int i;
  for (i = LIMBS - 1; i >= 0; i--)
    if (a->w[i] != b->w[i])
      return a->w[i] > b->w[i];
  return 1;
}

static void sub64(U64 *a, const U64 *b) {
  unsigned i;
  u16 borrow = 0;
  for (i = 0; i < LIMBS; i++) {
    u32 t = (u32)a->w[i] - (u32)b->w[i] - (u32)borrow;
    a->w[i] = (u16)t;
    borrow = (u16)((t >> 16) != 0);
  }
}

// Restoring division, one bit at a time, from the top down.  64 iterations is
// slow, but the alternative needs a wide multiply and this ALU has none.
static void divmod64(U64 num, U64 den, U64 *quot, U64 *rem) {
  U64 q, r;
  int bit;

  q.w[0] = q.w[1] = q.w[2] = q.w[3] = 0;
  r.w[0] = r.w[1] = r.w[2] = r.w[3] = 0;

  if (den.v == 0) {
    // Same shape as the 16- and 32-bit helpers: a zero divisor yields zero
    // rather than trapping, because there is nothing here to trap to.
    *quot = q;
    *rem = r;
    return;
  }

  for (bit = 63; bit >= 0; bit--) {
    unsigned limb = (unsigned)bit >> 4;
    unsigned index = (unsigned)bit & 15;

    r = shift_left(r, 1);
    r.w[0] = (u16)(r.w[0] | ((num.w[limb] >> index) & 1u));
    q = shift_left(q, 1);
    if (ge64(&r, &den)) {
      sub64(&r, &den);
      q.w[0] = (u16)(q.w[0] | 1u);
    }
  }

  *quot = q;
  *rem = r;
}

u64 __udivdi3(u64 a, u64 b) {
  U64 A, B, q, r;
  A.v = a;
  B.v = b;
  divmod64(A, B, &q, &r);
  return q.v;
}

u64 __umoddi3(u64 a, u64 b) {
  U64 A, B, q, r;
  A.v = a;
  B.v = b;
  divmod64(A, B, &q, &r);
  return r.v;
}

// The sign is read out of the top limb rather than written as "a < 0".  A
// signed 64-bit comparison is one of the things the compiler is entitled to
// turn into a call to __cmpdi2, and __cmpdi2 lives in this same file.
static int negative64(u64 a) {
  U64 A;
  A.v = a;
  return (A.w[LIMBS - 1] & 0x8000u) != 0;
}

s64 __divdi3(s64 a, s64 b) {
  u8 negate = 0;
  u64 ua = (u64)a, ub = (u64)b, q;

  if (negative64(ua)) { ua = (u64)0 - ua; negate ^= 1; }
  if (negative64(ub)) { ub = (u64)0 - ub; negate ^= 1; }
  q = __udivdi3(ua, ub);
  return negate ? (s64)((u64)0 - q) : (s64)q;
}

s64 __moddi3(s64 a, s64 b) {
  u8 negate = 0;
  u64 ua = (u64)a, ub = (u64)b, r;

  // C99 requires the remainder to take the sign of the dividend.
  if (negative64(ua)) { ua = (u64)0 - ua; negate = 1; }
  if (negative64(ub)) { ub = (u64)0 - ub; }
  r = __umoddi3(ua, ub);
  return negate ? (s64)((u64)0 - r) : (s64)r;
}

//===----------------------------------------------------------------------===//
// Comparison
//===----------------------------------------------------------------------===//
//
// The libgcc convention: 0 if a < b, 1 if equal, 2 if a > b.

int __ucmpdi2(u64 a, u64 b) {
  U64 A, B;
  int i;
  A.v = a;
  B.v = b;
  for (i = LIMBS - 1; i >= 0; i--) {
    if (A.w[i] < B.w[i]) return 0;
    if (A.w[i] > B.w[i]) return 2;
  }
  return 1;
}

int __cmpdi2(s64 a, s64 b) {
  U64 A, B;
  int i;
  A.v = (u64)a;
  B.v = (u64)b;

  // Only the top limb is signed; everything below it compares unsigned.
  {
    short ahi = (short)A.w[LIMBS - 1], bhi = (short)B.w[LIMBS - 1];
    if (ahi < bhi) return 0;
    if (ahi > bhi) return 2;
  }
  for (i = LIMBS - 2; i >= 0; i--) {
    if (A.w[i] < B.w[i]) return 0;
    if (A.w[i] > B.w[i]) return 2;
  }
  return 1;
}
