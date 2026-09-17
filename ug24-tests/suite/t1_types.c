#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

struct Packet { uint8_t type; uint16_t length; uint32_t value; };
union Value { uint16_t u16; uint32_t u32; };
struct Flags { uint8_t ready : 1; uint8_t error : 1; uint8_t mode : 2; uint8_t unused : 4; };

static uint16_t add16(uint16_t a, uint16_t b) { return a + b; }
static uint32_t add32(uint32_t a, uint32_t b) { return a + b; }
static int signed_test(int a, int b) { return a < b; }
static void pointer_test(uint16_t *p) { p[0] = 0x1111; p[1] = 0x2222; p[2] = 0x3333; }

int main(void)
{
    uint8_t u8 = 0xAA; uint16_t u16 = 0xBEEF; uint32_t u32 = 0xDEADBEEF;
    uint16_t array[3] = {0, 0, 0};
    struct Packet p = { 0x12, 0x3456, 0x789ABCDE };
    union Value v;
    struct Flags f = {1, 0, 2, 0};
    v.u32 = 0x12345678;
    pointer_test(array);

    printf("sizes: %u %u %u %u\n", (unsigned)sizeof(uint8_t), (unsigned)sizeof(uint16_t),
           (unsigned)sizeof(uint32_t), (unsigned)sizeof(void *));
    printf("values: %02X %04X %08lX\n", (unsigned)u8, (unsigned)u16, (unsigned long)u32);
    printf("limits: %d %d %u %lu\n", INT_MIN, INT_MAX, (unsigned)UINT_MAX, (unsigned long)UINT32_MAX);
    printf("add: %04X %08lX\n", (unsigned)add16(0x1000, 0x2345),
           (unsigned long)add32(0x10000000, 0x23456789));
    printf("signed: %d\n", signed_test(-10, 5));
    printf("array: %04X %04X %04X\n", array[0], array[1], array[2]);
    printf("struct: %02X %04X %08lX\n", p.type, p.length, (unsigned long)p.value);
    printf("union: %08lX\n", (unsigned long)v.u32);
    printf("bits: %u %u %u\n", f.ready, f.error, f.mode);
    printf("offset: %u %u %u\n", (unsigned)offsetof(struct Packet, type),
           (unsigned)offsetof(struct Packet, length), (unsigned)offsetof(struct Packet, value));
    return 0;
}
