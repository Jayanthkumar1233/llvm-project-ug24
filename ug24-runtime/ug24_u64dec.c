//===-- ug24_u64dec.c - Decimal digits of a 64-bit value ------------------===//
//
// Its own file because two unrelated callers need it and neither should drag
// the other in: printf's %%lld, and the %%f formatter in ug24_printf_float.c.
// A program that replaces the float formatter still gets working %%lld.
//
// No division anywhere.  __udivdi3 exists, but one call costs on the order of
// thirty thousand instructions on an 8-bit ALU, and printing a twenty-digit
// number would need twenty of them.  Subtracting powers of ten instead costs
// at most nine subtractions per digit.
//
//===----------------------------------------------------------------------===//

typedef unsigned char      u8;
typedef unsigned long long u64;

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

/// Decimal digits of \p value, most significant first, into \p out; returns
/// how many.
int __ug24_u64_to_decimal(char *out, u64 value);

int __ug24_u64_to_decimal(char *out, u64 value) {
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
int __ug24_decimal_digits(u64 value) {
  int n = 1;
  unsigned i;

  for (i = 0; i < POWERS_OF_TEN; i++)
    if (value >= kPowersOfTen[i]) {
      n = (int)(POWERS_OF_TEN - i);
      break;
    }
  return n;
}
