;===-- ug24_platform.s - Fallback peripheral map -------------------------===;
;
; ug24-runtime/ug24.ld owns the peripheral map and publishes it as absolute
; symbols, so the runtime, the simulator and any other loader all read the same
; addresses out of the image rather than agreeing them out of band.  See
; docs/uG24-platform.md.
;
; A program linked with its own script (-T) gets no such assignments, and the
; references from <ug24.h> would otherwise fail to link.  The weak definitions
; below are the fallback for exactly that case: a linker-script assignment
; overrides them, so with the stock script these are never used and this member
; is not even extracted from the archive.  A custom script that moves the MMIO
; window should copy the assignment block out of ug24.ld; leaving it out means
; falling back to the addresses here, which is what the toolchain did before
; the map became a symbol.
;
;===----------------------------------------------------------------------===;

	.weak	__mmio_base
	.set	__mmio_base,        0xFF00
	.weak	__mmio_end
	.set	__mmio_end,         0xFFFF

	.weak	__ug24_uart_tx
	.set	__ug24_uart_tx,     0xFF00
	.weak	__ug24_uart_status
	.set	__ug24_uart_status, 0xFF01
	.weak	__ug24_sim_exit
	.set	__ug24_sim_exit,    0xFF02

	.weak	__ug24_irq_status
	.set	__ug24_irq_status,  0xFF10
	.weak	__ug24_irq_enable
	.set	__ug24_irq_enable,  0xFF11
	.weak	__ug24_irq_raise
	.set	__ug24_irq_raise,   0xFF12
	.weak	__ug24_timer_load
	.set	__ug24_timer_load,  0xFF13
