/*
 * Copyright (c) 2020 Embedded Planet
 * Copyright (c) 2020 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include <assert.h>
#include <cstring>
#include "flash_map_backend/flash_map_backend.h"
#include "flash_map_backend/secondary_bd.h"
#include "sysflash/sysflash.h"

#include "blockdevice/BlockDevice.h"
#include "FlashIAP/FlashIAPBlockDevice.h"

#include "mcuboot_config/mcuboot_logging.h"

#include "bootutil_priv.h"

#define FLASH_DEVICE_INTERNAL_FLASH 0
#define FLASH_AREAS 3

/** Application defined secondary block device */
mbed::BlockDevice* mcuboot_secondary_bd = get_secondary_bd();

/** Internal application block device */
static FlashIAPBlockDevice mcuboot_primary_bd(MCUBOOT_PRIMARY_SLOT_START_ADDR, MCUBOOT_SLOT_SIZE);

#if MCUBOOT_SWAP_USING_SCRATCH
/** Scratch space is at the end of internal flash, after the main application */
static FlashIAPBlockDevice mcuboot_scratch_bd(MCUBOOT_SCRATCH_START_ADDR, MCUBOOT_SCRATCH_SIZE);
#endif

static mbed::BlockDevice* flash_map_bd[FLASH_AREAS] = {
        (mbed::BlockDevice*) &mcuboot_primary_bd,       /** Primary (loadable) image area */
        mcuboot_secondary_bd,                           /** Secondary (update candidate) image area */
#if MCUBOOT_SWAP_USING_SCRATCH
        (mbed::BlockDevice*) &mcuboot_scratch_bd        /** Scratch space for swapping images */
#else
        nullptr
#endif
};

static struct flash_area flash_areas[FLASH_AREAS];

static unsigned int open_count[FLASH_AREAS] = {0};

int flash_area_open(uint8_t id, const struct flash_area** fapp) {

    *fapp = &flash_areas[id];
    struct flash_area* fap = (struct flash_area*)*fapp;

    // The offset of the slot is from the beginning of the flash device.
    switch (id) {
        case PRIMARY_ID:
            fap->fa_off = MCUBOOT_PRIMARY_SLOT_START_ADDR;
            break;
        case SECONDARY_ID:
#if MCUBOOT_DIRECT_XIP
            fap->fa_off = MBED_CONF_MCUBOOT_XIP_SECONDARY_SLOT_ADDRESS;
#else
            fap->fa_off = 0;
#endif
            break;
#if MCUBOOT_SWAP_USING_SCRATCH
        case SCRATCH_ID:
            fap->fa_off = MCUBOOT_SCRATCH_START_ADDR;
            break;
#endif
        default:
            MCUBOOT_LOG_ERR("flash_area_open, unknown id %d", id);
            return -1;
    }

    open_count[id]++;
    MCUBOOT_LOG_DBG("flash area %d open count: %d (+)", id, open_count[id]);

    fap->fa_id = id;
    fap->fa_device_id = 0; // not relevant

    mbed::BlockDevice* bd = flash_map_bd[id];
    fap->fa_size = (uint32_t) bd->size();

    /* Only initialize if this isn't a nested call to open the flash area */
    if (open_count[id] == 1) {
        MCUBOOT_LOG_DBG("initializing flash area %d...", id);
        return bd->init();
    } else {
        return 0;
    }
}

void flash_area_close(const struct flash_area* fap) {
    uint8_t id = fap->fa_id;
    /* No need to close an unopened flash area, avoid an overflow of the counter */
    if (!open_count[id]) {
        return;
    }

    open_count[id]--;
    MCUBOOT_LOG_DBG("flash area %d open count: %d (-)", id, open_count[id]);
    if (!open_count[id]) {
        /* mcuboot is not currently consistent in opening/closing flash areas only once at a time
         * so only deinitialize the BlockDevice if all callers have closed the flash area. */
        MCUBOOT_LOG_DBG("deinitializing flash area block device %d...", id);
        mbed::BlockDevice* bd = flash_map_bd[id];
        bd->deinit();
    }
}

/*
 * Read/write/erase. Offset is relative from beginning of flash area.
 */
int flash_area_read(const struct flash_area* fap, uint32_t off, void* dst, uint32_t len) {
    mbed::BlockDevice* bd = flash_map_bd[fap->fa_id];

    /* Note: The address must be aligned to bd->get_read_size(). If MCUBOOT_READ_GRANULARITY
       is defined, the length does not need to be aligned. */
#ifdef MCUBOOT_READ_GRANULARITY
    uint32_t read_size = bd->get_read_size();
    if (read_size == 0) {
        MCUBOOT_LOG_ERR("Invalid read size: must be non-zero");
        return -1;
    }
    if (MCUBOOT_READ_GRANULARITY < read_size) {
        MCUBOOT_LOG_ERR("Please increase MCUBOOT_READ_GRANULARITY (currently %u) to be at least %u",
                    MCUBOOT_READ_GRANULARITY, read_size);
        return -1;
    }

    uint32_t remainder = len % read_size;
    len -= remainder;
    if (len != 0) {
#endif
        if (!bd->is_valid_read(off, len)) {
            MCUBOOT_LOG_ERR("Invalid read: fa_id %d offset 0x%x len 0x%x", fap->fa_id,
                    (unsigned int) off, (unsigned int) len);
            return -1;
        }
        else {
            int ret = bd->read(dst, off, len);
            if (ret != 0) {
                MCUBOOT_LOG_ERR("Read failed: fa_id %d offset 0x%x len 0x%x", fap->fa_id,
                        (unsigned int) off, (unsigned int) len);
                return ret;
            }
        }
#ifdef MCUBOOT_READ_GRANULARITY
    }

    if (remainder) {
        if (!bd->is_valid_read(off + len, read_size)) {
            MCUBOOT_LOG_ERR("Invalid read: fa_id %d offset 0x%x len 0x%x", fap->fa_id,
                    (unsigned int) (off + len), (unsigned int) read_size);
            return -1;
        }
        else {
            uint8_t buffer[MCUBOOT_READ_GRANULARITY];
            int ret = bd->read(buffer, off + len, read_size);
            if (ret != 0) {
                MCUBOOT_LOG_ERR("Read failed: %d", ret);
                return ret;
            }
            memcpy((uint8_t *)dst + len, buffer, remainder);
        }
    }
#endif

    return 0;
}

int flash_area_write(const struct flash_area* fap, uint32_t off, const void* src, uint32_t len) {
    mbed::BlockDevice* bd = flash_map_bd[fap->fa_id];
    return bd->program(src, off, len);
}

int flash_area_erase(const struct flash_area* fap, uint32_t off, uint32_t len) {
    mbed::BlockDevice* bd = flash_map_bd[fap->fa_id];
    return bd->erase(off, len);
}

uint8_t flash_area_align(const struct flash_area* fap) {
    mbed::BlockDevice* bd = flash_map_bd[fap->fa_id];
    return bd->get_program_size();
}

uint8_t flash_area_erased_val(const struct flash_area* fap) {
    mbed::BlockDevice* bd = flash_map_bd[fap->fa_id];

    int erased_val = bd->get_erase_value();

    /**
     * If the BlockDevice returns -1 for the get_erase_value (IMPORTANT: distinct from byte value 0xFF),
     * this means the erase value is unreliable and cannot be used. MCUboot does not support
     * such BlockDevices. Some internal checks rely on comparison to a known erase value.
     *
     * Wrap this BlockDevice in a FlashSimBlockDevice to provide a known erase value and ensure
     * compatibility with MCUboot.
     */
    assert(erased_val >= 0);

    return (uint8_t) erased_val;
}

int flash_area_get_sectors(int fa_id, uint32_t* count, struct flash_sector* sectors) {
    mbed::BlockDevice* bd = flash_map_bd[fa_id];

    /* Loop through sectors and collect information on them */
    bd_addr_t offset = 0;
    *count = 0;
    while (*count < MCUBOOT_MAX_IMG_SECTORS && bd->is_valid_read(offset, bd->get_read_size())) {

        sectors[*count].fs_off = offset;
        bd_size_t erase_size = bd->get_erase_size(offset);
        sectors[*count].fs_size = erase_size;

        offset += erase_size;
        *count += 1;
    }

    return 0;
}

int flash_area_id_from_image_slot(int slot) {
    return slot;
}

int flash_area_id_to_image_slot(int area_id) {
    return area_id;
}

/**
 * Multi images support not implemented yet
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    assert(image_index == 0);
    return slot;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    assert(image_index == 0);
    return area_id;
}
