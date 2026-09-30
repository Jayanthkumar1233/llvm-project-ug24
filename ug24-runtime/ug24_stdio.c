//===-- ug24_stdio.c - A small stdio for the uG24 -------------------------===//
//
// Enough of <stdio.h> to write ordinary C on this target: printf and its
// relatives, over the memory-mapped UART.  There is no operating system and no
// file system, so every stream is the console.
//
// Floating point (%f, %e, %g) and 64-bit integers (%lld) are both supported.
// The target has neither an FPU nor 64-bit arithmetic, so both go through the
// helpers in ug24_float.c, ug24_printf_float.c and ug24_int64.c, and both
// avoid 64-bit division: it exists but costs tens of thousands of
// instructions.
//
//===----------------------------------------------------------------------===//

#include <stdarg.h>
#include <stdio.h>
#include <ug24.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

// The console comes from the linker script by way of <ug24.h>; see
// docs/uG24-platform.md for why it is a symbol rather than an address.
#define UART_TX     UG24_UART_TX
#define UART_STATUS UG24_UART_STATUS
#define UART_READY  UG24_UART_READY

//===----------------------------------------------------------------------===//
// Floating-point conversion
//===----------------------------------------------------------------------===//
//
// The conversion itself is in ug24_printf_float.c.  It is referenced normally
// -- not weakly -- because it needs no floating-point arithmetic of its own:
// a float is exactly mant x 2^(exp-23), so %f comes out of shifts and a
// multiply by ten.  Linking it with printf costs its own code and nothing
// else.  The soft-float library is only reached for %e, which that file
// delegates to a weak symbol.

int __ug24_format_float(char *out, unsigned long bits, int precision,
                        char conv);

//===----------------------------------------------------------------------===//
// Output sink: either the UART or a caller-supplied buffer.
//===----------------------------------------------------------------------===//

typedef struct {
  char *buffer;      ///< NULL when writing to the console.
  unsigned capacity; ///< Space in the buffer, including the terminator.
  unsigned written;  ///< Characters produced, whether or not they fitted.
} Sink;

static void sink_put(Sink *sink, char c) {
  if (sink->buffer) {
    if (sink->written + 1 < sink->capacity)
      sink->buffer[sink->written] = c;
  } else {
    while ((UART_STATUS & UART_READY) == 0)
      ;
    UART_TX = (unsigned char)c;
  }
  sink->written++;
}

static void sink_pad(Sink *sink, char c, int count) {
  while (count-- > 0)
    sink_put(sink, c);
}

//===----------------------------------------------------------------------===//
// Conversion
//===----------------------------------------------------------------------===//

#define BUF_DIGITS 34

static const char digits_lower[] = "0123456789abcdef";
static const char digits_upper[] = "0123456789ABCDEF";

/// Render \p value in \p base into \p out, least significant digit first.
/// Returns the number of digits written.
static int to_digits(char *out, u32 value, unsigned base, const char *alphabet) {
  int length = 0;
  do {
    out[length++] = alphabet[value % base];
    value /= base;
  } while (value);
  return length;
}

typedef struct {
  unsigned left  : 1;   ///< '-'
  unsigned zero  : 1;   ///< '0'
  unsigned plus  : 1;   ///< '+'
  unsigned space : 1;   ///< ' '
  unsigned alt   : 1;   ///< '#'
} Flags;

/// Everything about printing a number except producing its digits: sign,
/// "0x", zero padding, field width and alignment.  \p digits holds \p count
/// digits least significant first, as both to_digits functions return them.
static void emit_digits(Sink *sink, const char *digits, int count, int is_zero,
                        unsigned base, int is_negative, Flags flags, int width,
                        int precision, const char *alphabet) {
  // A precision of zero prints nothing at all for a zero value.
  if (precision == 0 && is_zero)
    count = 0;

  int zeros = (precision > count) ? precision - count : 0;

  char sign = 0;
  if (is_negative)      sign = '-';
  else if (flags.plus)  sign = '+';
  else if (flags.space) sign = ' ';

  int prefix = 0;
  if (flags.alt && base == 16 && !is_zero) prefix = 2;     // "0x"
  else if (flags.alt && base == 8)         prefix = 1;     // "0"

  int body = count + zeros + (sign ? 1 : 0) + prefix;

  // '0' padding fills to the width, but only when the field is right-aligned
  // and no explicit precision was given.
  if (!flags.left) {
    if (flags.zero && precision < 0 && width > body) {
      zeros += width - body;
      body = width;
    } else {
      sink_pad(sink, ' ', width - body);
    }
  }

  if (sign)
    sink_put(sink, sign);
  if (prefix == 2) {
    sink_put(sink, '0');
    sink_put(sink, alphabet == digits_upper ? 'X' : 'x');
  } else if (prefix == 1) {
    sink_put(sink, '0');
  }

  sink_pad(sink, '0', zeros);
  while (count--)
    sink_put(sink, digits[count]);

  if (flags.left)
    sink_pad(sink, ' ', width - body);
}

static void emit_number(Sink *sink, u32 value, unsigned base, int is_negative,
                        Flags flags, int width, int precision,
                        const char *alphabet) {
  char digits[BUF_DIGITS];
  int count = to_digits(digits, value, base, alphabet);
  emit_digits(sink, digits, count, value == 0, base, is_negative, flags, width,
              precision, alphabet);
}

//===----------------------------------------------------------------------===//
// 64-bit conversions
//===----------------------------------------------------------------------===//
//
// %lld and %llu are separate from the 32-bit path because a u64 divide on this
// target is a library call costing tens of thousands of instructions.  Base ten
// therefore borrows the digit loop in ug24_printf_float.c, which subtracts
// powers of ten instead of dividing and is linked with printf anyway; the other
// bases are shifts.

// Producing the digits lives in ug24_printf_u64.c, referenced normally so that
// %lld is correct without a flag, and in its own archive member so a program
// that never prints one can replace it and get the space back.
int __ug24_format_u64(char *out, u64 value, unsigned base,
                      const char *alphabet);

static void emit_number64(Sink *sink, u64 value, unsigned base,
                          int is_negative, Flags flags, int width,
                          int precision, const char *alphabet) {
  char digits[BUF_DIGITS];
  int count = __ug24_format_u64(digits, value, base, alphabet);
  emit_digits(sink, digits, count, value == 0, base, is_negative, flags, width,
              precision, alphabet);
}

//===----------------------------------------------------------------------===//
// The formatter
//===----------------------------------------------------------------------===//

static int format(Sink *sink, const char *fmt, va_list ap) {
  while (*fmt) {
    if (*fmt != '%') {
      sink_put(sink, *fmt++);
      continue;
    }
    fmt++;                                    // step over '%'

    if (*fmt == '%') {                        // "%%"
      sink_put(sink, '%');
      fmt++;
      continue;
    }

    Flags flags = {0, 0, 0, 0, 0};
    for (;;) {
      if      (*fmt == '-') flags.left  = 1;
      else if (*fmt == '0') flags.zero  = 1;
      else if (*fmt == '+') flags.plus  = 1;
      else if (*fmt == ' ') flags.space = 1;
      else if (*fmt == '#') flags.alt   = 1;
      else break;
      fmt++;
    }

    int width = 0;
    if (*fmt == '*') {
      width = va_arg(ap, int);
      if (width < 0) { flags.left = 1; width = -width; }
      fmt++;
    } else {
      while (*fmt >= '0' && *fmt <= '9')
        width = width * 10 + (*fmt++ - '0');
    }

    int precision = -1;
    if (*fmt == '.') {
      fmt++;
      precision = 0;
      if (*fmt == '*') {
        precision = va_arg(ap, int);
        fmt++;
      } else {
        while (*fmt >= '0' && *fmt <= '9')
          precision = precision * 10 + (*fmt++ - '0');
      }
    }

    // Length modifier.  Each one has to match the width the caller actually
    // pushed, or va_arg consumes the wrong number of bytes and every later
    // argument in the same call comes out wrong too.
    //
    //   h, hh  absorbed by the default argument promotions
    //   l      long, 32 bits
    //   ll     long long, 64 bits
    //   j      intmax_t, which is long long here
    //   z, t   size_t and ptrdiff_t, both 16 bits on this target, so they take
    //          the default path -- reading a long for them used to consume two
    //          bytes too many
    int is_long = 0, is_long_long = 0;
    for (;;) {
      if (*fmt == 'h' || *fmt == 'z' || *fmt == 't') { fmt++; continue; }
      if (*fmt == 'j')             { is_long_long = 1; fmt++; continue; }
      if (*fmt == 'l') {
        fmt++;
        if (*fmt == 'l') { is_long_long = 1; fmt++; }
        else             { is_long = 1; }
        continue;
      }
      break;
    }

    char conv = *fmt++;
    switch (conv) {
    case 'd':
    case 'i': {
      if (is_long_long) {
        long long value = va_arg(ap, long long);
        int negative = value < 0;
        u64 magnitude = negative ? (u64)(-value) : (u64)value;
        emit_number64(sink, magnitude, 10, negative, flags, width, precision,
                      digits_lower);
        break;
      }
      long value = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
      int negative = value < 0;
      u32 magnitude = negative ? (u32)(-value) : (u32)value;
      emit_number(sink, magnitude, 10, negative, flags, width, precision,
                  digits_lower);
      break;
    }
    case 'u':
    case 'x':
    case 'X':
    case 'o': {
      unsigned base = (conv == 'u') ? 10 : (conv == 'o') ? 8 : 16;
      const char *alphabet = (conv == 'X') ? digits_upper : digits_lower;
      if (is_long_long) {
        u64 value = va_arg(ap, unsigned long long);
        emit_number64(sink, value, base, 0, flags, width, precision, alphabet);
        break;
      }
      u32 value = is_long ? va_arg(ap, unsigned long)
                          : (u32)va_arg(ap, unsigned int);
      emit_number(sink, value, base, 0, flags, width, precision, alphabet);
      break;
    }
    case 'p': {
      u32 value = (u32)(unsigned)va_arg(ap, void *);
      flags.alt = 1;
      emit_number(sink, value, 16, 0, flags, width, 4, digits_lower);
      break;
    }
    case 'c': {
      char c = (char)va_arg(ap, int);
      if (!flags.left) sink_pad(sink, ' ', width - 1);
      sink_put(sink, c);
      if (flags.left)  sink_pad(sink, ' ', width - 1);
      break;
    }
    case 's': {
      const char *s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      int length = 0;
      while (s[length] && (precision < 0 || length < precision))
        length++;
      if (!flags.left) sink_pad(sink, ' ', width - length);
      for (int i = 0; i < length; i++)
        sink_put(sink, s[i]);
      if (flags.left)  sink_pad(sink, ' ', width - length);
      break;
    }
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
      // double is IEEE single on this target, so the argument is four bytes
      // and the union below takes it apart without any float arithmetic --
      // which is the point: this file must stay free of soft-float calls so
      // that a program with no floating point in it links none.
      union { double d; unsigned long bits; } value;
      char buffer[48];
      int length;

      value.d = va_arg(ap, double);
      length = __ug24_format_float(buffer, value.bits, precision, conv);

      if (!flags.left) sink_pad(sink, flags.zero ? '0' : ' ', width - length);
      for (int i = 0; i < length; i++)
        sink_put(sink, buffer[i]);
      if (flags.left)  sink_pad(sink, ' ', width - length);
      break;
    }
    case 'a': case 'A':
      // Hexadecimal floating point has no users here and is not worth the
      // code; the argument is still consumed so the rest of the format is
      // not thrown out of step.
      (void)va_arg(ap, double);
      sink_put(sink, '<'); sink_put(sink, 'f'); sink_put(sink, 'p');
      sink_put(sink, '?'); sink_put(sink, '>');
      break;
    case '\0':
      return (int)sink->written;              // trailing '%'
    default:
      sink_put(sink, '%');
      sink_put(sink, conv);
      break;
    }
  }
  return (int)sink->written;
}

//===----------------------------------------------------------------------===//
// The public entry points
//===----------------------------------------------------------------------===//

int vprintf(const char *fmt, va_list ap) {
  Sink sink = {0, 0, 0};
  return format(&sink, fmt, ap);
}

int printf(const char *fmt, ...) {
  va_list ap;
  int count;
  va_start(ap, fmt);
  count = vprintf(fmt, ap);
  va_end(ap);
  return count;
}

int vsnprintf(char *out, unsigned size, const char *fmt, va_list ap) {
  Sink sink = {out, size, 0};
  int count = format(&sink, fmt, ap);
  if (size)
    out[(sink.written < size) ? sink.written : size - 1] = '\0';
  return count;
}

int snprintf(char *out, unsigned size, const char *fmt, ...) {
  va_list ap;
  int count;
  va_start(ap, fmt);
  count = vsnprintf(out, size, fmt, ap);
  va_end(ap);
  return count;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  int count;
  va_start(ap, fmt);
  count = vsnprintf(out, 0xFFFFU, fmt, ap);
  va_end(ap);
  return count;
}

int putchar(int c) {
  Sink sink = {0, 0, 0};
  sink_put(&sink, (char)c);
  return c;
}

int puts(const char *s) {
  Sink sink = {0, 0, 0};
  while (*s)
    sink_put(&sink, *s++);
  sink_put(&sink, '\n');
  return (int)sink.written;
}

//===----------------------------------------------------------------------===//
// Stream-shaped wrappers.  Every stream is the console.
//===----------------------------------------------------------------------===//

static int console_marker;
FILE *const stdout = (FILE *)&console_marker;
FILE *const stderr = (FILE *)&console_marker;
FILE *const stdin  = (FILE *)0;

int fputc(int c, FILE *stream) { (void)stream; return putchar(c); }

int fputs(const char *s, FILE *stream) {
  Sink sink = {0, 0, 0};
  (void)stream;
  while (*s)
    sink_put(&sink, *s++);
  return (int)sink.written;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
  (void)stream;
  return vprintf(fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...) {
  va_list ap;
  int count;
  (void)stream;
  va_start(ap, fmt);
  count = vprintf(fmt, ap);
  va_end(ap);
  return count;
}

int fflush(FILE *stream) { (void)stream; return 0; }
