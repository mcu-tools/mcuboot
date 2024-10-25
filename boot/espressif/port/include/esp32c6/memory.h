/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-C6 bootloader (matches current bootloader.ld resolve) */
#define BOOTLOADER_IRAM_SEG_START           0x40860610
#define BOOTLOADER_IRAM_SEG_LEN             0x9000

#define BOOTLOADER_IRAM_LOADER_SEG_START    0x40869610
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x7000

#define BOOTLOADER_DRAM_SEG_START           0x40870610
#define BOOTLOADER_DRAM_SEG_LEN             0xA000
