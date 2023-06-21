/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Magic is derived from sha256sum of the string "espmcuboot"
 * The application header magic must match this number
 */
#define ESP_LOAD_HEADER_MAGIC 0xace637d3

/* Load header that should be a part of application image
 * for MCUboot-Espressif port booting.
 */
typedef struct esp_image_load_header {
    uint32_t header_magic;          /* Magic for load header */
    uint32_t entry_addr;            /* Application entry address */
    uint32_t iram_dest_addr;        /* Destination address(VMA) for IRAM region */
    uint32_t iram_flash_offset;     /* Flash offset(LMA) for start of IRAM region */
    uint32_t iram_size;             /* Size of IRAM region */
    uint32_t dram_dest_addr;        /* Destination address(VMA) for DRAM region */
    uint32_t dram_flash_offset;     /* Flash offset(LMA) for start of DRAM region */
    uint32_t dram_size;             /* Size of DRAM region */
} esp_image_load_header_t;
