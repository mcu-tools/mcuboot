/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 * Copyright (c) 2022 Infineon Technologies AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <stdint.h>

#include "bootutil/bootutil.h"
#include "cy_flash.h"
#include "flash_map_backend_platform.h"

static int erase(uint8_t fa_device_id, uintptr_t addr, uint32_t len);
static int write(uint8_t fa_device_id, uintptr_t addr, const void *src,
                 uint32_t len);

static uint32_t get_min_erase_size(uint8_t fa_device_id)
{
    return flash_devices[fa_device_id].erase_size;
}

static uint8_t get_erase_val(uint8_t fa_device_id)
{
    return flash_devices[fa_device_id].erase_val;
}

static inline uint32_t get_base_address(uint8_t fa_device_id)
{
    return flash_devices[fa_device_id].address;
}

/*
 * Reads `len` bytes of flash memory at `off` to the buffer at `dst`
 */
static int read(uint8_t fa_device_id, uintptr_t addr, void *dst, uint32_t len)
{
    (void)fa_device_id;

    int rc = -1;
    void *src;
    void *result = NULL;

    /* Convert from uintptr_t to void*, MISRA C 11.6 */
    (void)memcpy((void *)&src, (void const *)&addr, sizeof(void *));
    /* flash read by simple memory copying */
    result = memcpy(dst, src, (size_t)len);
    if (result == dst) {
        rc = 0;
    }

    return rc;
}

/*
 * Writes `len` bytes of flash memory at `off` from the buffer at `src`
 */
static int write(uint8_t fa_device_id, uintptr_t addr, const void *src,
                 uint32_t len)
{
    (void)fa_device_id;

    int rc                   = BOOT_EFLASH;
    uintptr_t write_end_addr = 0u;
    const uint32_t *row_ptr  = NULL;
    uint32_t row_number      = 0u;
    uint32_t row_addr        = 0u;
    uint32_t write_sz        = 0x80;

    cy_stc_flash_programrow_config_t cfg = {
        .blocking = CY_FLASH_PROGRAMROW_BLOCKING,
        .dataLoc  = CY_FLASH_PROGRAMROW_DATA_LOCATION_SRAM,
        .intrMask = CY_FLASH_PROGRAMROW_NOT_SET_INTR_MASK,
        .dataSize = CY_FLASH_PROGRAMROW_DATA_SIZE_1024BIT};

    if (src != NULL) {
        write_end_addr = addr + len;

        if (len % write_sz != 0u) {
            rc = BOOT_EBADARGS;
        } else if (addr % write_sz != 0u) {
            rc = BOOT_EBADARGS;
        } else {
            /* do nothing */
        }
        if (rc != BOOT_EBADARGS) {
            row_number = (write_end_addr - addr) / write_sz;
            row_addr   = addr;

            row_ptr = (const uint32_t *)src;

            for (uint32_t i = 0; i < row_number; i++) {
                cfg.destAddr = (const uint32_t *)row_addr;
                cfg.dataAddr = (const uint32_t *)row_ptr;

                if (Cy_Flash_Program_WorkFlash(&cfg) != CY_FLASH_DRV_SUCCESS) {
                    rc = BOOT_EFLASH;
                    break;
                } else {
                    rc = 0;
                }
                row_addr += (uint32_t)write_sz;
                row_ptr = row_ptr + write_sz / 4U;
            }
        }
    }

    return rc;
}

/*< Erases `len` bytes of flash memory at `off` */
static int erase(uint8_t fa_device_id, uintptr_t addr, uint32_t len)
{
    int rc                   = -1;
    uint32_t row_addr        = 0u;
    uint32_t erase_sz        = flash_devices[fa_device_id].erase_size;
    uintptr_t erase_end_addr = addr + len;
    uint32_t row_start_addr  = (addr / erase_sz) * erase_sz;
    uint32_t row_end_addr    = (erase_end_addr / erase_sz) * erase_sz;
    uint32_t row_number      = (row_end_addr - row_start_addr) / erase_sz;

    /* assume single row needs to be erased */
    if (row_start_addr == row_end_addr) {
        row_number = 1U;
    }

    while (row_number != 0u) {
        row_number--;
        row_addr = row_start_addr + row_number * (uint32_t)erase_sz;
        if (Cy_Flash_EraseSector(row_addr) != CY_FLASH_DRV_SUCCESS) {
            rc = BOOT_EFLASH;
            break;
        } else {
            rc = 0;
        }
    }

    return rc;
}

static int open(uint8_t fa_device_id)
{
    (void)fa_device_id;

    Cy_Flashc_WorkWriteEnable();

    return 0;
}

static void close(uint8_t fa_device_id)
{
    (void)fa_device_id;
}

const struct flash_area_interface internal_mem_eeprom_interface = {
    .open             = &open,
    .close            = &close,
    .read             = &read,
    .write            = &write,
    .erase            = &erase,
    .get_erase_val    = &get_erase_val,
    .get_erase_size   = &get_min_erase_size,
    .get_base_address = &get_base_address};
