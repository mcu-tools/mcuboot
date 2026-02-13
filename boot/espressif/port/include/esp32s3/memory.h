/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* The offset between Ibus and Dbus.
 * Used to convert between 0x403xxxxx and 0x3fcxxxxx addresses.
 */
#define IRAM_DRAM_OFFSET         0x6F0000

/* Note: this and subsequent addresses calculations keep retrocompatibility
 * for standalone builds of Espressif Port.
 * For builds integrated with the RTOS (e.g. Zephyr), they must provide their
 * `memory.h` which must set the proper bootloader and application boundaries. */
#define BOOTLOADER_RAM_END       0x403CA000    // Dbus: 0x3FCDA000

/* Default memory layout for ESP32-S3 bootloader */
#define BOOTLOADER_IRAM_LOADER_SEG_LEN      0x2000
#define BOOTLOADER_IRAM_LOADER_SEG_START \
        (BOOTLOADER_RAM_END - BOOTLOADER_IRAM_LOADER_SEG_LEN)

#define BOOTLOADER_DRAM_LOADER_SEG_LEN      0x1800
#define BOOTLOADER_DRAM_LOADER_SEG_START \
        (BOOTLOADER_IRAM_LOADER_SEG_START - IRAM_DRAM_OFFSET) - BOOTLOADER_DRAM_LOADER_SEG_LEN

#define BOOTLOADER_IRAM_SEG_LEN             0xE800
#define BOOTLOADER_IRAM_SEG_START \
        (BOOTLOADER_DRAM_LOADER_SEG_START + IRAM_DRAM_OFFSET) - BOOTLOADER_IRAM_SEG_LEN

#define BOOTLOADER_DRAM_SEG_LEN             0xD000
#define BOOTLOADER_DRAM_SEG_START \
        (BOOTLOADER_IRAM_SEG_START - IRAM_DRAM_OFFSET) - BOOTLOADER_DRAM_SEG_LEN

/* The application image can use static RAM up until USER_DRAM_END/USER_IRAM_END
 * This address is where the bootloader starts allocating memory, and it should
 * not be overlapped by the application. */
#define USER_DRAM_END    (BOOTLOADER_DRAM_LOADER_SEG_START) // 0x3fcd6800
#define USER_IRAM_END    (USER_DRAM_END + IRAM_DRAM_OFFSET) // 0x403c6800
