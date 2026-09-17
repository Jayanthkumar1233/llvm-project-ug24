//===-- ug24.h - Minimal runtime interface for the uG24 -------------------===//
//
// There is no operating system on this target, so output goes to a
// memory-mapped UART.  The addresses below must agree with the MMIO window in
// ug24-runtime/ug24.ld and with ug24-sim/ug24sim.c.
//
//===----------------------------------------------------------------------===//

#ifndef UG24_H
#define UG24_H

typedef unsigned char  ug24_u8;
typedef signed char    ug24_s8;
typedef unsigned short ug24_u16;
typedef signed short   ug24_s16;

//===----------------------------------------------------------------------===//
// Peripheral registers
//===----------------------------------------------------------------------===//

#define UG24_UART_TX     (*(volatile unsigned char *)0xFF00)
#define UG24_UART_STATUS (*(volatile unsigned char *)0xFF01)
#define UG24_SIM_EXIT    (*(volatile unsigned char *)0xFF02)

/// Set when the transmitter can accept another byte.
#define UG24_UART_READY  0x01

//===----------------------------------------------------------------------===//
// Console output
//===----------------------------------------------------------------------===//

/// Write one character to the console.
void ug24_putchar(char c);

/// Write a NUL-terminated string.
void ug24_puts(const char *s);

/// Write a string followed by a newline.
void ug24_println(const char *s);

/// Write \p value in decimal.
void ug24_put_u16(unsigned short value);

/// Write \p value in decimal, with a leading '-' when negative.
void ug24_put_s16(short value);

/// Write \p value as two hexadecimal digits.
void ug24_put_hex8(unsigned char value);

/// Write \p value as four hexadecimal digits.
void ug24_put_hex16(unsigned short value);

/// Stop the core.  Under the simulator this becomes the process exit status.
void ug24_exit(unsigned char status);

#endif // UG24_H
