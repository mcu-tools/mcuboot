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

MEMORY
{
  iram_seg (RWX) :                  org = 0x40093000, len = 0x8800
  iram_loader_seg (RWX) :           org = 0x4009B800, len = 0x4800
  dram_seg (RW) :                   org = 0x3FFF5000, len = 0x8900
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
    *(.iram1 .iram1.*) /* catch stray IRAM_ATTR */
    *libhal.a:bootloader_flash.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_flash_config_esp32.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_clock_loader.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_common_loader.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_flash.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_random.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_random*.*(.literal.bootloader_random_disable .text.bootloader_random_disable)
    *libhal.a:bootloader_random*.*(.literal.bootloader_random_enable .text.bootloader_random_enable)
    *libhal.a:bootloader_efuse_esp32.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_utility.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_sha.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_console_loader.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_panic.*(.literal .text .literal.* .text.*)
    *libhal.a:bootloader_soc.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_image_format.*(.literal .text .literal.* .text.*)
    *libhal.a:flash_encrypt.*(.literal .text .literal.* .text.*)
    *libhal.a:flash_encryption_secure_features.*(.literal .text .literal.* .text.*)
    *libhal.a:flash_partitions.*(.literal .text .literal.* .text.*)
    *libhal.a:secure_boot.*(.literal .text .literal.* .text.*)
    *libhal.a:secure_boot_secure_features.*(.literal .text .literal.* .text.*)
    *libhal.a:secure_boot_signatures_bootloader.*(.literal .text .literal.* .text.*)
    *libhal.a:wdt_hal_iram.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_efuse_table.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_efuse_fields.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_efuse_api.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_efuse_utility.*(.literal .text .literal.* .text.*)
    *libhal.a:esp_efuse_api_key_esp32.*(.literal .text .literal.* .text.*)
    *esp_mcuboot.*(.literal .text .literal.* .text.*)
    *esp_loader.*(.literal .text .literal.* .text.*)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _loader_text_end = ABSOLUTE(.);
  } > iram_loader_seg
  .iram.text :
  {
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
  } > iram_seg
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
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } > iram_seg
}
