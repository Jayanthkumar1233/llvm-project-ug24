/* Linker script for bare-metal uG24 programs.
 *
 * The uG24 has a single 16-bit address space shared by instruction and data
 * fetches, so this script lays out one flat 64 KB image: code from address 0,
 * then read-only data, then initialised data and .bss, with the stack growing
 * down from the top of RAM.
 *
 * The top page (0xFF00-0xFFFF) is left out of the RAM region for
 * memory-mapped peripherals - see ug24-runtime/include/ug24.h.  Adjust both
 * files together if the SoC decodes its peripherals elsewhere.
 *
 * The split between the instruction and data TCMs is a per-SoC configuration
 * parameter (ITCM_SIZE / DTCM_SIZE in the hardware spec), so adjust the
 * MEMORY block below to match the part being targeted.
 */

OUTPUT_FORMAT("elf32-ug24")
OUTPUT_ARCH(ug24)
ENTRY(_start)

MEMORY
{
  MEM  (rwx) : ORIGIN = 0x0000, LENGTH = 0xFF00
  MMIO (rw)  : ORIGIN = 0xFF00, LENGTH = 0x0100
}

__stack_size = DEFINED(__stack_size) ? __stack_size : 0x400;

SECTIONS
{
  .text 0x0000 :
  {
    *(.text.start)
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(2);
  } > MEM

  .data : ALIGN(2)
  {
    __data_start = .;
    *(.data .data.*)
    . = ALIGN(2);
    __data_end = .;
  } > MEM

  .bss : ALIGN(2)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(2);
    __bss_end = .;
  } > MEM

  /* The heap fills whatever is left between .bss and the stack.  malloc()
   * lives entirely inside this window; nothing checks it against the stack
   * pointer at run time, so keep __stack_size honest for the program. */
  . = ALIGN(2);
  __heap_start = .;

  /* The stack grows downwards from the top of RAM, below the peripherals. */
  __stack_top = ORIGIN(MEM) + LENGTH(MEM) - 2;
  __heap_end  = __stack_top - __stack_size;

  __mmio_base = ORIGIN(MMIO);

  /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) }
}
