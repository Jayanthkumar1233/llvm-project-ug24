//===-- stdio.h - Console I/O for the uG24 --------------------------------===//
//
// A freestanding stdio.  There is no operating system and no file system on
// this target, so there are no files: every stream is the memory-mapped UART
// that ug24.ld reserves at 0xFF00.
//
// Supported conversions:
//     %d %i %u %x %X %o %c %s %p %%
//     flags   - 0 + space #
//     width   a number or *
//     length  hh h l z j t   (hh and h are absorbed by the default argument
//                             promotions; l and the rest select 32 bits)
//
// Not supported, and deliberately so:
//     %f %e %g %a   there is no FPU and no soft-float library; these print
//                   "<fp?>" rather than silently producing wrong digits
//     %lld          64-bit arithmetic is not worth its cost on an 8-bit ALU
//     scanf         no input device is defined
//
//===----------------------------------------------------------------------===//

#ifndef _UG24_STDIO_H
#define _UG24_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// There is only one device, so FILE carries no state.  The stream arguments
/// exist for source compatibility and are ignored.
typedef struct _UG24_FILE FILE;

extern FILE *const stdout;
extern FILE *const stderr;
extern FILE *const stdin;   ///< Always NULL: there is no input device.

#define EOF (-1)

int printf(const char *__format, ...);
int vprintf(const char *__format, va_list __args);

int sprintf(char *__out, const char *__format, ...);
int snprintf(char *__out, unsigned __size, const char *__format, ...);
int vsnprintf(char *__out, unsigned __size, const char *__format,
              va_list __args);

int fprintf(FILE *__stream, const char *__format, ...);
int vfprintf(FILE *__stream, const char *__format, va_list __args);

int putchar(int __c);
int puts(const char *__s);
int fputc(int __c, FILE *__stream);
int fputs(const char *__s, FILE *__stream);
int fflush(FILE *__stream);

#ifdef __cplusplus
}
#endif

#endif // _UG24_STDIO_H
