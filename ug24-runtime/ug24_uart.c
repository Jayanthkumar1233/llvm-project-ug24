//===-- ug24_uart.c - The one place a byte reaches the console ------------===//
//
// Both <stdio.h> and the ug24_put* helpers come through here, so that there is
// a single answer to "what does this do when the transmitter is not ready?".
//
// The transmit handshake is an assumption.  The uG24 core contains no UART, so
// neither the specification nor the encoding spreadsheet says that a status
// register exists, where it is, or which bit means ready; UART_STATUS at
// 0xFF01 bit 0 is this project's invention, pending specification query G5.
// A loader that does not model it reads zero.
//
// An unbounded `while (!ready);` therefore turns an unimplemented register into
// a program that runs for ever and prints nothing, which is the worst failure
// mode available: it looks exactly like a miscompiled program, and the first
// thing anyone suspects is the compiler.  A separately written simulator hit
// precisely this and spent its debugging effort patching the ELF.
//
// So the handshake is settled once, on the first byte, when the transmitter is
// idle and the answer is least ambiguous:
//
//   * ready within the bound  -> flow control works; poll normally from now on,
//                                without a bound, which is what hardware needs.
//   * never ready             -> nothing implements the register; stop asking
//                                and write the byte.  Output at worst garbles,
//                                where before it did not exist.
//
// Hardware that implements the register answers on the first or second read, so
// the bound costs nothing there.
//
//===----------------------------------------------------------------------===//

#include "include/ug24.h"

/// How many times to read UART_STATUS before concluding that nothing answers.
/// Generous for any implementation that models the register at all, and about
/// 25,000 instructions when nothing does -- paid once per program.
#define UART_PATIENCE 4096

enum { FlowUnknown = 0, FlowPolled = 1, FlowAbsent = 2 };

static unsigned char uart_flow = FlowUnknown;

void __ug24_uart_put(unsigned char byte) {
    if (uart_flow != FlowAbsent) {
        unsigned short spins = 0;

        while ((UG24_UART_STATUS & UG24_UART_READY) == 0) {
            if (uart_flow == FlowPolled)
                continue;               // established device: wait as long as
                                        // it takes, which is what hardware
                                        // flow control means
            if (++spins == UART_PATIENCE) {
                uart_flow = FlowAbsent;
                break;
            }
        }

        if (uart_flow == FlowUnknown)
            uart_flow = FlowPolled;
    }

    UG24_UART_TX = byte;
}
