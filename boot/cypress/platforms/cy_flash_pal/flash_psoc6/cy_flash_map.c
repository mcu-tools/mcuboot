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

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mcuboot_config/mcuboot_config.h"
#include "flash_map_backend/flash_map_backend.h"
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/fault_injection_hardening.h" /* for FIH_PANIC */

#include "cy_flash.h"
#include "cy_flash_map.h"

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
#include "cy_smif_psoc6.h"
#endif

#ifdef MCUBOOT_SWAP_USING_STATUS
#include "swap_status.h"
#endif

#ifndef CY_BOOT_INTERNAL_FLASH_ERASE_VALUE
/* This is the value of internal flash bytes after an erase */
#define CY_BOOT_INTERNAL_FLASH_ERASE_VALUE  (0x00u)
#endif /* CY_BOOT_INTERNAL_FLASH_ERASE_VALUE */

#ifndef CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE
/* This is the value of external flash bytes after an erase */
#define CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE  (0xFFu)
#endif /* CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE */

/*
 * Returns device flash start based on supported fd_id
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    int rc = -1;

    if (ret != NULL) {
        if (FLASH_DEVICE_INTERNAL_FLASH == fd_id) {
            *ret = CY_FLASH_BASE;
            rc = 0;
        }
    #ifdef CY_BOOT_USE_EXTERNAL_FLASH
        else if ((fd_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            *ret = CY_SMIF_BASE_MEM_OFFSET;
            rc = 0;
        }
    #endif /* CY_BOOT_USE_EXTERNAL_FLASH */
        else {
            BOOT_LOG_ERR("invalid flash ID %u; expected %u or %u",
                        (unsigned)fd_id, FLASH_DEVICE_INTERNAL_FLASH,
                        FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX));
        }
    }

    return rc;
}

/*
 * Opens the area for use. id is one of the `fa_id`s
 */
int flash_area_open(uint8_t id, const struct flash_area **fa)
{
    int rc = -1;
    uint32_t i = 0u;

    if (fa != NULL) {
        while (boot_area_descs[i] != NULL) {

            if (id == boot_area_descs[i]->fa_id) {
                *fa = boot_area_descs[i];
                rc = 0;
                break;
            }
            i++;
        }
    }

    return rc;
}

/*
 * Clear pointer to flash area fa
 */
void flash_area_close(const struct flash_area *fa)
{
    (void)fa; /* Nothing to do there */
}

/*
 * Reads `len` bytes of flash memory at `off` to the buffer at `dst`
 */
int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                     uint32_t len)
{
    int rc = -1;
    uintptr_t addr = 0u;
    uintptr_t flash_base = 0u;
    void* src;

    if ((dst != NULL) && (fa != NULL)) {

        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size) {

            return BOOT_EBADARGS;
        }

        rc = flash_device_base(fa->fa_device_id, &flash_base);

        if (0 == rc) {

            addr = flash_base + fa->fa_off + off;

            if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
                /* Convert from uintptr_t to void*, MISRA C 11.6 */
                (void)memcpy((void *)&src, (void const *)&addr, sizeof(void*));
                /* flash read by simple memory copying */
                (void)memcpy(dst, src, (size_t)len);
            }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
            else {
                rc = psoc6_smif_read(fa, addr, dst, len);
            }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
        }
    }

    return rc;
}

/*
 * Writes `len` bytes of flash memory at `off` from the buffer at `src`
 */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len)
{
    int rc = BOOT_EFLASH;
    uintptr_t write_start_addr = 0u;
    uintptr_t write_end_addr = 0u;
    const uint32_t * row_ptr = NULL;
    uintptr_t flash_base = 0u;

    if ((src != NULL) && (fa != NULL)) {

        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size) {

            return BOOT_EBADARGS;
        }

        rc = flash_device_base(fa->fa_device_id, &flash_base);

        if (0 == rc) {

            write_start_addr = flash_base + fa->fa_off + off;
            write_end_addr = flash_base + fa->fa_off + off + len;

            if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {

                uint32_t row_number = 0u;
                uint32_t row_addr = 0u;

                if (len % CY_FLASH_SIZEOF_ROW != 0u) {
                    return BOOT_EBADARGS;
                }

                if (write_start_addr % CY_FLASH_SIZEOF_ROW != 0u) {
                    return BOOT_EBADARGS;
                }

                row_number = (write_end_addr - write_start_addr) / CY_FLASH_SIZEOF_ROW;
                row_addr = write_start_addr;

                row_ptr = (const uint32_t *) src;

                for (uint32_t i = 0; i < row_number; i++) {
                    if (Cy_Flash_WriteRow(row_addr, row_ptr) != CY_FLASH_DRV_SUCCESS) {
                        rc = BOOT_EFLASH;
                        break;
                    }

                    row_addr += (uint32_t) CY_FLASH_SIZEOF_ROW;
                    row_ptr = row_ptr + CY_FLASH_SIZEOF_ROW / 4U;
                }
            }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
            else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) != 0u) {
                rc = psoc6_smif_write(fa, write_start_addr, src, len);
            }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
            else {
                return BOOT_EFLASH;
            }
        }
    }

    return rc;
}

/*< Erases `len` bytes of flash memory at `off` */
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    int rc = -1;
    uintptr_t erase_start_addr = 0u;
    uintptr_t erase_end_addr = 0u;
    uintptr_t flash_base = 0u;

    if (fa != NULL) {
        if (off > fa->fa_size ||
            len > fa->fa_size ||
            off + len > fa->fa_size) {

            return BOOT_EBADARGS;
        }

        rc = flash_device_base(fa->fa_device_id, &flash_base);

        if (0 == rc) {
            erase_start_addr = flash_base + fa->fa_off + off;
            erase_end_addr = flash_base + fa->fa_off + off + len;

            if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
                uint32_t row_number = 0u;
                uint32_t row_addr = 0u;
                uint32_t row_start_addr = (erase_start_addr / CY_FLASH_SIZEOF_ROW) * CY_FLASH_SIZEOF_ROW;
                uint32_t row_end_addr = (erase_end_addr / CY_FLASH_SIZEOF_ROW) * CY_FLASH_SIZEOF_ROW;

                /* assume single row needs to be erased */
                if (row_start_addr == row_end_addr) {
                    if (Cy_Flash_EraseRow(row_start_addr) != CY_FLASH_DRV_SUCCESS) {
                        rc = BOOT_EFLASH;
                    }
                }
                else {
                    row_number = (row_end_addr - row_start_addr) / CY_FLASH_SIZEOF_ROW;

                    while (row_number != 0u) {
                        row_number--;
                        row_addr = row_start_addr + row_number * (uint32_t) CY_FLASH_SIZEOF_ROW;
                        if (Cy_Flash_EraseRow(row_addr) != CY_FLASH_DRV_SUCCESS) {
                            rc = BOOT_EFLASH;
                            break;
                        }
                    }
                }
            }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
            else {
                rc = psoc6_smif_erase(erase_start_addr, len);
            }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
        }
    }

    return rc;
}

/*< Returns this `flash_area`s alignment */
size_t flash_area_align(const struct flash_area *fa)
{
    size_t rc = 0u; /* error code (alignment cannot be zero) */

    if (fa != NULL) {
        if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
            rc = CY_FLASH_ALIGN;
        }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
        else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            rc = qspi_get_prog_size();
        }
        else {
            /* No action required */
        }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
    }

    return rc;
}

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
/*< Initializes an array of flash_area elements for the slot's sectors */
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa)
{
    int rc = -1;

    if (cnt != NULL && fa != NULL) {
        if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
            (void)idx;
            (void)cnt;
            rc = 0;
        }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
        else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
            (void)idx;
            (void)cnt;
            rc = 0;
        }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
        else {
            /* incorrect/non-existing flash device id */
            rc = -1;
        }
    }

    return rc;
}
#endif /* MCUBOOT_USE_FLASH_AREA_GET_SECTORS */

/*
 * This depends on the mappings defined in sysflash.h.
 * MCUBoot uses continuous numbering for the primary slot, the secondary slot,
 * and the scratch while zephyr might number it differently.
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    int rc = -1;
    if ((image_index < 0) || (image_index >= MCUBOOT_IMAGE_NUMBER)) {
        return -1;
    }

    switch (slot) {
        case 0:
            rc = (int)FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index);
            break;
        case 1:
            rc = (int)FLASH_AREA_IMAGE_SECONDARY((uint32_t)image_index);
            break;
        case 2:
            rc = (int)FLASH_AREA_IMAGE_SCRATCH;
            break;
        default:
            rc = -1; /* flash_area_open will fail on that */
            break;
    }
    return rc;
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if ((image_index < 0) || (image_index >= MCUBOOT_IMAGE_NUMBER)) {
        return -1;
    }

    if ((int) FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index) == area_id) {
        return 0;
    }
    if ((int) FLASH_AREA_IMAGE_SECONDARY((uint32_t)image_index) == area_id) {
        return 1;
    }

    return -1;
}

int flash_area_id_to_image_slot(int area_id)
{
    return flash_area_id_to_multi_image_slot(0, area_id);
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    uint8_t ret = 0u;

    if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
        ret = (uint8_t) CY_BOOT_INTERNAL_FLASH_ERASE_VALUE;
    }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) != 0u) {
        ret = (uint8_t) CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE;
    }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
    else {
        ret = 0u; /* The variable is reassigned to make the statement comply with the MISRA rule 15.7 */
        FIH_PANIC; /* There is no appropriate error code */
    }

    return ret;
}

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
int flash_area_get_sectors(int idx, uint32_t *cnt, struct flash_sector *ret)
{
    int rc = 0;
    uint32_t i = 0u;
    struct flash_area *fa = NULL;
    size_t sectors_n = 0u;
    uint32_t my_sector_addr = 0u;
    uint32_t my_sector_size = 0u;

    while (boot_area_descs[i] != NULL) {
        if (idx == (int) boot_area_descs[i]->fa_id) {
            fa = boot_area_descs[i];
            break;
        }
        i++;
    }

    if ((fa != NULL) && (cnt != NULL) && (ret != NULL)) {
        size_t sector_size = 0;
        size_t area_size = fa->fa_size;

        if (FLASH_DEVICE_INTERNAL_FLASH == fa->fa_device_id) {
            sector_size = CY_FLASH_SIZEOF_ROW;
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && defined(MCUBOOT_SWAP_USING_STATUS) && !defined(MCUBOOT_SWAP_USING_SCRATCH)
            if ((int) FLASH_AREA_IMAGE_SWAP_STATUS == idx) {
                sector_size = CY_FLASH_SIZEOF_ROW;
            }
            else {
                sector_size = qspi_get_erase_size();
            }
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && defined(MCUBOOT_SWAP_USING_STATUS) && !defined(MCUBOOT_SWAP_USING_SCRATCH) */
        }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
        else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
        /* implement for SMIF */
        /* lets assume they are equal */
#if defined(MCUBOOT_SWAP_USING_STATUS) || defined(USE_XIP)
            sector_size = qspi_get_erase_size();
#else
            sector_size = CY_FLASH_SIZEOF_ROW;
#endif /* MCUBOOT_SWAP_USING_STATUS */
        }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
        else {
            /* fa->fa_device_id = FLASH_DEVICE_UNDEFINED,
               in this case the area should be empty with a very simple sector size of 1 byte */
            area_size = 0u;
            sector_size = 1u;
        }

        sectors_n = (area_size + (sector_size - 1U)) / sector_size;

        if (sectors_n > (size_t)MCUBOOT_MAX_IMG_SECTORS) {
            sector_size *= 2u;
        }

        sectors_n = 0;
        my_sector_addr = fa->fa_off;
        while (area_size > 0u) {
            my_sector_size = sector_size;
#ifdef MCUBOOT_SWAP_USING_SCRATCH
            uint32_t my_sector_offs = my_sector_addr % my_sector_size;

            if (my_sector_offs != 0u) {
                my_sector_size = sector_size - my_sector_offs;
            }

            if (my_sector_size > area_size) {
                my_sector_size = area_size;
            }
#endif /* MCUBOOT_SWAP_USING_SCRATCH */
            ret[sectors_n].fs_size = my_sector_size;
            ret[sectors_n].fs_off = my_sector_addr;

            my_sector_addr += my_sector_size;
            if (area_size >= my_sector_size) {
            	area_size -= my_sector_size;
            	sectors_n++;
            }
            else {
            	area_size = 0;
            }
        }

        if (sectors_n <= *cnt) {
            *cnt = sectors_n;
        }
        else {
            rc = -1;
        }
    }
    else {
        rc = -1;
    }

    return rc;
}
#endif /* MCUBOOT_USE_FLASH_AREA_GET_SECTORS */
