/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-C3 bootloader (matches current bootloader.ld) */
#define BOOTLOADER_IRAM_SEG_START           0x403C7000
#define BOOTLOADER_IRAM_SEG_LEN             0x9000

#define BOOTLOADER_IRAM_LOADER_SEG_START    0x403D0000
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x5400

#define BOOTLOADER_DRAM_SEG_START           0x3FCD5400
#define BOOTLOADER_DRAM_SEG_LEN             0xA000
