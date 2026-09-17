//===-- ug24_stdlib.c - Allocator and utilities for the uG24 --------------===//
//
// A first-fit free list over the linker-defined heap window.  Blocks carry a
// six-byte header and are coalesced with their successor on free.  There is no
// guard against the heap meeting the stack: on a part with a 64 KB flat map
// and no MMU there is nothing useful to do about it at run time, so the
// linker script's __stack_size is the contract.
//
//===----------------------------------------------------------------------===//

#include <stdlib.h>

typedef unsigned char u8;
typedef unsigned short u16;

#define SIM_EXIT (*(volatile unsigned char *)0xFF02)

extern char __heap_start;
extern char __heap_end;

typedef struct Block {
    struct Block *next;   ///< Next block in address order, or NULL.
    u16 size;             ///< Payload bytes, not counting this header.
    u16 used;             ///< Non-zero while the payload is handed out.
} Block;

#define HEADER ((u16)sizeof(Block))

static Block *heap_list;

static void heap_init(void)
{
    char *start = &__heap_start;
    char *end   = &__heap_end;

    if ((u16)(end - start) <= HEADER)
        return;                       // No usable heap; malloc will return 0.

    heap_list = (Block *)start;
    heap_list->next = 0;
    heap_list->size = (u16)((u16)(end - start) - HEADER);
    heap_list->used = 0;
}

/// Split \p b so that it holds exactly \p want bytes, if the tail is big
/// enough to be worth a header of its own.
static void split(Block *b, u16 want)
{
    u16 tail = (u16)(b->size - want);

    if (tail < HEADER + 2)
        return;

    Block *rest = (Block *)((char *)b + HEADER + want);
    rest->size = (u16)(tail - HEADER);
    rest->used = 0;
    rest->next = b->next;

    b->size = want;
    b->next = rest;
}

void *malloc(unsigned size)
{
    if (size == 0)
        return 0;
    if (!heap_list)
        heap_init();

    u16 want = (u16)((size + 1) & ~1u);   // Keep every payload even-sized.

    for (Block *b = heap_list; b; b = b->next) {
        if (b->used || b->size < want)
            continue;
        split(b, want);
        b->used = 1;
        return (char *)b + HEADER;
    }
    return 0;
}

void free(void *ptr)
{
    if (!ptr)
        return;

    Block *b = (Block *)((char *)ptr - HEADER);
    b->used = 0;

    // Merge forward as far as the run of free blocks goes.  Merging backwards
    // would need a back pointer; the forward walk in malloc finds those.
    while (b->next && !b->next->used) {
        b->size = (u16)(b->size + HEADER + b->next->size);
        b->next = b->next->next;
    }
}

void *calloc(unsigned count, unsigned size)
{
    unsigned total = count * size;
    char *p = (char *)malloc(total);

    if (p)
        for (unsigned i = 0; i < total; i++)
            p[i] = 0;
    return p;
}

void *realloc(void *ptr, unsigned size)
{
    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return 0;
    }

    Block *b = (Block *)((char *)ptr - HEADER);
    u16 want = (u16)((size + 1) & ~1u);

    if (b->size >= want) {
        split(b, want);
        return ptr;
    }

    // Grow in place when the next block is free and the two together fit.
    if (b->next && !b->next->used &&
        (u16)(b->size + HEADER + b->next->size) >= want) {
        b->size = (u16)(b->size + HEADER + b->next->size);
        b->next = b->next->next;
        split(b, want);
        return ptr;
    }

    char *fresh = (char *)malloc(size);
    if (!fresh)
        return 0;
    for (u16 i = 0; i < b->size; i++)
        fresh[i] = ((char *)ptr)[i];
    free(ptr);
    return fresh;
}

unsigned ug24_heap_largest_free(void)
{
    u16 best = 0;

    if (!heap_list)
        heap_init();
    for (Block *b = heap_list; b; b = b->next)
        if (!b->used && b->size > best)
            best = b->size;
    return best;
}

//===----------------------------------------------------------------------===//
// Odds and ends
//===----------------------------------------------------------------------===//

void exit(int status)
{
    SIM_EXIT = (u8)status;
    for (;;)
        ;
}

void abort(void)
{
    exit(1);
}

int abs(int value) { return value < 0 ? -value : value; }
long labs(long value) { return value < 0 ? -value : value; }

long atol(const char *s)
{
    long value = 0;
    int negative = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    if (*s == '-') { negative = 1; s++; }
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9')
        value = value * 10 + (*s++ - '0');

    return negative ? -value : value;
}

int atoi(const char *s) { return (int)atol(s); }

static unsigned long rand_state = 1;

int rand(void)
{
    rand_state = rand_state * 1103515245UL + 12345UL;
    return (int)((rand_state >> 16) & 0x7FFF);
}

void srand(unsigned seed) { rand_state = seed; }
