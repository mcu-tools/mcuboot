/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * Original license:
 *
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

#include "bootutil_area.h"
#include "bootutil_priv.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_public.h"
#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/**
 * Amount of space used to save information required when doing a swap,
 * or while a swap is under progress, but not the status of sector swap
 * progress itself.
 */
static inline uint32_t
boot_trailer_info_sz(void)
{
    return (
#ifdef MCUBOOT_ENC_IMAGES
           /* encryption keys */
#  if MCUBOOT_SWAP_SAVE_ENCTLV
           BOOT_ENC_TLV_ALIGN_SIZE * 2            +
#  else
           BOOT_ENC_KEY_ALIGN_SIZE * 2            +
#  endif
#endif
           /* swap_type + copy_done + image_ok + swap_size */
           BOOT_MAX_ALIGN * 4                     +
#ifdef MCUBOOT_SWAP_USING_OFFSET
           /* TLV size for both slots */
           BOOT_MAX_ALIGN                         +
#endif
           BOOT_MAGIC_ALIGN_SIZE
           );
}

/**
 * Amount of space used to maintain progress information for a single swap
 * operation.
 */
static inline uint32_t
boot_status_entry_sz(uint32_t min_write_sz)
{
#if defined(MCUBOOT_SINGLE_APPLICATION_SLOT) ||      \
    defined(MCUBOOT_FIRMWARE_LOADER) ||              \
    defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    /* Single image MCUboot modes do not have a swap status fields */
    return 0;
#else
    return BOOT_STATUS_STATE_COUNT * min_write_sz;
#endif
}

uint32_t
boot_status_sz(uint32_t min_write_sz)
{
    return BOOT_STATUS_MAX_ENTRIES * boot_status_entry_sz(min_write_sz);
}

uint32_t
boot_trailer_sz(uint32_t min_write_sz)
{
    return boot_status_sz(min_write_sz) + boot_trailer_info_sz();
}

#if MCUBOOT_SWAP_USING_SCRATCH
uint32_t boot_scratch_trailer_sz(uint32_t min_write_sz)
{
    return boot_status_entry_sz(min_write_sz) + boot_trailer_info_sz();
}
#endif

#if defined(MCUBOOT_MINIMAL_SCRAMBLE)
/**
 * Get size of header aligned to device erase unit or write block, depending on whether device has
 * erase or not.
 *
 * @param fa    The flash_area containing the header.
 * @param slot  The slot the header belongs to.
 * @param off   Pointer to variable to store the offset of the header.
 * @param size  Pointer to variable to store the size of the header.
 *
 * @return 0 on success; nonzero on failure.
 */
static int
boot_header_scramble_off_sz(const struct flash_area *fa, int slot, size_t *off, size_t *size)
{
    int ret = 0;
    const size_t write_block = flash_area_align(fa);
    size_t loff = 0;
    struct flash_sector sector;

    BOOT_LOG_DBG("boot_header_scramble_off_sz: slot %d", slot);

    (void)slot;
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    /* In case of swap offset, header of secondary slot image is positioned
     * in second sector of slot.
     */
    if (slot == BOOT_SLOT_SECONDARY) {
        ret = flash_area_get_sector(fa, 0, &sector);
        if (ret < 0) {
            return ret;
        }
        loff = flash_sector_get_size(&sector);
        BOOT_LOG_DBG("boot_header_scramble_off_sz: adjusted loff %d", loff);
    }
#endif

    if (device_requires_erase(fa)) {
        /* For device requiring erase align to erase unit */
        ret = flash_area_get_sector(fa, loff, &sector);
        if (ret < 0) {
            return ret;
        }

        *size = flash_sector_get_size(&sector);
    } else {
        /* For device not requiring erase align to write block */
        *size = ALIGN_UP(sizeof(((struct image_header *)0)->ih_magic), write_block);
    }
    *off = loff;

    BOOT_LOG_DBG("boot_header_scramble_off_sz: size %u", (unsigned int)*size);

    return ret;
}
#endif /* MCUBOOT_MINIMAL_SCRAMBLE */

int
boot_trailer_scramble_offset(const struct flash_area *fa, size_t alignment, size_t *off)
{
    int ret = 0;

    BOOT_LOG_DBG("boot_trailer_scramble_offset: flash_area %p, alignment %u",
                 fa, (unsigned int)alignment);

    /* Not allowed to enforce alignment smaller than device allows */
    if (alignment < flash_area_align(fa)) {
        alignment = flash_area_align(fa);
    }

    if (device_requires_erase(fa)) {
        /* For device requiring erase align to erase unit */
        struct flash_sector sector;

        ret = flash_area_get_sector(fa, flash_area_get_size(fa) - boot_trailer_sz(alignment),
                                    &sector);
        if (ret < 0) {
            return ret;
        }

        *off = flash_sector_get_off(&sector);
    } else {
        /* For device not requiring erase align to write block */
        *off = flash_area_get_size(fa) - ALIGN_DOWN(boot_trailer_sz(alignment), alignment);
    }

    BOOT_LOG_DBG("boot_trailer_scramble_offset: final alignment %u, offset %u",
                 (unsigned int)alignment, (unsigned int)*off);

    return ret;
}

int
boot_erase_region(const struct flash_area *fa, uint32_t off, uint32_t size, bool backwards)
{
    int rc = 0;

    BOOT_LOG_DBG("boot_erase_region: flash_area %p, offset %" PRIu32 ""
                 ", size %" PRIu32 ", backwards == %" PRIu8,
                 fa, off, size, (int)backwards);

    if (off >= flash_area_get_size(fa) || (flash_area_get_size(fa) - off) < size) {
        rc = -1;
        goto end;
    } else if (device_requires_erase(fa)) {
        uint32_t end_offset = 0;
        struct flash_sector sector;

        BOOT_LOG_DBG("boot_erase_region: device with erase");

        if (backwards) {
            /* Get the lowest page offset first */
            rc = flash_area_get_sector(fa, off, &sector);

            if (rc < 0) {
                goto end;
            }

            end_offset = flash_sector_get_off(&sector);

            /* Set boundary condition, the highest probable offset to erase, within
             * last sector to erase
             */
            off += size - 1;
        } else {
            /* Get the highest page offset first */
            rc = flash_area_get_sector(fa, (off + size - 1), &sector);

            if (rc < 0) {
                goto end;
            }

            end_offset = flash_sector_get_off(&sector);
        }

        while (true) {
            /* Size to read in this iteration */
            size_t csize;

            /* Get current sector and, also, correct offset */
            rc = flash_area_get_sector(fa, off, &sector);

            if (rc < 0) {
                goto end;
            }

            /* Corrected offset and size of current sector to erase */
            off = flash_sector_get_off(&sector);
            csize = flash_sector_get_size(&sector);

            rc = flash_area_erase(fa, off, csize);

            if (rc < 0) {
                goto end;
            }

            MCUBOOT_WATCHDOG_FEED();

            if (backwards) {
                if (end_offset >= off) {
                    /* Reached the first offset in range and already erased it */
                    break;
                }

                /* Move down to previous sector, the flash_area_get_sector will
                 * correct the value to real page offset
                 */
                off -= 1;
            } else {
                /* Move up to next sector */
                off += csize;

                if (off > end_offset) {
                    /* Reached the end offset in range and already erased it */
                    break;
                }

                /* Workaround for flash_sector_get_off() being broken in mynewt, hangs with
                 * infinite loop if this is not present, should be removed if bug is fixed.
                 */
                off += 1;
            }
        }
    } else {
        BOOT_LOG_DBG("boot_erase_region: device without erase");
    }

end:
    return rc;
}

int
boot_scramble_region(const struct flash_area *fa, uint32_t off, uint32_t size, bool backwards)
{
    int rc = 0;

    BOOT_LOG_DBG("boot_scramble_region: %p %" PRIu32 " %" PRIu32 " %d",
                 fa, off, size, (int)backwards);

    if (size == 0) {
        goto done;
    }

    if (device_requires_erase(fa)) {
        rc = boot_erase_region(fa, off, size, backwards);
    } else if (off >= flash_area_get_size(fa) || (flash_area_get_size(fa) - off) < size) {
        rc = -1;
        goto done;
    } else {
        uint8_t buf[BOOT_MAX_ALIGN];
        const size_t write_block = flash_area_align(fa);
        uint32_t end_offset;

        BOOT_LOG_DBG("boot_scramble_region: device without erase, overwriting");
        memset(buf, flash_area_erased_val(fa), sizeof(buf));

        if (backwards) {
            end_offset = ALIGN_DOWN(off, write_block);
            /* Starting at the last write block in range */
            off += size - write_block;
        } else {
            end_offset = ALIGN_DOWN((off + size), write_block);
        }
        BOOT_LOG_DBG("boot_scramble_region: start offset %" PRIu32 ", "
                     "end offset %" PRIu32, off, end_offset);

        while (off != end_offset) {
            /* Write over the area to scramble data that is there */
            rc = flash_area_write(fa, off, buf, write_block);
            if (rc != 0) {
                BOOT_LOG_DBG("boot_scramble_region: error %d for %p "
                             "%" PRIu32 " %u",
                             rc, fa, off, (unsigned int)write_block);
                break;
            }

            MCUBOOT_WATCHDOG_FEED();

            if (backwards) {
                if (end_offset >= off) {
                    /* Reached the first offset in range and already scrambled it */
                    break;
                }

                off -= write_block;
            } else {
                off += write_block;

                if (end_offset <= off) {
                    /* Reached the end offset in range and already scrambled it */
                    break;
                }
            }
        }
    }

done:
    return rc;
}

int
boot_scramble_slot(const struct flash_area *fa, int slot)
{
    size_t size;
    int ret = 0;

    (void)slot;

    /* Without minimal entire area needs to be scrambled */
#if !defined(MCUBOOT_MINIMAL_SCRAMBLE)
    size = flash_area_get_size(fa);
    ret = boot_scramble_region(fa, 0, size, false);
#else
    size_t off = 0;

    ret = boot_header_scramble_off_sz(fa, slot, &off, &size);
    if (ret < 0) {
        return ret;
    }

    ret = boot_scramble_region(fa, off, size, false);
    if (ret < 0) {
        return ret;
    }

    ret = boot_trailer_scramble_offset(fa, 0, &off);
    if (ret < 0) {
        return ret;
    }

    ret = boot_scramble_region(fa, off, (flash_area_get_size(fa) - off), true);
#endif
    return ret;
}
