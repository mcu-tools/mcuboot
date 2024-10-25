/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-S2 bootloader (matches current bootloader.ld) */
#define BOOTLOADER_IRAM_SEG_START           0x40047000
#define BOOTLOADER_IRAM_SEG_LEN             0x9000

#define BOOTLOADER_IRAM_LOADER_SEG_START    0x40050000
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x6000

#define BOOTLOADER_DRAM_SEG_START           0x3FFE6000
#define BOOTLOADER_DRAM_SEG_LEN             0x9A00
