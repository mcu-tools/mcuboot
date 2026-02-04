/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Simplified memory map for the bootloader.
 *
 * The main purpose is to make sure the bootloader can load into main memory
 * without overwriting itself.
 */

#include "memory.h"

_bootloader_iram_loader_seg_start = BOOTLOADER_IRAM_LOADER_SEG_START;
_bootloader_dram_loader_seg_start = BOOTLOADER_DRAM_LOADER_SEG_START;

MEMORY
{
  iram_loader_seg (RWX) :           org = BOOTLOADER_IRAM_LOADER_SEG_START, len = BOOTLOADER_IRAM_LOADER_SEG_LEN
  dram_loader_seg (RW) :            org = BOOTLOADER_DRAM_LOADER_SEG_START, len = BOOTLOADER_DRAM_LOADER_SEG_LEN
  iram_seg (RWX) :                  org = BOOTLOADER_IRAM_SEG_START, len = BOOTLOADER_IRAM_SEG_LEN
  dram_seg (RW) :                   org = BOOTLOADER_DRAM_SEG_START, len = BOOTLOADER_DRAM_SEG_LEN
}

/*  Default entry point:  */
ENTRY(main);

SECTIONS
{
  .iram_loader.text :
  {
    . = ALIGN (16);
    _loader_text_start = ABSOLUTE(.);
    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *esp_mcuboot.*(.literal .text .literal.* .text.*)
    *esp_loader.*(.literal .text .literal.* .text.*)
    *main.*(.literal .text .literal.* .text.*)

    /* iram_loader section must not be overlapped by application IRAM/DRAM
     * mapping, therefore the following were added based on the dependencies
     * from esp_loader as its from where the application RAM parts will be
     * loaded into memory and ultimately boot. The cross reference table
     * were used to map the reachable dependencies
     */
    *libhal.a:bootloader_flash.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:bootloader_sha.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:bootloader_wdt.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:bootloader_panic.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:cache_hal.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:mmu_hal.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *libhal.a:wdt_hal_iram.*(.literal .text .literal.* .text.* .iram0 .iram0.* .iram1 .iram1.*)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _loader_text_end = ABSOLUTE(.);
  } > iram_loader_seg

  /* Ensure the iram_loader_seg is large enough */
  ASSERT((_loader_text_end - ORIGIN(iram_loader_seg)) <= BOOTLOADER_IRAM_LOADER_SEG_LEN,
    "iram_loader_seg overflow: increase BOOTLOADER_IRAM_LOADER_SEG_LEN")

  .dram_loader.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _dram_loader_start = ABSOLUTE(.);
    _loader_bss_start = ABSOLUTE(.);
    *esp_loader.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *esp_mcuboot.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *main.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:bootloader_flash.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:bootloader_sha.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:bootloader_wdt.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:bootloader_panic.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:cache_hal.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:mmu_hal.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    *libhal.a:wdt_hal_iram.*(.sbss .sbss.* .sbss2 .sbss2.* .bss .bss.*)
    . = ALIGN (8);
    _loader_bss_end = ABSOLUTE(.);
  } > dram_loader_seg

  .dram_loader.data :
  {
    . = ALIGN(16);
    *esp_loader.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *esp_mcuboot.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *main.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:bootloader_flash.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:bootloader_sha.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:bootloader_wdt.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:bootloader_panic.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:cache_hal.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:mmu_hal.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:wdt_hal_iram.*(.data .data.* .rodata .rodata.* .sdata .sdata.* .sdata2 .sdata2.* .srodata .srodata.*)
    . = ALIGN(16);
    _dram_loader_end = ABSOLUTE(.);
  } > dram_loader_seg

  /* Ensure the dram_loader_seg is large enough */
  ASSERT((_dram_loader_end - ORIGIN(dram_loader_seg)) <= BOOTLOADER_DRAM_LOADER_SEG_LEN,
    "dram_loader_seg overflow: increase BOOTLOADER_DRAM_LOADER_SEG_LEN")

  .iram.text :
  {
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
  } > iram_seg


  /* Shared RAM */
  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _dram_start = ABSOLUTE(.);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } >dram_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    _data_end = ABSOLUTE(.);
  } >dram_seg

  .dram0.rodata :
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    *(.eh_frame)
    . = (. + 3) & ~ 3;
    /*  C++ constructor and destructor tables, properly ordered:  */
    __init_array_start = ABSOLUTE(.);
    KEEP (*crtbegin.*(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __init_array_end = ABSOLUTE(.);
    KEEP (*crtbegin.*(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    _rodata_end = ABSOLUTE(.);
    /* Literals are also RO data. */
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
    _dram_end = ABSOLUTE(.);
  } >dram_seg

  .iram.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.iram .iram.*) /* catch stray IRAM_ATTR */
    *(.iram1 .iram1.*)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } > iram_seg

}
