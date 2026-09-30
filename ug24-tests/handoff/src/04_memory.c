/* 04_memory - initialised data, the heap and the string routines.
 *
 * The interesting part is the first line.  `banner` and `table` live in .data,
 * which means their contents are in the image at a load address and crt0 copies
 * them to their run address before main starts.  A simulator that loads by
 * section name, or that ignores p_paddr, usually prints garbage here while
 * everything else in this kit still works.
 *
 * malloc comes out of the window between __heap_start and __stack_top, both of
 * which are symbols in the image.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char banner[] = "initialised in .data";
static const unsigned short table[5] = { 11, 22, 33, 44, 55 };
static unsigned short zeroed[4];          /* .bss: must start at zero */

struct Device {
    unsigned short id;
    unsigned char  state;
    unsigned char  flags;
};

int main(void)
{
    printf("uG24 sample 4: memory\n");

    printf("  .data string     = %s\n", banner);
    printf("  .data length     = %u\n", (unsigned)strlen(banner));

    unsigned total = 0;
    for (int i = 0; i < 5; i++) total += table[i];
    printf("  .data table sum  = %u   (11+22+33+44+55)\n", total);

    unsigned any = 0;
    for (int i = 0; i < 4; i++) any |= zeroed[i];
    printf("  .bss all zero    = %s\n", any ? "no" : "yes");

    struct Device dev = { 0x55AA, 1, 0 };
    dev.flags |= 1u << 2;
    dev.flags ^= 1u << 1;
    printf("  struct           = %04X %u %02X\n", dev.id, dev.state, dev.flags);

    char buf[32];
    strcpy(buf, "abc");
    strcat(buf, "def");
    printf("  strcpy/strcat    = %s (len %u)\n", buf, (unsigned)strlen(buf));
    printf("  strcmp abc,abd   = %s\n", strcmp("abc", "abd") < 0 ? "less" : "not less");

    char dst[8];
    memset(dst, '-', sizeof dst);
    memcpy(dst, "XY", 2);
    dst[7] = '\0';
    printf("  memset/memcpy    = %s\n", dst);

    char *heap = malloc(16);
    if (!heap) { printf("  malloc           = failed\n"); return 1; }
    strcpy(heap, "from the heap");
    printf("  malloc           = %s\n", heap);
    free(heap);

    return 0;
}
