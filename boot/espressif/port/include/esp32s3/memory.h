/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-S3 bootloader (matches current bootloader.ld) */
#define BOOTLOADER_IRAM_SEG_START           0x403B0000
#define BOOTLOADER_IRAM_SEG_LEN             0xA000

#define BOOTLOADER_IRAM_LOADER_SEG_START    0x403BA000
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x6000

#define BOOTLOADER_DRAM_SEG_START           0x3FCD0000
#define BOOTLOADER_DRAM_SEG_LEN             0xA000
