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
//#include <cstring>
#include <mcuboot_config/mcuboot_logging.h>
#include <mcuboot_config/mcuboot_config.h>
#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>

//#include <bootutil/bootutil_priv.h>


/* mode = 0 for primary slot, 
*  mode = 1 for secondary slot */
int flash_example(uint8_t *buf, uint32_t len, uint8_t mode) {

    if (mode == 0)
    {
        memcpy(MCUBOOT_PRIMARY_SLOT_START_ADDR, buf, len );
    } else {
        memcpy(MCUBOOT_SECONDARY_SLOT_START_ADDR, buf, len );
    }
    
    return(0);
}



static uint32_t* flash_map_bd[FLASH_AREAS] = {
        MCUBOOT_PRIMARY_SLOT_START_ADDR,       /** Primary (loadable) image area */
        MCUBOOT_SECONDARY_SLOT_START_ADDR,     /** Secondary (update candidate) image area */
#if MCUBOOT_SWAP_USING_SCRATCH
        MCUBOOT_SCRATCH_START_ADDR      /** Scratch space for swapping images */
#else
        (uint32_t*) NULL
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
            fap->fa_off = (uint32_t) MCUBOOT_PRIMARY_SLOT_START_ADDR;
            break;
        case SECONDARY_ID:
#if MCUBOOT_DIRECT_XIP
            fap->fa_off = MBED_CONF_MCUBOOT_XIP_SECONDARY_SLOT_ADDRESS;
#else
            fap->fa_off = (uint32_t) MCUBOOT_SECONDARY_SLOT_START_ADDR;
#endif
            break;
#if MCUBOOT_SWAP_USING_SCRATCH
        case SCRATCH_ID:
            fap->fa_off =  (uint32_t) MCUBOOT_SCRATCH_START_ADDR;
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

    uint32_t* bd = flash_map_bd[id];
    fap->fa_size = MCUBOOT_SLOT_SIZE;

    /* Only initialize if this isn't a nested call to open the flash area */
    if (open_count[id] == 1) {
        MCUBOOT_LOG_DBG("initializing flash area %d...", id);
        //memset(bd,0,MCUBOOT_SLOT_SIZE);
        return 0;
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
        //uint32_t* bd = flash_map_bd[id];
    }
}

/*
 * Read/write/erase. Offset is relative from beginning of flash area.
 */
int flash_area_read(const struct flash_area* fap, uint32_t off, void* dst, uint32_t len) {
     uint32_t* bd = flash_map_bd[fap->fa_id];
     uint32_t *start = bd + off / sizeof(uint32_t);

     //memcpy(dst,bd + ( off / sizeof(uint32_t) ),len);
     memcpy(dst,start,len); // bd + off;
    return 0;
}

int flash_area_write(const struct flash_area* fap, uint32_t off, const void* src, uint32_t len) {
    uint32_t* bd = flash_map_bd[fap->fa_id];
     uint32_t *dst = bd + off / sizeof(uint32_t);

    if (bd+off+len > bd + MCUBOOT_SLOT_SIZE) return -1;

    memcpy(dst, src, len);
    return 0;
}

int flash_area_erase(const struct flash_area* fap, uint32_t off, uint32_t len) {
    uint32_t* bd = flash_map_bd[fap->fa_id];
    uint32_t *start = bd + off / sizeof(uint32_t);
    uint32_t* max = start + len / sizeof(uint32_t);

    if (max > bd + MCUBOOT_SLOT_SIZE) return -1;

    memset(start,0,len);
    return 0;
}

uint8_t flash_area_align(const struct flash_area* fap) {
    uint32_t* bd = flash_map_bd[fap->fa_id];
    // bd->get_program_size();
    return 1;

}

uint8_t flash_area_erased_val(const struct flash_area* fap) {
    uint32_t* bd = flash_map_bd[fap->fa_id];
    // bd->get_erase_value();
    return 0;
}

int flash_area_get_sectors(int fa_id, uint32_t* count, struct flash_sector* sectors) {
    uint32_t* bd = flash_map_bd[fa_id];

    // Loop through sectors and collect information on them
    
    uint32_t offset = 0;
    
    //*count = 1;
    uint32_t erase_size;
    erase_size = MCUBOOT_ERASE_SIZE; // bd->get_erase_size(offset);

    *count = 0;
    while (*count < MCUBOOT_MAX_IMG_SECTORS) {

        sectors[*count].fs_off = offset;
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
