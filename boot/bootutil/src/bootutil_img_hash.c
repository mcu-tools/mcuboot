/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2025 Arm Limited
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

#include <stdint.h>
#include <inttypes.h>
#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/crypto/sha.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifndef MCUBOOT_SIGN_PURE
/*
 * Compute SHA hash over the image.
 * (SHA384 if ECDSA-P384 is being used,
 *  SHA256 otherwise).
 */
int
bootutil_img_hash(struct boot_loader_state *state,
                  struct image_header *hdr, const struct flash_area *fap,
                  uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result,
                  uint8_t *seed, int seed_len
                 )
{
    bootutil_sha_context sha_ctx;
    uint32_t size;
    uint16_t hdr_size;
    uint32_t blk_off;
    uint32_t tlv_off;
#if !defined(MCUBOOT_HASH_STORAGE_DIRECTLY)
    int rc;
    uint32_t off;
    uint32_t blk_sz;
#endif
#ifdef MCUBOOT_HASH_STORAGE_DIRECTLY
    uintptr_t base = 0;
    int fa_ret;
#endif
#if defined(MCUBOOT_ENC_IMAGES)
    int image_index;
#endif
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    uint32_t sector_off = 0;
#endif

#if (BOOT_IMAGE_NUMBER == 1) || !defined(MCUBOOT_ENC_IMAGES) || \
    defined(MCUBOOT_RAM_LOAD)
    (void)state;
    (void)hdr_size;
    (void)blk_off;
    (void)tlv_off;
#ifdef MCUBOOT_RAM_LOAD
    (void)blk_sz;
    (void)off;
    (void)rc;
    (void)fap;
    (void)tmp_buf;
    (void)tmp_buf_sz;
#endif
#endif
    BOOT_LOG_DBG("bootutil_img_hash");

#ifdef MCUBOOT_ENC_IMAGES
    if (state == NULL) {
        image_index = 0;
    } else {
        image_index = BOOT_CURR_IMG(state);
    }

    /* Encrypted images only exist in the secondary slot */
    if (MUST_DECRYPT(fap, image_index, hdr) &&
            !boot_enc_valid(BOOT_CURR_ENC_SLOT(state, BOOT_SLOT_SECONDARY))) {
        BOOT_LOG_DBG("bootutil_img_hash: error encrypted image found in primary slot");
        return -1;
    }
#endif

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    /* For swap using offset mode, the image starts in the second sector of the upgrade slot, so
     * apply the offset when this is needed
     */
    sector_off = boot_get_state_secondary_offset(state, fap);
#endif

    bootutil_sha_init(&sha_ctx);

    /* in some cases (split image) the hash is seeded with data from
     * the loader image */
    if (seed && (seed_len > 0)) {
        bootutil_sha_update(&sha_ctx, seed, seed_len);
    }

    /* Hash is computed over image header and image itself. */
    size = hdr_size = hdr->ih_hdr_size;
    size += hdr->ih_img_size;
    tlv_off = size;

    /* If protected TLVs are present they are also hashed. */
    size += hdr->ih_protect_tlv_size;

#ifdef MCUBOOT_HASH_STORAGE_DIRECTLY
    /* No chunk loading, storage is mapped to address space and can
     * be directly given to hashing function.
     */
    fa_ret = flash_device_base(flash_area_get_device_id(fap), &base);
    if (fa_ret != 0) {
        base = 0;
    }

    bootutil_sha_update(&sha_ctx, (void *)(base + flash_area_get_off(fap)), size);
#else /* MCUBOOT_HASH_STORAGE_DIRECTLY */
#ifdef MCUBOOT_RAM_LOAD
    bootutil_sha_update(&sha_ctx,
                        (void*)(IMAGE_RAM_BASE + hdr->ih_load_addr),
                        size);
#else
    for (off = 0; off < size; off += blk_sz) {
        blk_sz = size - off;
        if (blk_sz > tmp_buf_sz) {
            blk_sz = tmp_buf_sz;
        }
#ifdef MCUBOOT_ENC_IMAGES
        /* The only data that is encrypted in an image is the payload;
         * both header and TLVs (when protected) are not.
         */
        if ((off < hdr_size) && ((off + blk_sz) > hdr_size)) {
            /* read only the header */
            blk_sz = hdr_size - off;
        }
        if ((off < tlv_off) && ((off + blk_sz) > tlv_off)) {
            /* read only up to the end of the image payload */
            blk_sz = tlv_off - off;
        }
#endif
#if defined(MCUBOOT_SWAP_USING_OFFSET)
        rc = flash_area_read(fap, off + sector_off, tmp_buf, blk_sz);
#else
        rc = flash_area_read(fap, off, tmp_buf, blk_sz);
#endif
        if (rc) {
            bootutil_sha_drop(&sha_ctx);
            BOOT_LOG_DBG("bootutil_img_validate Error %d reading data chunk "
                         "%p %" PRIu32 " %" PRIu32,
                         rc, fap, off, blk_sz);
            return rc;
        }
#ifdef MCUBOOT_ENC_IMAGES
        if (MUST_DECRYPT(fap, image_index, hdr)) {
            /* Only payload is encrypted (area between header and TLVs) */
            int slot = flash_area_id_to_multi_image_slot(image_index,
                            flash_area_get_id(fap));

            if (off >= hdr_size && off < tlv_off) {
                blk_off = (off - hdr_size) & 0xf;
                boot_enc_decrypt(BOOT_CURR_ENC_SLOT(state, slot), off - hdr_size,
                                 blk_sz, blk_off, tmp_buf);
            }
        }
#endif
        bootutil_sha_update(&sha_ctx, tmp_buf, blk_sz);
    }
#endif /* MCUBOOT_RAM_LOAD */
#endif /* MCUBOOT_HASH_STORAGE_DIRECTLY */
    bootutil_sha_finish(&sha_ctx, hash_result);
    bootutil_sha_drop(&sha_ctx);

    return 0;
}
#endif /* !MCUBOOT_SIGN_PURE */
