#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

enum State { STATE_IDLE = 0, STATE_RUN = 1, STATE_ERROR = 2 };
struct Device { uint16_t id; uint8_t state; uint8_t flags; };
typedef uint32_t (*operation_t)(uint32_t);

static uint32_t square(uint32_t x) { return x * x; }
static uint32_t cube(uint32_t x) { return x * x * x; }
static uint32_t apply(operation_t fn, uint32_t value) { return fn(value); }
static uint16_t static_counter(void) { static uint16_t counter = 0; return ++counter; }

int main(void)
{
    const uint16_t constant = 0x1234;
    struct Device dev = { 0x55AA, STATE_RUN, 0 };
    bool enabled = true;
    uint16_t c1, c2, c3;

    dev.flags |= (1u << 2);
    dev.flags ^= (1u << 1);

    printf("const=%04X\n", constant);
    printf("device=%04X %u %02X\n", dev.id, dev.state, dev.flags);
    printf("bool=%d\n", enabled);
    printf("square=%lu\n", (unsigned long)apply(square, 12));
    printf("cube=%lu\n", (unsigned long)apply(cube, 5));
    c1 = static_counter(); c2 = static_counter(); c3 = static_counter();
    printf("static=%u %u %u\n", c1, c2, c3);
    return 0;
}
