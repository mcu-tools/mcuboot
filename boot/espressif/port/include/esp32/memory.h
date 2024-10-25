/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32 bootloader (matches current bootloader.ld) */
#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
#define BOOTLOADER_IRAM_LOADER_SEG_START_MP 0x400AB900
#endif
#define BOOTLOADER_IRAM_LOADER_SEG_START    0x40078000
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x6500

#define BOOTLOADER_IRAM_SEG_START           0x40090000
#define BOOTLOADER_IRAM_SEG_LEN             0x9000

#define BOOTLOADER_DRAM_SEG_START           0x3FFF4700
#define BOOTLOADER_DRAM_SEG_LEN             0xB900
