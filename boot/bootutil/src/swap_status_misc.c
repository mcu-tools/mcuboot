/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2020 Cypress Semiconductors
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

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "swap_priv.h"
#include "bootutil/bootutil_log.h"

#include "swap_status.h"

#include "mcuboot_config/mcuboot_config.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

#define ERROR_VALUE      (UINT32_MAX)

#if defined(MCUBOOT_SWAP_USING_STATUS)

static int
boot_find_status(int image_index, const struct flash_area **fap);

static int
boot_magic_decode(const union boot_img_magic_t *magic_p)
{
    if (memcmp((const void*)magic_p->val, (const void *)&boot_img_magic.val, BOOT_MAGIC_SZ) == 0) {
        return BOOT_MAGIC_GOOD;
    }
    return BOOT_MAGIC_BAD;
}

static int
boot_flag_decode(uint8_t flag)
{
    if (flag != (uint8_t)BOOT_FLAG_SET) {
        return BOOT_FLAG_BAD;
    }
    return BOOT_FLAG_SET;
}

/* Offset Section */
static inline uint32_t
boot_magic_off(const struct flash_area *fap)
{
    (void)fap;
    return (uint32_t)BOOT_SWAP_STATUS_D_SIZE_RAW - (uint32_t)BOOT_MAGIC_SZ;
}

uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    return boot_magic_off(fap) - BOOT_SWAP_STATUS_IMG_OK_SZ;
}

uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    return boot_image_ok_off(fap) - BOOT_SWAP_STATUS_COPY_DONE_SZ;
}

uint32_t
boot_swap_info_off(const struct flash_area *fap)
{
    return boot_copy_done_off(fap) - BOOT_SWAP_STATUS_SWAPINF_SZ;
}

uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return boot_swap_info_off(fap) - BOOT_SWAP_STATUS_SWAPSZ_SZ;
}

uint32_t
boot_status_off(const struct flash_area *fap)
{
    (void)fap;
    /* this offset is equal to 0, because swap status fields
       in this implementation count from the start of partition */
    return 0;
}

#ifdef MCUBOOT_ENC_IMAGES
/**
 * @returns ERROR_VALUE on error, otherwise result.
 */
static inline uint32_t
boot_enc_key_off(const struct flash_area *fap, uint8_t slot)
{
    uint32_t slot_offset;
    uint32_t res = ERROR_VALUE;
    uint32_t boot_swap_size_offset = boot_swap_size_off(fap);

#ifdef MCUBOOT_SWAP_SAVE_ENCTLV
    /* suggest encryption key is also stored in status partition */
    slot_offset = ((uint32_t)slot + 1UL) * (uint32_t)BOOT_ENC_TLV_SIZE;
#else
    slot_offset = ((uint32_t)slot + 1UL) * (uint32_t)BOOT_ENC_KEY_SIZE;
#endif

    if (boot_swap_size_offset >= slot_offset)
    {
        res = boot_swap_size_offset - slot_offset;
    }
    return res;
}
#endif

/**
 * Write trailer data; status bytes, swap_size, etc
 *
 * @returns 0 on success, -1 on error.
 */
int
boot_write_trailer(const struct flash_area *fap, uint32_t off,
        const uint8_t *inbuf, uint8_t inlen)
{
    int rc;

    /* copy status part trailer to primary image before set copy_done flag */
    if (boot_copy_done_off(fap) == off &&
        fap->fa_id == FLASH_AREA_IMAGE_PRIMARY(0u) &&
        BOOT_SWAP_STATUS_COPY_DONE_SZ == inlen) {

        BOOT_LOG_DBG("copy status part trailer to primary image slot");
        rc = swap_status_to_image_trailer(fap);
        if (rc != 0) {
            BOOT_LOG_ERR("trailer copy failed");
            return -1;
        }
    }

    rc = swap_status_update(fap->fa_id, off, inbuf, inlen);

    if (rc != 0) {
        return -1;
    }
    return rc;
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_write_enc_key(const struct flash_area *fap, uint8_t slot,
        const struct boot_status *bs)
{
    int rc;
    uint32_t off = boot_enc_key_off(fap, slot);
    if (ERROR_VALUE == off)
    {
        return -1;
    }
#ifdef MCUBOOT_SWAP_SAVE_ENCTLV
    rc = swap_status_update(fap->fa_id, off,
                            bs->enctlv[slot], BOOT_ENC_TLV_ALIGN_SIZE);
#else
    rc = swap_status_update(fap->fa_id, off,
                            bs->enckey[slot], BOOT_ENC_KEY_SIZE);
#endif
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int
boot_read_enc_key(int image_index, uint8_t slot, struct boot_status *bs)
{
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    rc = boot_find_status(image_index, &fap);
    if (0 == rc) {
        off = boot_enc_key_off(fap, slot);
        if (ERROR_VALUE == off)
        {
            return -1;
        }
#ifdef MCUBOOT_SWAP_SAVE_ENCTLV
        rc = swap_status_retrieve(fap->fa_id, off, bs->enctlv[slot], BOOT_ENC_TLV_ALIGN_SIZE);
        if (0 == rc) {
            uint8_t aes_iv[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];

            /* Only try to decrypt initialized TLV metadata */
            if (!bootutil_buffer_is_filled(bs->enctlv[slot],
                                           BOOT_UNINITIALIZED_TLV_FILL,
                                           BOOT_ENC_TLV_ALIGN_SIZE)) {
                rc = boot_enc_decrypt(bs->enctlv[slot], bs->enckey[slot], 0, aes_iv);
            }
        }
#else
        rc = swap_status_retrieve(fap->fa_id, off, bs->enckey[slot], BOOT_ENC_KEY_SIZE);
#endif
        flash_area_close(fap);
    }

    return rc;
}
#endif /* MCUBOOT_ENC_IMAGES */

/* Write Section */
int
boot_write_magic(const struct flash_area *fap)
{
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);

    rc = swap_status_update(fap->fa_id, off,
                            (const uint8_t *)&boot_img_magic, BOOT_MAGIC_SZ);

    if (rc != 0) {
        return -1;
    }
    return 0;
}

/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param bs                    The boot status to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(const struct boot_loader_state *state, struct boot_status *bs)
{
    const struct flash_area *fap = NULL;
    uint32_t off;
    uint8_t area_id;
    uint8_t tmp_state;
    int rc;
    (void)state;

    if (bs->idx < BOOT_STATUS_IDX_0) {
        return BOOT_EFLASH;
    }
    /* NOTE: The first sector copied (that is the last sector on slot) contains
     *       the trailer. Since in the last step the primary slot is erased, the
     *       first two status writes go to the scratch which will be copied to
     *       the primary slot!
     */

#ifdef MCUBOOT_SWAP_USING_SCRATCH
    if (bs->use_scratch != 0U) {
        /* Write to scratch status. */
        area_id = (uint8_t)FLASH_AREA_IMAGE_SCRATCH;
    } else
#endif
    {
        /* Write to the primary slot. */
        area_id = FLASH_AREA_IMAGE_PRIMARY(BOOT_CURR_IMG(state));
    }

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }
    off = boot_status_off(fap) + boot_status_internal_off(bs, 1);

    tmp_state = bs->state;

    rc = swap_status_update(fap->fa_id, off, &tmp_state, 1);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

done:
    flash_area_close(fap);

    return rc;
}

int
boot_read_swap_state(const struct flash_area *fap,
                     struct boot_swap_state *state)
{
    union boot_img_magic_t magic = {0U};
    uint32_t off = 0U;
    uint32_t trailer_off = 0U;
    uint8_t swap_info = 0U;
    int rc = 0;
    uint32_t erase_trailer = 0;
    bool buf_is_clean = false;
    bool is_primary = false;
    bool is_secondary = false;
    uint32_t i;

    const struct flash_area *fap_stat = NULL;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    off = boot_magic_off(fap);
    /* retrieve value for magic field from status partition area */
    rc = swap_status_retrieve(fap->fa_id, off, &magic, BOOT_MAGIC_SZ);
    if (rc < 0) {
        return -1;
    }

    for (i = 0u; i < (uint32_t)BOOT_IMAGE_NUMBER; i++) {
        if (fap->fa_id == FLASH_AREA_IMAGE_PRIMARY(i)) {
            is_primary = true;
            break;
        }
        if (fap->fa_id == FLASH_AREA_IMAGE_SECONDARY(i)) {
            is_secondary = true;
            break;
        }
    }

    /* fill magic number value if equal to expected */
    if (bootutil_buffer_is_erased(fap_stat, &magic, BOOT_MAGIC_SZ)) {
        state->magic = BOOT_MAGIC_UNSET;

        /* attempt to find magic in upgrade img slot trailer */
        if (is_secondary) {
            trailer_off = fap->fa_size - BOOT_MAGIC_SZ;

            rc = flash_area_read(fap, trailer_off, &magic, BOOT_MAGIC_SZ);
            if (rc != 0) {
                return -1;
            }
            buf_is_clean = bootutil_buffer_is_erased(fap, &magic, BOOT_MAGIC_SZ);
            if (buf_is_clean) {
                state->magic = BOOT_MAGIC_UNSET;
            } else {
                state->magic = (uint8_t)boot_magic_decode(&magic);
                /* put magic to status partition for upgrade slot*/
                if ((uint32_t)BOOT_MAGIC_GOOD == state->magic) {
                    rc = swap_status_update(fap->fa_id, off,
                                    (uint8_t *)&magic, BOOT_MAGIC_SZ);
                }
                if (rc < 0) {
                    return -1;
                } else {
                    erase_trailer = 1;
                }
            }
        }
    } else {
        state->magic = (uint8_t)boot_magic_decode(&magic);
    }

    off = boot_swap_info_off(fap);
    rc = swap_status_retrieve(fap->fa_id, off, &swap_info, sizeof swap_info);
    if (rc < 0) {
        return -1;
    }
    if (bootutil_buffer_is_erased(fap_stat, &swap_info, sizeof swap_info) ||
        state->swap_type >= (uint8_t)BOOT_SWAP_TYPE_FAIL) {
        state->swap_type = (uint8_t)BOOT_SWAP_TYPE_NONE;
        state->image_num = 0;
    }
    else {
        /* Extract the swap type and image number */
        state->swap_type = (uint8_t)BOOT_GET_SWAP_TYPE_M(swap_info);
        state->image_num = (uint8_t)BOOT_GET_IMAGE_NUM_M(swap_info);
    }

    off = boot_copy_done_off(fap);
    rc = swap_status_retrieve(fap->fa_id, off, &state->copy_done, sizeof state->copy_done);
    if (rc < 0) {
        return -1;
    }
    /* need to check swap_info was empty */
    if (bootutil_buffer_is_erased(fap_stat, &state->copy_done, sizeof state->copy_done)) {
       state->copy_done = BOOT_FLAG_UNSET;
    } else {
       state->copy_done = (uint8_t)boot_flag_decode(state->copy_done);
    }

    off = boot_image_ok_off(fap);
    rc = swap_status_retrieve(fap->fa_id, off, &state->image_ok, sizeof state->image_ok);
    if (rc < 0) {
       return -1;
    }
    /* need to check swap_info was empty */
    if (bootutil_buffer_is_erased(fap_stat, &state->image_ok, sizeof state->image_ok)) {
        /* assign img_ok unset */
        state->image_ok = BOOT_FLAG_UNSET;

        /* attempt to read img_ok value in upgrade img slots trailer area
         * it is set when image in slot for upgrade is signed for swap_type permanent
        */
        bool process_image_ok = (uint8_t)BOOT_FLAG_SET == state->copy_done;
        if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
            BOOT_LOG_DBG(" * selected SCRATCH area, copy_done = %u", (unsigned)state->copy_done);
        }
        else if (is_secondary) {
            process_image_ok = true;
        }
        else if (!is_primary) {
            process_image_ok = false;
            rc = -1;
        }
        else {
            /* Fix MISRA Rule 15.7 */
        }
        if (process_image_ok) {
            trailer_off = fap->fa_size - (uint8_t)BOOT_MAGIC_SZ - (uint8_t)BOOT_MAX_ALIGN;

            rc = flash_area_read(fap, trailer_off, &state->image_ok, sizeof state->image_ok);
            if (rc != 0) {
                return -1;
            }

            buf_is_clean = bootutil_buffer_is_erased(fap, &state->image_ok, sizeof state->image_ok);
            if (buf_is_clean) {
                state->image_ok = BOOT_FLAG_UNSET;
            } else {
                state->image_ok = (uint8_t)boot_flag_decode(state->image_ok);

                /* put img_ok to status partition for upgrade slot */
                if (state->image_ok != (uint8_t)BOOT_FLAG_BAD) {
                    rc = swap_status_update(fap->fa_id, off,
                                &state->image_ok, sizeof state->image_ok);
                }
                if (rc < 0) {
                    return -1;
                }

                /* don't erase trailer, just move img_ok to status part */
                erase_trailer = 0;
            }
        }
    } else {
       state->image_ok = (uint8_t)boot_flag_decode(state->image_ok);
    }

    if ((erase_trailer != 0u) && (fap->fa_id != FLASH_AREA_IMAGE_SCRATCH) && (0 == rc)) {
        /* erase magic from upgrade img trailer */
        rc = flash_area_erase(fap, trailer_off, BOOT_MAGIC_SZ);
        if (rc != 0) {
            return rc;
        }
    }
    return rc;
}

/**
 * This functions tries to locate the status area after an aborted swap,
 * by looking for the magic in the possible locations.
 *
 * If the magic is successfully found, a flash_area * is returned and it
 * is the responsibility of the called to close it.
 *
 * @returns 0 on success, -1 on errors
 */
static int
boot_find_status(int image_index, const struct flash_area **fap)
{
    union boot_img_magic_t magic = {0};
    uint32_t off;
    int rc = -1;
    uint8_t area = FLASH_AREA_ERROR;

    if ((image_index < 0) || (image_index >= MCUBOOT_IMAGE_NUMBER)) {
        return rc;
    }
    /* the status is always in status partition */
    area = FLASH_AREA_IMAGE_PRIMARY((uint32_t)image_index);


    /*
     * In the middle a swap, tries to locate the area that is currently
     * storing a valid magic, first on the primary slot, then on scratch.
     * Both "slots" can end up being temporary storage for a swap and it
     * is assumed that if magic is valid then other metadata is too,
     * because magic is always written in the last step.
     */
    rc = flash_area_open(area, fap);
    if (rc != 0) {
         return rc;
    }
    off = boot_magic_off(*fap);
    rc = swap_status_retrieve(area, off, &magic, BOOT_MAGIC_SZ);

    if (0 == rc) {
        rc = memcmp((const void*)&magic.val, (const void *)&boot_img_magic.val, BOOT_MAGIC_SZ);
    }

    flash_area_close(*fap);
    return rc;
}

int
boot_read_swap_size(int image_index, uint32_t *swap_size)
{
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    rc = boot_find_status(image_index, &fap);
    if (0 == rc) {
        off = boot_swap_size_off(fap);

        rc = swap_status_retrieve(fap->fa_id, off, swap_size, sizeof *swap_size);
    }
    return rc;
}

int
swap_erase_trailer_sectors(const struct boot_loader_state *state,
                           const struct flash_area *fap)
{
    int32_t sub_offs;
    uint32_t trailer_offs;
    uint8_t fa_id_primary;
    uint8_t fa_id_secondary;
    uint8_t image_index;
    int rc;
    (void)state;

    BOOT_LOG_INF("Erasing trailer; fa_id=%u", (unsigned)fap->fa_id);
    /* trailer is located in status-partition */
    const struct flash_area *fap_stat = NULL;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    if (fap->fa_id != FLASH_AREA_IMAGE_SCRATCH) {
        image_index = BOOT_CURR_IMG(state);
        rc = flash_area_id_from_multi_image_slot((int32_t)image_index,
                BOOT_PRIMARY_SLOT);
        if (rc < 0) {
            return -1;
        }
        fa_id_primary = (uint8_t)rc;

        rc = flash_area_id_from_multi_image_slot((int32_t)image_index,
                BOOT_SECONDARY_SLOT);
        if (rc < 0) {
            return -1;
        }
        fa_id_secondary = (uint8_t)rc;

        /* skip if Flash Area is not recognizable */
        if ((fap->fa_id != fa_id_primary) && (fap->fa_id != fa_id_secondary)) {
            return -1;
        }
    }

    sub_offs = swap_status_init_offset(fap->fa_id);
    if (sub_offs < 0) {
        return -1;
    }

    /* delete starting from last sector and moving to beginning */
    /* calculate last sector of status sub-area */
    rc = flash_area_erase(fap_stat, (uint32_t)sub_offs, (uint32_t)BOOT_SWAP_STATUS_SIZE);
    if (rc != 0) {
        return -1;
    }

    if (fap->fa_id != FLASH_AREA_IMAGE_SCRATCH) {
        /*
         * it is also needed to erase trailer area in slots since they may contain
         * data, which is already cleared in corresponding status partition
         */
        trailer_offs = fap->fa_size - BOOT_SWAP_STATUS_TRAILER_SIZE;
        rc = flash_area_erase(fap, trailer_offs, BOOT_SWAP_STATUS_TRAILER_SIZE);
    }

    flash_area_close(fap_stat);

    return rc;
}

int
swap_status_init(const struct boot_loader_state *state,
                 const struct flash_area *fap,
                 const struct boot_status *bs)
{
    struct boot_swap_state swap_state = {0};
    uint8_t image_index;
    int rc;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    image_index = BOOT_CURR_IMG(state);

    BOOT_LOG_DBG("initializing status; fa_id=%u", (unsigned)fap->fa_id);

    rc = boot_read_swap_state_by_id((int32_t)FLASH_AREA_IMAGE_SECONDARY(image_index),
            &swap_state);
    assert(0 == rc);

    if (bs->swap_type != (uint8_t)BOOT_SWAP_TYPE_NONE) {
        rc = boot_write_swap_info(fap, bs->swap_type, image_index);
        assert(0 == rc);
    }

    if ((uint8_t)BOOT_FLAG_SET == swap_state.image_ok) {
        rc = boot_write_image_ok(fap);
        assert(0 == rc);
    }

    rc = boot_write_swap_size(fap, bs->swap_size);
    assert(0 == rc);

#ifdef MCUBOOT_ENC_IMAGES
    rc = boot_write_enc_key(fap, 0, bs);
    assert(0 == rc);

    rc = boot_write_enc_key(fap, 1, bs);
    assert(0 == rc);
#endif

    rc = boot_write_magic(fap);
    assert(0 == rc);

    return rc;
}

int
swap_read_status(struct boot_loader_state *state, struct boot_status *bs)
{
    const struct flash_area *fap = NULL;
    const struct flash_area *fap_stat = NULL;
    uint32_t off;
    uint8_t swap_info = 0;
    uint8_t area_id;
    int rc = 0;

    bs->source = swap_status_source(state);

    if (BOOT_STATUS_SOURCE_NONE == bs->source) {
        return 0;
    }
    else if (BOOT_STATUS_SOURCE_PRIMARY_SLOT == bs->source) {
        area_id = FLASH_AREA_IMAGE_PRIMARY(BOOT_CURR_IMG(state));
    }
    else if (BOOT_STATUS_SOURCE_SCRATCH == bs->source) {
        area_id = FLASH_AREA_IMAGE_SCRATCH;
    }
    else {
        return -1;
    }

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        return -1;
    }

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    rc = swap_read_status_bytes(fap, state, bs);
    if (0 == rc) {
        off = boot_swap_info_off(fap);
        rc = swap_status_retrieve((uint8_t)area_id, off, &swap_info, sizeof swap_info);
        if (rc < 0) {
            return -1;
        }
        if (bootutil_buffer_is_erased(fap_stat, &swap_info, sizeof swap_info)) {
            BOOT_SET_SWAP_INFO_M(swap_info, 0u, (uint8_t)BOOT_SWAP_TYPE_NONE);
            rc = 0;
        }

        /* Extract the swap type info */
        bs->swap_type = BOOT_GET_SWAP_TYPE_M(swap_info);
    }

    flash_area_close(fap);
    flash_area_close(fap_stat);

    return rc;
}

#endif /* MCUBOOT_SWAP_USING_STATUS */
