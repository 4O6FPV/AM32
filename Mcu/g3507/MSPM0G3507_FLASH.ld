/*
 * Linker script for AM32 on the TI MSPM0G3507 (128 KB flash, 32 KB SRAM).
 *
 * Flash map (MSPM0 main flash starts at 0x00000000, 1 KB sectors):
 *   0x00000000 - 0x00000FFF  AM32 bootloader (4 KB, last 32 bytes = devinfo)
 *   0x00001000 - 0x000010BF  application vector table (16 + 32 entries)
 *   0x000010C0 - 0x0001F7DF  application code / rodata / data init
 *   0x0001F7E0 - 0x0001F7FF  FILE_NAME (32 bytes, read by the configurator)
 *   0x0001F800 - 0x0001FFFF  EEPROM emulation (2 sectors, settings use one)
 *
 * NONMAIN (BCR/BSL configuration at 0x41C00000) is deliberately NOT placed
 * here: writing it wrongly can lock the part, and objcopy -O binary would
 * otherwise produce a ~1 GB .bin spanning the gap.
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size = 0x000;
_Min_Stack_Size = 0x400;

MEMORY
{
  RAM           (xrw) : ORIGIN = 0x20200000, LENGTH = 32K
  FLASH_VECTAB  (rx)  : ORIGIN = 0x00001000, LENGTH = 192
  FLASH         (rx)  : ORIGIN = ORIGIN(FLASH_VECTAB) + LENGTH(FLASH_VECTAB), LENGTH = 0x1F800 - 0x1000 - 32 - LENGTH(FLASH_VECTAB)
  EEPROM        (rx)  : ORIGIN = 0x0001F800, LENGTH = 2K
  FILE_NAME     (rx)  : ORIGIN = ORIGIN(EEPROM) - 32, LENGTH = 32
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH_VECTAB

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .file_name :
  {
    . = ALIGN(4);
    KEEP (*(.file_name))
    . = ALIGN(4);
  } >FILE_NAME

  /* initialised data, plus .ramfunc code copied to SRAM by the startup */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    *(.ramfunc)
    *(.ramfunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
