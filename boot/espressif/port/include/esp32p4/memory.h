/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-P4 bootloader (rev >= 3.0) */

/* Note: this and subsequent addresses calculations keep retrocompatibility
 * for standalone builds of Espressif Port.
 * For builds integrated with the RTOS (e.g. Zephyr), they must provide their
 * `memory.h` which must set the proper bootloader and application boundaries.
 *
 * Derived from IDF bootloader.rev3.ld.in:
 *   bootloader_usable_dram_end = 0x4ffbcfc0 - 0x2000
 */
#define BOOTLOADER_RAM_END                  0x4FFBAFC0

#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x2400
#define BOOTLOADER_IRAM_LOADER_SEG_START \
        (BOOTLOADER_RAM_END - BOOTLOADER_IRAM_LOADER_SEG_LEN)

#define BOOTLOADER_DRAM_LOADER_SEG_LEN      0x1800
#define BOOTLOADER_DRAM_LOADER_SEG_START \
        (BOOTLOADER_IRAM_LOADER_SEG_START - BOOTLOADER_DRAM_LOADER_SEG_LEN)

#define BOOTLOADER_IRAM_SEG_LEN             0xF000
#define BOOTLOADER_IRAM_SEG_START \
        (BOOTLOADER_DRAM_LOADER_SEG_START - BOOTLOADER_IRAM_SEG_LEN)

#define BOOTLOADER_DRAM_SEG_LEN             0xB000
#define BOOTLOADER_DRAM_SEG_START \
        (BOOTLOADER_IRAM_SEG_START - BOOTLOADER_DRAM_SEG_LEN)
