//===-- stdlib.h - General utilities for the uG24 -------------------------===//
//
// The allocator here is a first-fit free list over the region the linker
// script calls __heap_start .. __heap_end -- whatever is left between .bss and
// the stack.  It is small and it coalesces on free, which is the right trade
// for a part with 64 KB of address space; it is not a general-purpose
// allocator and it does not check the heap against the stack pointer.
//
// Firmware that cannot tolerate fragmentation should keep using static
// buffers.  malloc() is here so that ordinary C compiles, not as a
// recommendation.
//
//===----------------------------------------------------------------------===//

#ifndef _UG24_STDLIB_H
#define _UG24_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 32767

void *malloc(unsigned __size);
void *calloc(unsigned __count, unsigned __size);
void *realloc(void *__ptr, unsigned __size);
void  free(void *__ptr);

/// Bytes still available in the largest free run.  Not standard C; useful when
/// deciding whether the allocator is worth keeping in a given program.
unsigned ug24_heap_largest_free(void);

void exit(int __status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

int  abs(int __value);
long labs(long __value);
int  atoi(const char *__s);
long atol(const char *__s);

int  rand(void);
void srand(unsigned __seed);

#ifdef __cplusplus
}
#endif

#endif // _UG24_STDLIB_H
