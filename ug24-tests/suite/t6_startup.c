#include <stdio.h>
#include <stdint.h>

uint16_t initialized = 0x1234;
uint16_t initialized2 = 0xABCD;
uint16_t zero1;
uint32_t zero2;
static uint16_t static_initialized = 0x5678;
static uint16_t static_zero;

static uint32_t recursive_sum(uint16_t n)
{
    if (n == 0) return 0;
    return n + recursive_sum(n - 1);
}

int main(void)
{
    uint16_t local = 0xBEEF;
    printf("data=%04X %04X\n", initialized, static_initialized);
    printf("bss=%u %lu\n", zero1, (unsigned long)zero2);
    printf("static bss=%u\n", static_zero);
    printf("local=%04X\n", local);
    printf("recursive=%lu\n", (unsigned long)recursive_sum(20));
    initialized++;
    zero1 = 100;
    printf("modified=%04X %u\n", initialized, zero1);
    return 0;
}
