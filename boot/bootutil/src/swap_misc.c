/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 JUUL Labs
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
 * limitations under the License.
 */

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "swap_priv.h"
#include "bootutil/bootutil_log.h"

#include "mcuboot_config/mcuboot_config.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#if defined(MCUBOOT_SWAP_USING_SCRATCH) || defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET)
int
swap_erase_trailer_sectors(const struct boot_loader_state *state,
                           const struct flash_area *fap)
{
    int rc = 0;

    /* Intention is to prepare slot for write, if device does not require/support
     * erase, there is nothing to do here.
     */
    if (device_requires_erase(fap)) {
        uint8_t slot = BOOT_PRIMARY_SLOT;
        uint32_t sector;
        uint32_t trailer_sz;
        uint32_t total_sz;

        BOOT_LOG_DBG("Erasing trailer; fa_id=%d", flash_area_get_id(fap));

        /* By default it is assumed that slot is primary */
        if (fap == BOOT_IMG_AREA(state, BOOT_SECONDARY_SLOT)) {
            slot = BOOT_SECONDARY_SLOT;
        }

        /* Delete starting from last sector and moving to beginning */
        sector = boot_img_num_sectors(state, slot) - 1;
        trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));
        total_sz = 0;
        do {
            uint32_t sz = boot_img_sector_size(state, slot, sector);
            uint32_t off = boot_img_sector_off(state, slot, sector);

            rc = boot_erase_region(fap, off, sz, false);
            assert(rc == 0);

            sector--;
            total_sz += sz;
        } while (total_sz < trailer_sz);
    } else {
        BOOT_LOG_DBG("Erasing trailer not required; fa_id=%d", flash_area_get_id(fap));
    }
    return rc;
}

int
swap_scramble_trailer_sectors(const struct boot_loader_state *state,
                              const struct flash_area *fap)
{
    size_t off;
    int rc;

    BOOT_LOG_DBG("Scrambling trailer; fa_id=%d", flash_area_get_id(fap));

    /* Delete starting from last sector and moving to beginning */
    rc = boot_trailer_scramble_offset(fap, BOOT_WRITE_SZ(state), &off);
    if (rc < 0) {
        return BOOT_EFLASH;
    }
    rc = boot_scramble_region(fap, off, (flash_area_get_size(fap) - off), true);
    if (rc < 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

/* NOTE: There is often call made to swap_scramble_trailer_sectors followed
 * by swap_status_init to initialize swap status: this is not efficient on
 * devices that do not require erase; we need implementation of swap_status_init
 * or swap_status_reinit, that will remove old status and initialize new one
 * in a single call; the current approach writes certain parts of status
 * twice on these devices.
 */
int
swap_status_init(const struct boot_loader_state *state,
                 const struct flash_area *fap,
                 const struct boot_status *bs)
{
    struct boot_swap_state swap_state;
    uint8_t image_index;
    int rc;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    image_index = BOOT_CURR_IMG(state);

    BOOT_LOG_DBG("initializing status; fa_id=%d", flash_area_get_id(fap));

    rc = boot_read_swap_state(state->imgs[image_index][BOOT_SECONDARY_SLOT].area,
                              &swap_state);
    assert(rc == 0);

    if (bs->swap_type != BOOT_SWAP_TYPE_NONE) {
        rc = boot_write_swap_info(fap, bs->swap_type, image_index);
        assert(rc == 0);
    }

    if (swap_state.image_ok == BOOT_FLAG_SET) {
        rc = boot_write_image_ok(fap);
        assert(rc == 0);
    }

    rc = boot_write_swap_size(fap, bs->swap_size);
    assert(rc == 0);

#ifdef MCUBOOT_ENC_IMAGES
    rc = boot_write_enc_key(fap, 0, bs);
    assert(rc == 0);

    rc = boot_write_enc_key(fap, 1, bs);
    assert(rc == 0);
#endif

    rc = boot_write_magic(fap);
    assert(rc == 0);

    return 0;
}

int
swap_read_status(struct boot_loader_state *state, struct boot_status *bs)
{
    const struct flash_area *fap;
    uint32_t off;
    uint8_t swap_info;
    int rc;

    bs->source = swap_status_source(state);
    switch (bs->source) {
    case BOOT_STATUS_SOURCE_NONE:
        return 0;

#if MCUBOOT_SWAP_USING_SCRATCH
    case BOOT_STATUS_SOURCE_SCRATCH:
        fap = state->scratch.area;
        break;
#endif

    case BOOT_STATUS_SOURCE_PRIMARY_SLOT:
        fap = BOOT_IMG_AREA(state, BOOT_PRIMARY_SLOT);
        break;

    default:
        assert(0);
        return BOOT_EBADARGS;
    }

    assert(fap != NULL);

    rc = swap_read_status_bytes(fap, state, bs);
    if (rc == 0) {
        off = boot_swap_info_off(fap);
        rc = flash_area_read(fap, off, &swap_info, sizeof swap_info);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }

        if (bootutil_buffer_is_erased(fap, &swap_info, sizeof swap_info)) {
            BOOT_SET_SWAP_INFO(swap_info, 0, BOOT_SWAP_TYPE_NONE);
            rc = 0;
        }

        /* Extract the swap type info */
        bs->swap_type = BOOT_GET_SWAP_TYPE(swap_info);
    }

done:
    return rc;
}

int
swap_set_copy_done(uint8_t image_index)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(image_index),
            &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_write_copy_done(fap);
    flash_area_close(fap);
    return rc;
}

int
swap_set_image_ok(uint8_t image_index)
{
    const struct flash_area *fap;
    struct boot_swap_state state;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(image_index),
            &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_read_swap_state(fap, &state);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto out;
    }

    if (state.image_ok == BOOT_FLAG_UNSET) {
        rc = boot_write_image_ok(fap);
    }

out:
    flash_area_close(fap);
    return rc;
}


#endif /* defined(MCUBOOT_SWAP_USING_SCRATCH) || defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) */
