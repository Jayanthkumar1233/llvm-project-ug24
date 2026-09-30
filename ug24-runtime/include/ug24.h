//===-- ug24.h - Minimal runtime interface for the uG24 -------------------===//
//
// There is no operating system on this target, so output goes to a
// memory-mapped UART.
//
// Every register below is a symbol the linker script defines, not a literal
// address: ug24-runtime/ug24.ld owns the peripheral map, publishes it in the
// ELF symbol table, and this header, the simulator and any other loader all
// read it from there.  Point the script's MMIO region at a different page and
// the runtime follows, with no source change and nothing to agree out of band.
// The reasoning, and what a third-party simulator has to do to honour it, are
// in docs/uG24-platform.md.
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

extern volatile unsigned char __ug24_uart_tx;
extern volatile unsigned char __ug24_uart_status;
extern volatile unsigned char __ug24_sim_exit;

#define UG24_UART_TX     __ug24_uart_tx
#define UG24_UART_STATUS __ug24_uart_status
#define UG24_SIM_EXIT    __ug24_sim_exit

/// Set when the transmitter can accept another byte.
#define UG24_UART_READY  0x01

//===----------------------------------------------------------------------===//
// Interrupt controller
//===----------------------------------------------------------------------===//
//
// Everything in this section rests on the interrupt model described under
// "Interrupts" in docs/uG24-assumptions.md: the specification to hand names
// the PSW enable bits but not the vector table or the controller registers.
//
// Writing a handler:
//
//     __attribute__((interrupt)) void __ug24_irq(void) {
//         ticks++;
//         UG24_IRQ_STATUS = UG24_IRQ_TIMER;   // clear the source
//     }
//
// The name is what wires it to a vector: crt0's table jumps to the weak
// symbols __ug24_nmi, __ug24_irq and __ug24_swi, and a definition here
// replaces the default handler.  The attribute is what makes the function
// preserve every register it touches and return through PSW and PC.

extern volatile unsigned char __ug24_irq_status;
extern volatile unsigned char __ug24_irq_enable;
extern volatile unsigned char __ug24_irq_raise;
extern volatile unsigned char __ug24_timer_load;

#define UG24_IRQ_STATUS  __ug24_irq_status
#define UG24_IRQ_ENABLE  __ug24_irq_enable
#define UG24_IRQ_RAISE   __ug24_irq_raise
#define UG24_TIMER_LOAD  __ug24_timer_load

/// Interrupt sources, as bits in UG24_IRQ_STATUS and UG24_IRQ_ENABLE.
#define UG24_IRQ_TIMER   0x01
#define UG24_IRQ_SW      0x02

/// Let maskable interrupts through: PSW.IE and PSW.ME both set.  There is no
/// SETF instruction, so each bit is cleared and then inverted.
static inline void ug24_enable_interrupts(void) {
    __asm__ __volatile__("clrf 15\n\tinvf 15\n\tclrf 14\n\tinvf 14"
                         ::: "memory");
}

/// Block maskable interrupts by clearing PSW.IE.
static inline void ug24_disable_interrupts(void) {
    __asm__ __volatile__("clrf 15" ::: "memory");
}

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
