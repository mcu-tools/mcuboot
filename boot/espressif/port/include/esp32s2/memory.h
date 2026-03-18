/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Default memory layout for ESP32-S2 bootloader */

/* The offset between Dbus and Ibus.
 * Used to convert between 0x4002xxxx and 0x3ffbxxxx addresses.
 */
#define IRAM_DRAM_OFFSET         0x70000

/* Note: this and subsequent addresses calculations keep retrocompatibility
 * for standalone builds of Espressif Port.
 * For builds integrated with the RTOS (e.g. Zephyr), they must provide their
 * `memory.h` which must set the proper bootloader and application boundaries. */
#define BOOTLOADER_RAM_END       0x3FFEAB00    // Ibus: 0x4005AB00

#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x2400
#define BOOTLOADER_IRAM_LOADER_SEG_START \
        (BOOTLOADER_RAM_END + IRAM_DRAM_OFFSET - BOOTLOADER_IRAM_LOADER_SEG_LEN)

#define BOOTLOADER_DRAM_LOADER_SEG_LEN      0x1800
#define BOOTLOADER_DRAM_LOADER_SEG_START \
        (BOOTLOADER_IRAM_LOADER_SEG_START - IRAM_DRAM_OFFSET) - BOOTLOADER_DRAM_LOADER_SEG_LEN

#define BOOTLOADER_IRAM_SEG_LEN             0xE000
#define BOOTLOADER_IRAM_SEG_START \
        (BOOTLOADER_DRAM_LOADER_SEG_START + IRAM_DRAM_OFFSET) - BOOTLOADER_IRAM_SEG_LEN

#define BOOTLOADER_DRAM_SEG_LEN             0xD000
#define BOOTLOADER_DRAM_SEG_START \
        (BOOTLOADER_IRAM_SEG_START - IRAM_DRAM_OFFSET) - BOOTLOADER_DRAM_SEG_LEN
