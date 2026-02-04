/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-H2 bootloader */

/* Note: this and subsequent addresses calculations keep retrocompatibility
 * for standalone builds of Espressif Port.
 * For builds integrated with the RTOS (e.g. Zephyr), they must provide their
 * `memory.h` which must set the proper bootloader and application boundaries. */
#define BOOTLOADER_RAM_END                  0x4084AFD0

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
