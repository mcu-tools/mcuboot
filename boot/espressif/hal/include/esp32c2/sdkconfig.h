/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define BOOTLOADER_BUILD 1
#define CONFIG_IDF_FIRMWARE_CHIP_ID 0x000C
#define CONFIG_IDF_TARGET_ESP32C2 1
#define CONFIG_ESP32C2_REV_MIN_0 1
#define CONFIG_ESP32C2_REV_MIN_FULL 3
#define CONFIG_ESP_REV_MIN_FULL CONFIG_ESP32C2_REV_MIN_FULL
#define CONFIG_ESP32C2_REV_MIN 3
#define CONFIG_ESP32C2_REV_MAX_FULL 99
#define CONFIG_ESP_REV_MAX_FULL CONFIG_ESP32C2_REV_MAX_FULL
#define CONFIG_IDF_TARGET_ARCH_RISCV 1
#define CONFIG_MMU_PAGE_SIZE 0x10000
#define CONFIG_XTAL_FREQ_26 1
#define CONFIG_XTAL_FREQ 26
#define CONFIG_SPI_FLASH_ROM_DRIVER_PATCH 1
#define CONFIG_MCUBOOT 1
#define NDEBUG 1
#define CONFIG_BOOTLOADER_WDT_TIME_MS 9000
#define CONFIG_ESP_CONSOLE_UART_BAUDRATE 74880
#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH 0x0000
#define CONFIG_PARTITION_TABLE_OFFSET 0x10000
#define CONFIG_EFUSE_VIRTUAL_OFFSET 0x250000
#define CONFIG_EFUSE_VIRTUAL_SIZE 0x2000
#define CONFIG_EFUSE_MAX_BLK_LEN 256
#define CONFIG_BOOTLOADER_FLASH_XMC_SUPPORT 1
