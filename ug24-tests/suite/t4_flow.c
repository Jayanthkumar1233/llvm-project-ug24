#include <stdio.h>
#include <stdint.h>

static uint32_t calculate(uint32_t n)
{
    uint32_t x = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (i & 1) x += i * 3;
        else       x += i * 7;
    }
    return x;
}

static uint16_t switch_test(uint16_t x)
{
    switch (x) {
    case 0:  return 100;
    case 1:  return 200;
    case 2:  return 300;
    case 10: return 1000;
    default: return 0xFFFF;
    }
}

static uint32_t divide_test(uint32_t x)
{
    uint32_t a = x / 7;
    uint32_t b = x % 7;
    return a * 100 + b;
}

int main(void)
{
    printf("calc=%lu\n", (unsigned long)calculate(100));
    printf("switch=%u %u %u\n", switch_test(0), switch_test(2), switch_test(99));
    printf("div=%lu\n", (unsigned long)divide_test(123456));
    return 0;
}
