/* Linker script for bare-metal uG24 programs.
 *
 * The memory map is the one confirmed by the hardware team: 32 KB of ROM from
 * 0x0000 holding the vector table, code and read-only data, and 32 KB of RAM
 * from 0x8000 holding .data, .bss, the heap and the stack.
 *
 * Because the two are now distinct regions, .data has a load address in ROM
 * and a run address in RAM, and crt0's copy loop does real work -- it was a
 * no-op while the map was one flat region.  A simulator that loads by section
 * name rather than by p_paddr prints garbage from an initialised variable and
 * nothing else wrong, which is why the cross-check kit has a program for it.
 *
 * The one part of the map the hardware team has not confirmed is where the SoC
 * decodes its peripherals: specification query G5.  The confirmed answer puts
 * RAM up to 0xFFFF with the stack top there, which leaves the console nowhere
 * to live, so the top page is held out of RAM as before and the stack starts
 * below it.  Every peripheral address is published as a symbol (see the
 * assignments at the end of this file and docs/uG24-platform.md), so when G5 is
 * answered only this file changes.
 */

OUTPUT_FORMAT("elf32-ug24")
OUTPUT_ARCH(ug24)
ENTRY(_start)

MEMORY
{
  ROM  (rx)  : ORIGIN = 0x0000, LENGTH = 0x8000   /* 32 KB, confirmed */
  RAM  (rw)  : ORIGIN = 0x8000, LENGTH = 0x7F00   /* 32 KB less the top page */
  MMIO (rw)  : ORIGIN = 0xFF00, LENGTH = 0x0100   /* assumed; query G5 */
}

/* PROVIDE, not a conditional assignment: "DEFINED(x) ? x : default" is
 * self-referential here and lld resolves it to zero, which silently put
 * __heap_end at __stack_top and let malloc hand out the stack's memory.
 * Override it on the link line with -Wl,--defsym=__stack_size=N. */
PROVIDE(__stack_size = 0x400);

SECTIONS
{
  .text ORIGIN(ROM) :
  {
    /* KEEP: nothing references the vector table, so --gc-sections would
     * otherwise drop the entry point along with every interrupt handler. */
    KEEP(*(.text.vectors))
    KEEP(*(.text.default_isr))
    *(.text.start)
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(2);
  } > ROM

  /* AT> ROM is what makes crt0's copy loop necessary: the initial contents
   * travel in the image next to .text, and the variables themselves live in
   * RAM.  __data_load_start is where to copy from, __data_start where to. */
  .data : ALIGN(2)
  {
    __data_start = .;
    *(.data .data.*)
    . = ALIGN(2);
    __data_end = .;
  } > RAM AT> ROM
  __data_load_start = LOADADDR(.data);

  .bss : ALIGN(2)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(2);
    __bss_end = .;
  } > RAM

  /* The heap fills whatever is left between .bss and the stack.  malloc()
   * lives entirely inside this window; nothing checks it against the stack
   * pointer at run time, so keep __stack_size honest for the program. */
  . = ALIGN(2);
  __heap_start = .;

  /* The stack grows downwards from the top of RAM, below the peripherals. */
  __stack_top = ORIGIN(RAM) + LENGTH(RAM) - 2;
  __heap_end  = __stack_top - __stack_size;

  /* The peripheral map, published in the symbol table so that a loader or a
   * simulator never has to guess where the console is.  The uG24 core has no
   * peripherals of its own: neither the ISA specification nor the encoding
   * spreadsheet says where a UART is decoded, so an independently written
   * simulator cannot derive these addresses from those documents.  Reading
   * them out of the ELF turns that guess into a fact -- and keeps working
   * when an SoC decodes the page somewhere other than 0xFF00, because
   * everything below is an offset from the MMIO region.
   *
   * See docs/uG24-platform.md for the contract and a loader that reads it. */
  __mmio_base = ORIGIN(MMIO);
  __mmio_end  = ORIGIN(MMIO) + LENGTH(MMIO) - 1;

  __ug24_uart_tx     = ORIGIN(MMIO) + 0x00;
  __ug24_uart_status = ORIGIN(MMIO) + 0x01;
  __ug24_sim_exit    = ORIGIN(MMIO) + 0x02;

  __ug24_irq_status  = ORIGIN(MMIO) + 0x10;
  __ug24_irq_enable  = ORIGIN(MMIO) + 0x11;
  __ug24_irq_raise   = ORIGIN(MMIO) + 0x12;
  __ug24_timer_load  = ORIGIN(MMIO) + 0x13;

  /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) }
}
