//===-- ug24_builtins.c - Runtime helpers for the uG24 backend ------------===//
//
// The uG24 ALU is 8 bits wide and has no variable shift, so the compiler
// lowers 16-bit shifts, multiplies and divides to the helpers below.  They are
// written so that they only ever need operations the backend can select
// directly: shifts by a constant of one, and 8-bit add/subtract/compare.
//
//===----------------------------------------------------------------------===//

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long u32;
typedef signed long s32;

//===----------------------------------------------------------------------===//
// 16-bit shifts
//===----------------------------------------------------------------------===//
//
// These are written as a switch over the shift amount with only *constant*
// shifts in each arm.  A loop that doubles a value is recognised by the
// optimiser as a variable shift, which it then lowers back into a call to
// this very function; spelling the cases out avoids that recursion.

u16 __ashlhi3(u16 value, u8 amount) {
  u8 lo = (u8)value;
  u8 hi = (u8)(value >> 8);

  switch (amount & 0x1f) {
  case 0:  break;
  case 1:  hi = (u8)((hi << 1) | (lo >> 7)); lo = (u8)(lo << 1); break;
  case 2:  hi = (u8)((hi << 2) | (lo >> 6)); lo = (u8)(lo << 2); break;
  case 3:  hi = (u8)((hi << 3) | (lo >> 5)); lo = (u8)(lo << 3); break;
  case 4:  hi = (u8)((hi << 4) | (lo >> 4)); lo = (u8)(lo << 4); break;
  case 5:  hi = (u8)((hi << 5) | (lo >> 3)); lo = (u8)(lo << 5); break;
  case 6:  hi = (u8)((hi << 6) | (lo >> 2)); lo = (u8)(lo << 6); break;
  case 7:  hi = (u8)((hi << 7) | (lo >> 1)); lo = (u8)(lo << 7); break;
  case 8:  hi = lo; lo = 0; break;
  case 9:  hi = (u8)(lo << 1); lo = 0; break;
  case 10: hi = (u8)(lo << 2); lo = 0; break;
  case 11: hi = (u8)(lo << 3); lo = 0; break;
  case 12: hi = (u8)(lo << 4); lo = 0; break;
  case 13: hi = (u8)(lo << 5); lo = 0; break;
  case 14: hi = (u8)(lo << 6); lo = 0; break;
  case 15: hi = (u8)(lo << 7); lo = 0; break;
  default: hi = 0; lo = 0; break;
  }
  return (u16)(((u16)hi << 8) | lo);
}

u16 __lshrhi3(u16 value, u8 amount) {
  u8 lo = (u8)value;
  u8 hi = (u8)(value >> 8);

  switch (amount & 0x1f) {
  case 0:  break;
  case 1:  lo = (u8)((lo >> 1) | (hi << 7)); hi = (u8)(hi >> 1); break;
  case 2:  lo = (u8)((lo >> 2) | (hi << 6)); hi = (u8)(hi >> 2); break;
  case 3:  lo = (u8)((lo >> 3) | (hi << 5)); hi = (u8)(hi >> 3); break;
  case 4:  lo = (u8)((lo >> 4) | (hi << 4)); hi = (u8)(hi >> 4); break;
  case 5:  lo = (u8)((lo >> 5) | (hi << 3)); hi = (u8)(hi >> 5); break;
  case 6:  lo = (u8)((lo >> 6) | (hi << 2)); hi = (u8)(hi >> 6); break;
  case 7:  lo = (u8)((lo >> 7) | (hi << 1)); hi = (u8)(hi >> 7); break;
  case 8:  lo = hi; hi = 0; break;
  case 9:  lo = (u8)(hi >> 1); hi = 0; break;
  case 10: lo = (u8)(hi >> 2); hi = 0; break;
  case 11: lo = (u8)(hi >> 3); hi = 0; break;
  case 12: lo = (u8)(hi >> 4); hi = 0; break;
  case 13: lo = (u8)(hi >> 5); hi = 0; break;
  case 14: lo = (u8)(hi >> 6); hi = 0; break;
  case 15: lo = (u8)(hi >> 7); hi = 0; break;
  default: lo = 0; hi = 0; break;
  }
  return (u16)(((u16)hi << 8) | lo);
}

s16 __ashrhi3(s16 value, u8 amount) {
  u8 lo = (u8)value;
  s8 hi = (s8)(((u16)value) >> 8);
  u8 fill = (u8)(hi < 0 ? 0xff : 0x00);

  switch (amount & 0x1f) {
  case 0:  break;
  case 1:  lo = (u8)((lo >> 1) | ((u8)hi << 7)); hi = (s8)(hi >> 1); break;
  case 2:  lo = (u8)((lo >> 2) | ((u8)hi << 6)); hi = (s8)(hi >> 2); break;
  case 3:  lo = (u8)((lo >> 3) | ((u8)hi << 5)); hi = (s8)(hi >> 3); break;
  case 4:  lo = (u8)((lo >> 4) | ((u8)hi << 4)); hi = (s8)(hi >> 4); break;
  case 5:  lo = (u8)((lo >> 5) | ((u8)hi << 3)); hi = (s8)(hi >> 5); break;
  case 6:  lo = (u8)((lo >> 6) | ((u8)hi << 2)); hi = (s8)(hi >> 6); break;
  case 7:  lo = (u8)((lo >> 7) | ((u8)hi << 1)); hi = (s8)(hi >> 7); break;
  case 8:  lo = (u8)hi; hi = (s8)fill; break;
  case 9:  lo = (u8)((s8)hi >> 1); hi = (s8)fill; break;
  case 10: lo = (u8)((s8)hi >> 2); hi = (s8)fill; break;
  case 11: lo = (u8)((s8)hi >> 3); hi = (s8)fill; break;
  case 12: lo = (u8)((s8)hi >> 4); hi = (s8)fill; break;
  case 13: lo = (u8)((s8)hi >> 5); hi = (s8)fill; break;
  case 14: lo = (u8)((s8)hi >> 6); hi = (s8)fill; break;
  case 15: lo = (u8)((s8)hi >> 7); hi = (s8)fill; break;
  default: lo = fill; hi = (s8)fill; break;
  }
  return (s16)(((u16)(u8)hi << 8) | lo);
}

//===----------------------------------------------------------------------===//
// 16-bit multiply and divide
//===----------------------------------------------------------------------===//

u16 __mulhi3(u16 a, u16 b) {
  u16 result = 0;
  while (b) {
    if (b & 1)
      result = (u16)(result + a);
    a = (u16)(a + a);
    b = (u16)(b >> 1);
  }
  return result;
}

// Unsigned 16-bit division, producing both quotient and remainder.
static u16 divmod16(u16 num, u16 den, u16 *rem) {
  u16 quot = 0;
  u16 acc = 0;
  u8 bit = 16;

  if (den == 0) {
    *rem = 0;
    return 0;
  }

  while (bit--) {
    acc = (u16)(acc + acc);
    if (num & 0x8000u)
      acc = (u16)(acc + 1);
    num = (u16)(num + num);
    quot = (u16)(quot + quot);
    if (acc >= den) {
      acc = (u16)(acc - den);
      quot = (u16)(quot + 1);
    }
  }

  *rem = acc;
  return quot;
}

u16 __udivhi3(u16 a, u16 b) {
  u16 rem;
  return divmod16(a, b, &rem);
}

u16 __umodhi3(u16 a, u16 b) {
  u16 rem;
  divmod16(a, b, &rem);
  return rem;
}

s16 __divhi3(s16 a, s16 b) {
  u8 negate = 0;
  u16 ua, ub, q;

  if (a < 0) { a = (s16)-a; negate ^= 1; }
  if (b < 0) { b = (s16)-b; negate ^= 1; }
  ua = (u16)a;
  ub = (u16)b;
  q = __udivhi3(ua, ub);
  return negate ? (s16)-(s16)q : (s16)q;
}

s16 __modhi3(s16 a, s16 b) {
  u8 negate = 0;
  u16 ua, ub, r;

  if (a < 0) { a = (s16)-a; negate = 1; }
  if (b < 0) { b = (s16)-b; }
  ua = (u16)a;
  ub = (u16)b;
  r = __umodhi3(ua, ub);
  return negate ? (s16)-(s16)r : (s16)r;
}

//===----------------------------------------------------------------------===//
// 8-bit divide - the hardware DIV writes its result to W, which the backend
// does not model as a pattern, so it goes through a helper too.
//===----------------------------------------------------------------------===//

u8 __udivqi3(u8 a, u8 b) { return (u8)__udivhi3(a, b); }
u8 __umodqi3(u8 a, u8 b) { return (u8)__umodhi3(a, b); }

s8 __divqi3(s8 a, s8 b) { return (s8)__divhi3(a, b); }
s8 __modqi3(s8 a, s8 b) { return (s8)__modhi3(a, b); }

// The hardware MUL produces the full 16-bit product in W, but the selector
// does not model that implicit destination yet, so 8-bit multiplies come
// here too.
u8 __mulqi3(u8 a, u8 b) { return (u8)__mulhi3(a, b); }

//===----------------------------------------------------------------------===//
// Memory helpers, used by struct assignment and array initialisation
//===----------------------------------------------------------------------===//

void *memcpy(void *dst, const void *src, unsigned size) {
  u8 *d = (u8 *)dst;
  const u8 *s = (const u8 *)src;
  while (size--)
    *d++ = *s++;
  return dst;
}

void *memset(void *dst, int value, unsigned size) {
  u8 *d = (u8 *)dst;
  while (size--)
    *d++ = (u8)value;
  return dst;
}

void *memmove(void *dst, const void *src, unsigned size) {
  u8 *d = (u8 *)dst;
  const u8 *s = (const u8 *)src;
  if (d < s) {
    while (size--)
      *d++ = *s++;
  } else {
    d += size;
    s += size;
    while (size--)
      *--d = *--s;
  }
  return dst;
}

//===----------------------------------------------------------------------===//
// 32-bit shifts
//===----------------------------------------------------------------------===//
//
// Written on the two 16-bit halves.  The only shifts that appear are by the
// constant 1 and by the constant 16, both of which the backend expands inline,
// so nothing here can turn back into a call to itself.

u32 __ashlsi3(u32 value, unsigned amount) {
  u16 lo = (u16)value;
  u16 hi = (u16)(value >> 16);
  while (amount--) {
    hi = (u16)((u16)(hi << 1) | (u16)(lo >> 15));
    lo = (u16)(lo << 1);
  }
  return ((u32)hi << 16) | (u32)lo;
}

u32 __lshrsi3(u32 value, unsigned amount) {
  u16 lo = (u16)value;
  u16 hi = (u16)(value >> 16);
  while (amount--) {
    lo = (u16)((u16)(lo >> 1) | (u16)(hi << 15));
    hi = (u16)(hi >> 1);
  }
  return ((u32)hi << 16) | (u32)lo;
}

s32 __ashrsi3(s32 value, unsigned amount) {
  u16 lo = (u16)value;
  s16 hi = (s16)(value >> 16);
  while (amount--) {
    lo = (u16)((u16)(lo >> 1) | (u16)((u16)hi << 15));
    hi = (s16)(hi >> 1);
  }
  return ((s32)hi << 16) | (s32)(u32)lo;
}

//===----------------------------------------------------------------------===//
// 32-bit multiply
//===----------------------------------------------------------------------===//

/// Full 16x16 -> 32 product, built from four 8x8 products so the hardware
/// multiplier does the work.
static u32 mul16to32(u16 a, u16 b) {
  // The halves are kept in u8 variables on purpose.  Written as (a & 0xff)
  // the operands reach the backend as a mask rather than a widened byte, and
  // the 8x8 -> 16 hardware multiply does not match.
  u8 a0 = (u8)a, a1 = (u8)(a >> 8);
  u8 b0 = (u8)b, b1 = (u8)(b >> 8);

  u16 p00 = (u16)((u16)a0 * (u16)b0);
  u16 p01 = (u16)((u16)a0 * (u16)b1);
  u16 p10 = (u16)((u16)a1 * (u16)b0);
  u16 p11 = (u16)((u16)a1 * (u16)b1);

  u32 result = (u32)p00;
  u32 middle = (u32)p01 + (u32)p10;

  result += middle << 8;
  result += (u32)p11 << 16;
  return result;
}

u32 __mulsi3(u32 a, u32 b) {
  u16 alo = (u16)a, ahi = (u16)(a >> 16);
  u16 blo = (u16)b, bhi = (u16)(b >> 16);

  u32 result = mul16to32(alo, blo);
  result += (u32)(u16)(alo * bhi) << 16;
  result += (u32)(u16)(ahi * blo) << 16;
  return result;
}

//===----------------------------------------------------------------------===//
// 32-bit divide
//===----------------------------------------------------------------------===//
//
// Restoring division, one bit at a time, on the halves.  32 iterations is slow
// but it is the only thing this ALU can do.

// noinline: inlining this into the four wrappers below pushes the 8-bit
// register file past what the allocator can colour.
__attribute__((noinline))
static u32 divmod32(u32 num, u32 den, u32 *rem) {
  u32 quotient = 0;
  u32 remainder = 0;
  u8 bit = 32;

  if (den == 0) {          // Undefined in C; return all-ones rather than hang.
    if (rem)
      *rem = num;
    return 0xFFFFFFFFUL;
  }

  // Shift one bit of the numerator into the remainder at a time.  Only shifts
  // by the constant 1 and 31 appear, and the backend expands those inline, so
  // this cannot turn back into a call to __ashlsi3 or to itself.
  while (bit--) {
    remainder = (remainder << 1) | ((num >> 31) & 1UL);
    num <<= 1;
    quotient <<= 1;
    if (remainder >= den) {
      remainder -= den;
      quotient |= 1UL;
    }
  }

  if (rem)
    *rem = remainder;
  return quotient;
}

u32 __udivsi3(u32 a, u32 b) { return divmod32(a, b, 0); }

u32 __umodsi3(u32 a, u32 b) {
  u32 rem;
  divmod32(a, b, &rem);
  return rem;
}

s32 __divsi3(s32 a, s32 b) {
  u8 negate = 0;
  u32 ua, ub, q;

  if (a < 0) { a = -a; negate ^= 1; }
  if (b < 0) { b = -b; negate ^= 1; }
  ua = (u32)a;
  ub = (u32)b;
  q = divmod32(ua, ub, 0);
  return negate ? -(s32)q : (s32)q;
}

s32 __modsi3(s32 a, s32 b) {
  u8 negate = 0;
  u32 ua, ub, rem;

  if (a < 0) { a = -a; negate = 1; }
  if (b < 0) { b = -b; }
  ua = (u32)a;
  ub = (u32)b;
  divmod32(ua, ub, &rem);
  return negate ? -(s32)rem : (s32)rem;
}

//===----------------------------------------------------------------------===//
// String helpers
//===----------------------------------------------------------------------===//

unsigned strlen(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  return (unsigned)(p - s);
}

int memcmp(const void *a, const void *b, unsigned size) {
  const u8 *x = (const u8 *)a, *y = (const u8 *)b;
  while (size--) {
    if (*x != *y)
      return (int)*x - (int)*y;
    x++; y++;
  }
  return 0;
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) { a++; b++; }
  return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, unsigned n) {
  while (n && *a && *a == *b) { a++; b++; n--; }
  if (n == 0)
    return 0;
  return (int)(u8)*a - (int)(u8)*b;
}

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while ((*d++ = *src++) != '\0')
    ;
  return dst;
}

char *strncpy(char *dst, const char *src, unsigned n) {
  char *d = dst;
  while (n && *src) { *d++ = *src++; n--; }
  while (n--) *d++ = '\0';
  return dst;
}

char *strcat(char *dst, const char *src) {
  char *d = dst;
  while (*d) d++;
  while ((*d++ = *src++) != '\0')
    ;
  return dst;
}

char *strchr(const char *s, int c) {
  while (*s) {
    if (*s == (char)c)
      return (char *)s;
    s++;
  }
  return (c == 0) ? (char *)s : 0;
}
