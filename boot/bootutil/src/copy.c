/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 JUUL Labs
 * Copyright (c) 2024 Elektroline Inc.
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
#include "bootutil/bootutil_log.h"
#include "copy_priv.h"

#include "mcuboot_config/mcuboot_config.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_COPY_WITH_REVERT

static inline bool
boot_data_is_set_to(uint8_t val, void *data, size_t len)
{
    uint8_t i;
    uint8_t *p = (uint8_t *)data;
    for (i = 0; i < len; i++) {
        if (val != p[i]) {
            return false;
        }
    }
    return true;
}

static int
boot_check_header_erased(struct boot_loader_state *state, int slot)
{
    const struct flash_area *fap;
    struct image_header *hdr;
    uint8_t erased_val;
    int area_id;
    int rc;

    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        return -1;
    }

    erased_val = flash_area_erased_val(fap);
    flash_area_close(fap);

    hdr = boot_img_hdr(state, slot);
    if (!boot_data_is_set_to(erased_val, &hdr->ih_magic, sizeof(hdr->ih_magic))) {
        return -1;
    }

    return 0;
}

int
boot_read_image_header(struct boot_loader_state *state, int slot,
                       struct image_header *out_hdr, struct boot_status *bs)
{
    const struct flash_area *fap;
    int area_id;
    int rc = 0;

    (void)bs;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);

    rc = flash_area_open(area_id, &fap);
    if (rc == 0) {
        rc = flash_area_read(fap, 0, out_hdr, sizeof *out_hdr);
        flash_area_close(fap);
    }

    if (rc != 0) {
        rc = BOOT_EFLASH;
    }

    return rc;
}

bool copy_compare_hash(uint8_t *hash1, uint8_t *hash2)
{
    for (int i = 0; i < 32; i++) {
        if (hash1[i] != hash2[i]) {
            return false;
        }
    }

    return true;
}

int
copy_get_slot_type(struct boot_loader_state *state)
{
    uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    struct image_header *hdr;
    struct image_header *primary_header;
    uint8_t image_index;
    uint8_t primary_hash[32] = {0};
    uint8_t secondary_hash[32] = {0};
    uint8_t tertiary_hash[32] = {0};
    struct boot_swap_state secondary_slot;
    struct boot_swap_state tertiary_slot;
    struct boot_copy_state *copy_state;
    int primary;
    int secondary;
    int tertiary;
    int rc;

    image_index = BOOT_CURR_IMG(state);
    copy_state = &state->copy[image_index];
    primary = FLASH_AREA_IMAGE_PRIMARY(image_index);
    secondary = FLASH_AREA_IMAGE_SECONDARY(image_index);
    tertiary = FLASH_AREA_IMAGE_TERTIARY(image_index);

    /* Set initial values. Update = secondary, recovery = tertiary  */
    copy_state->update = secondary;
    copy_state->recovery = tertiary;
    copy_state->recovery_valid = false;

    primary_header = boot_img_hdr(state, primary);
    rc = bootutil_img_validate(BOOT_CURR_ENC(state), image_index,
                primary_header, BOOT_IMG_AREA(state, primary),
                tmpbuf, BOOT_TMPBUF_SZ, NULL, 0, primary_hash);

    hdr = boot_img_hdr(state, secondary);
    if (boot_check_header_erased(state, secondary) == 0 ||
        (hdr->ih_flags & IMAGE_F_NON_BOOTABLE)) {
    } else {
        bootutil_img_validate(BOOT_CURR_ENC(state), image_index,
                    hdr, BOOT_IMG_AREA(state, secondary), tmpbuf,
                    BOOT_TMPBUF_SZ, NULL, 0, secondary_hash);
    }

    hdr = boot_img_hdr(state, tertiary);
    if (boot_check_header_erased(state, tertiary) == 0 ||
        (hdr->ih_flags & IMAGE_F_NON_BOOTABLE)) {
    } else {
        rc = bootutil_img_validate(BOOT_CURR_ENC(state), image_index,
                    hdr, BOOT_IMG_AREA(state, tertiary), tmpbuf,
                    BOOT_TMPBUF_SZ, NULL, 0, tertiary_hash);
    }

    rc = boot_read_swap_state_by_id(secondary, &secondary_slot);
    if (rc == BOOT_EFLASH) {
        BOOT_LOG_INF("Secondary image of image pair"
                     "is unreachable. Treat it as empty");
        secondary_slot.magic = BOOT_MAGIC_UNSET;
        secondary_slot.swap_type = BOOT_SWAP_TYPE_NONE;
        secondary_slot.copy_done = BOOT_FLAG_UNSET;
        secondary_slot.image_ok = BOOT_FLAG_UNSET;
        secondary_slot.image_num = 0;
    } else if (rc) {
        return rc;
    }

    rc = boot_read_swap_state_by_id(tertiary, &tertiary_slot);
    if (rc == BOOT_EFLASH) {
        BOOT_LOG_INF("Secondary image of image pair"
                     "is unreachable. Treat it as empty");
        tertiary_slot.magic = BOOT_MAGIC_UNSET;
        tertiary_slot.swap_type = BOOT_SWAP_TYPE_NONE;
        tertiary_slot.copy_done = BOOT_FLAG_UNSET;
        tertiary_slot.image_ok = BOOT_FLAG_UNSET;
        tertiary_slot.image_num = 0;
    } else if (rc) {
        return rc;
    }

    if (secondary_slot.image_ok == BOOT_FLAG_SET) {
        copy_state->recovery = secondary;
        copy_state->update = tertiary;
        if (copy_compare_hash(primary_hash, secondary_hash)) {
            copy_state->recovery_valid = true;
        }
    } else if (tertiary_slot.image_ok == BOOT_FLAG_SET) {
        copy_state->recovery = tertiary;
        copy_state->update = secondary;
        if (copy_compare_hash(primary_hash, tertiary_hash)) {
            copy_state->recovery_valid = true;
        }
    }

    return 0;
}

uint32_t
boot_status_internal_off(const struct boot_status *bs, int elem_sz)
{
    int idx_sz;

    idx_sz = elem_sz * BOOT_STATUS_STATE_COUNT;

    return (bs->idx - BOOT_STATUS_IDX_0) * idx_sz +
           (bs->state - BOOT_STATUS_STATE_0) * elem_sz;
}

/*
 * We basically do just overwrite with this algorithm therefor slots are
 * compatible when they fit into each other.
 */
int
boot_slots_compatible(struct boot_loader_state *state)
{
    size_t num_sectors_primary;
    size_t num_sectors_secondary;
    size_t num_sectors_tertiary;
    size_t primary_size;
    size_t secondary_size;
    size_t tertiary_size;

    /* Basic check whether sectors do not exceed maximum img sectors. */
    num_sectors_primary = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT);
    num_sectors_secondary = boot_img_num_sectors(state, BOOT_SECONDARY_SLOT);
    num_sectors_tertiary = boot_img_num_sectors(state, BOOT_TERTIARY_SLOT);
    if ((num_sectors_primary > BOOT_MAX_IMG_SECTORS) ||
        (num_sectors_secondary > BOOT_MAX_IMG_SECTORS) ||
        (num_sectors_tertiary > BOOT_MAX_IMG_SECTORS)) {
        BOOT_LOG_WRN("Cannot upgrade: more sectors than allowed");
        return 0;
    }

    /* Check whether slot sizes are equal. */
    primary_size = BOOT_IMG_AREA(state, BOOT_PRIMARY_SLOT)->fa_size;
    secondary_size = BOOT_IMG_AREA(state, BOOT_SECONDARY_SLOT)->fa_size;
    tertiary_size = BOOT_IMG_AREA(state, BOOT_TERTIARY_SLOT)->fa_size;
    if ((primary_size != secondary_size) ||
        (primary_size != tertiary_size) ||
        (secondary_size != tertiary_size)) {
        BOOT_LOG_WRN("Cannot upgrade: slots sizes are not equal");
        return 0;
    }

    return 1;
}

#endif /* MCUBOOT_COPY_WITH_REVERT */
