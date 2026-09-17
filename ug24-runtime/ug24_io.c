//===-- ug24_io.c - Console output for the uG24 ---------------------------===//
//
// Everything here goes through the memory-mapped UART described in ug24.h.
// Division is avoided in the decimal conversion because an unsigned 16-bit
// divide is a call into the software helpers; repeated subtraction against a
// table of powers of ten is smaller and faster on this core.
//
//===----------------------------------------------------------------------===//

#include "include/ug24.h"

void ug24_putchar(char c) {
    while ((UG24_UART_STATUS & UG24_UART_READY) == 0)
        ;
    UG24_UART_TX = (unsigned char)c;
}

void ug24_puts(const char *s) {
    while (*s)
        ug24_putchar(*s++);
}

void ug24_println(const char *s) {
    ug24_puts(s);
    ug24_putchar('\n');
}

void ug24_put_u16(unsigned short value) {
    static const unsigned short powers[5] = {10000, 1000, 100, 10, 1};
    ug24_u8 i, started = 0;

    for (i = 0; i < 5; i++) {
        unsigned short p = powers[i];
        ug24_u8 digit = 0;
        while (value >= p) {
            value = (unsigned short)(value - p);
            digit++;
        }
        if (digit || started || i == 4) {
            ug24_putchar((char)('0' + digit));
            started = 1;
        }
    }
}

void ug24_put_s16(short value) {
    if (value < 0) {
        ug24_putchar('-');
        value = (short)-value;
    }
    ug24_put_u16((unsigned short)value);
}

void ug24_put_hex8(unsigned char value) {
    static const char digits[] = "0123456789abcdef";
    ug24_putchar(digits[(value >> 4) & 0x0f]);
    ug24_putchar(digits[value & 0x0f]);
}

void ug24_put_hex16(unsigned short value) {
    ug24_put_hex8((unsigned char)(value >> 8));
    ug24_put_hex8((unsigned char)value);
}

void ug24_exit(unsigned char status) {
    UG24_SIM_EXIT = status;
    for (;;)
        ;
}
