/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-H2 bootloader (matches current bootloader.ld resolve) */
#define BOOTLOADER_IRAM_SEG_START           0x408317D0
#define BOOTLOADER_IRAM_SEG_LEN             0x8800

#define BOOTLOADER_IRAM_LOADER_SEG_START    0x40839FD0
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x7000

#define BOOTLOADER_DRAM_SEG_START           0x40840FD0
#define BOOTLOADER_DRAM_SEG_LEN             0xA000
