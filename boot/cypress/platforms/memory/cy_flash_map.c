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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/fault_injection_hardening.h" /* for FIH_PANIC */
#include "cy_flash.h"
#include "flash_map_backend_platform.h"
#include "mcuboot_config/mcuboot_config.h"
#include "memorymap.h"

/*
 * Macro takes flash API parameters fa, off and len
 * Checks flash area boundaries considering off and len
 * Returns absolute address of flash area memory where
 * operation should execute
 */
#define MEM_VALIDATE_AND_GET_ADDRES(fa, off, len, addr, rc)          \
    uintptr_t mem_base = 0u;                                         \
    if ((fa) != NULL) {                                              \
        if ((off) > (fa)->fa_size ||                                 \
            (len) > (fa)->fa_size ||                                 \
            (off) + (len) > (fa)->fa_size) {                         \
            (rc) = BOOT_EBADARGS;                                    \
        }                                                            \
        if (flash_area_get_api((fa)->fa_device_id) != NULL) {        \
            (rc) = flash_device_base((fa)->fa_device_id, &mem_base); \
            if ((0 == (rc)) && (mem_base != 0u)) {                   \
                (addr) = mem_base + (fa)->fa_off + (off);            \
            }                                                        \
        }                                                            \
    }

/*
 * PORTING GUIDE: REQUIRED
 * Returns device flash start based on supported fd_id
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    int rc = -1;

    if (ret != NULL) {
        if (flash_area_get_api(fd_id) != NULL) {
            *ret = flash_area_get_api(fd_id)->get_base_address(fd_id);
            rc = 0;
        }
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
 * Opens the area for use. id is one of the `fa_id`s
 */
int flash_area_open(uint8_t fa_id, const struct flash_area **fa)
{
    int rc = -1;
    uint32_t i = 0u;

    if (fa != NULL) {
        while (boot_area_descs[i] != NULL) {
            if (fa_id == boot_area_descs[i]->fa_id) {
                *fa = boot_area_descs[i];
                if (flash_area_get_api((*fa)->fa_device_id) != NULL) {
                    rc = flash_area_get_api((*fa)->fa_device_id)->open((*fa)->fa_device_id);
                    break;
                }
            }
            i++;
        }
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
 * Clear pointer to flash area fa
 */
void flash_area_close(const struct flash_area *fa)
{
    if (fa != NULL) {
        if (flash_area_get_api(fa->fa_device_id) != NULL) {
            flash_area_get_api(fa->fa_device_id)->close(fa->fa_device_id);
        }
    }
}

/*
 * PORTING GUIDE: REQUIRED
 * Reads `len` bytes of flash memory at `off` to the buffer at `dst`
 */
int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                    uint32_t len)
{
    int rc = -1;
    uintptr_t addr = 0u;

    MEM_VALIDATE_AND_GET_ADDRES(fa, off, len, addr, rc);
    if (addr != 0u) {
        rc = flash_area_get_api(fa->fa_device_id)->read(fa->fa_device_id, addr, dst, len);
    } else {
        /* do nothing */
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
 * Writes `len` bytes of flash memory at `off` from the buffer at `src`
 */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len)
{
    int rc = BOOT_EFLASH;
    uintptr_t addr = 0u;

    MEM_VALIDATE_AND_GET_ADDRES(fa, off, len, addr, rc);
    if (addr != 0u) {
        rc = flash_area_get_api(fa->fa_device_id)->write(fa->fa_device_id, addr, src, len);
    } else {
        /* do nothing */
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
 * Erases `len` bytes of flash memory at `off`
 */
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    int rc = -1;
    uintptr_t addr = 0u;

    MEM_VALIDATE_AND_GET_ADDRES(fa, off, len, addr, rc);
    if (addr != 0u) {
        rc = flash_area_get_api(fa->fa_device_id)->erase(fa->fa_device_id, addr, len);
    } else {
        /* do nothing */
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
 * Returns this `flash_area`s alignment
 */
size_t flash_area_align(const struct flash_area *fa)
{
    size_t rc = 0u; /* error code (alignment cannot be zero) */

    if ((fa != NULL) && (flash_area_get_api(fa->fa_device_id) != NULL)) {
        rc = flash_area_get_api(fa->fa_device_id)->get_erase_size(fa->fa_device_id);
    }

    return rc;
}

/*
 * PORTING GUIDE: REQUIRED
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
#ifdef MCUBOOT_SWAP_USING_SCRATCH
        case 2:
            rc = (int)FLASH_AREA_IMAGE_SCRATCH;
            break;
#endif
        default:
            rc = -1; /* flash_area_open will fail on that */
            break;
    }
    return rc;
}

/* PORTING GUIDE: REQUIRED */
int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

/* PORTING GUIDE: REQUIRED */
int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if ((image_index < 0) || (image_index >= MCUBOOT_IMAGE_NUMBER)) {
        return -1;
    }

    if ((int)FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index) == area_id) {
        return 0;
    }
    if ((int)FLASH_AREA_IMAGE_SECONDARY((uint32_t)image_index) == area_id) {
        return 1;
    }

    return -1;
}

/* PORTING GUIDE: NOT REQUIRED
 * This API is not used in mcuboot common code. It exists, however
 * to complement flash_area_id_from_image_slot()
 */

int flash_area_id_to_image_slot(int area_id)
{
    return flash_area_id_to_multi_image_slot(0, area_id);
}

/* PORTING GUIDE: REQUIRED
 * Returns the value expected to be read when accesing any erased
 * flash byte.
 */
uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    if ((fa != NULL) && (flash_area_get_api(fa->fa_device_id))) {
        return flash_area_get_api(fa->fa_device_id)->get_erase_val(fa->fa_device_id);
    } else {
        return 0u;
    }
}

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS

static int flash_area_get_fa_from_area_id(int idx, struct flash_area **fa)
{
    int rc = -1;
    uint32_t i = 0u;

    while (boot_area_descs[i] != NULL) {
        if (idx == (int)boot_area_descs[i]->fa_id) {
            *fa = boot_area_descs[i];
            rc = 0;
            break;
        }
        i++;
    }

    return rc;
}

int flash_area_get_sectors(int idx, uint32_t *cnt, struct flash_sector *ret)
{
    int rc = 0;
    struct flash_area *fa = NULL;
    size_t sectors_n = 0u;
    uint32_t my_sector_addr = 0u;
    uint32_t my_sector_size = 0u;

    rc = flash_area_get_fa_from_area_id(idx, &fa);

    if ((fa != NULL) && (cnt != NULL) && (ret != NULL) && (rc == 0) && (flash_area_get_api(fa->fa_device_id) != NULL)) {
        size_t sector_size = flash_area_get_api(fa->fa_device_id)->get_erase_size(fa->fa_device_id);
        size_t area_size = fa->fa_size;

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
            } else {
                area_size = 0;
                sectors_n++;
            }
        }

        if (sectors_n <= *cnt) {
            *cnt = sectors_n;
        } else {
            rc = -1;
        }
    } else {
        rc = -1;
    }

    return rc;
}
#endif /* MCUBOOT_USE_FLASH_AREA_GET_SECTORS */
