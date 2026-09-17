#include <stdio.h>
#include <stdint.h>

static volatile uint8_t REG_STATUS;
static volatile uint8_t REG_DATA;

static void write_reg(uint8_t value) { REG_DATA = value; }
static uint8_t read_reg(void) { return REG_DATA; }
static void set_ready(void) { REG_STATUS |= 1; }
static int wait_ready(void) { if (REG_STATUS & 1) return 1; return 0; }

int main(void)
{
    REG_STATUS = 0;
    write_reg(0x55);
    printf("data=%02X\n", (unsigned)read_reg());
    set_ready();
    printf("ready=%d\n", wait_ready());
    REG_DATA = 0xAA;
    printf("data=%02X\n", (unsigned)REG_DATA);
    return 0;
}
