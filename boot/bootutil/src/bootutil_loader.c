/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016-2020 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2023 Arm Limited
 * Copyright (c) 2024-2025 Nordic Semiconductor ASA
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

/**
 * This file provides an interface to the manifest-based boot loader.
 * Functions defined in this file should only be called while the boot loader is
 * running.
 */

#include "bootutil_loader.h"
#include "bootutil/boot_record.h"
#include "bootutil/boot_hooks.h"
#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif
#ifdef MCUBOOT_HW_ROLLBACK_PROT
#include "bootutil/security_cnt.h"
#endif
#if defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) || \
    defined(MCUBOOT_SWAP_USING_SCRATCH)
#include "swap_priv.h"
#endif
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

bool
boot_check_header_erased(struct boot_loader_state *state, int slot)
{
    const struct flash_area *fap = NULL;
    struct image_header *hdr;

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    hdr = boot_img_hdr(state, slot);
    if (bootutil_buffer_is_erased(fap, &hdr->ih_magic, sizeof(hdr->ih_magic))) {
        return true;
    }

    return false;
}

bool
boot_check_header_valid(struct boot_loader_state *state, int slot)
{
    const struct flash_area *fap = NULL;
    struct image_header *hdr;
    uint32_t size;

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    hdr = boot_img_hdr(state, slot);
    if (hdr->ih_magic != IMAGE_MAGIC) {
        return false;
    }

    if (!boot_u32_safe_add(&size, hdr->ih_img_size, hdr->ih_hdr_size)) {
        return false;
    }

#ifdef MCUBOOT_DECOMPRESS_IMAGES
    if (!MUST_DECOMPRESS(fap, BOOT_CURR_IMG(state), hdr)) {
#else
    if (1) {
#endif
        if (!boot_u32_safe_add(&size, size, hdr->ih_protect_tlv_size)) {
            return false;
        }
    }

    if (size >= flash_area_get_size(fap)) {
        return false;
    }

#if !defined(MCUBOOT_ENC_IMAGES)
    if (IS_ENCRYPTED(hdr)) {
        return false;
    }
#else
    if ((hdr->ih_flags & IMAGE_F_ENCRYPTED_AES128) &&
        (hdr->ih_flags & IMAGE_F_ENCRYPTED_AES256))
    {
        return false;
    }
#endif

#if !defined(MCUBOOT_DECOMPRESS_IMAGES)
    if (IS_COMPRESSED(hdr)) {
        return false;
    }
#else
    if ((hdr->ih_flags & IMAGE_F_COMPRESSED_LZMA1) &&
        (hdr->ih_flags & IMAGE_F_COMPRESSED_LZMA2))
    {
        return false;
    }
#endif

    return true;
}

int
boot_read_image_headers(struct boot_loader_state *state, bool require_all, struct boot_status *bs)
{
    int rc;
    int i;

    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        rc = BOOT_HOOK_CALL(boot_read_image_header_hook, BOOT_HOOK_REGULAR,
                            BOOT_CURR_IMG(state), i, boot_img_hdr(state, i));
        if (rc == BOOT_HOOK_REGULAR)
        {
            rc = boot_read_image_header(state, i, boot_img_hdr(state, i), bs);
        }
        if (rc != 0) {
            /* If `require_all` is set, fail on any single fail, otherwise
             * if at least the first slot's header was read successfully,
             * then the boot loader can attempt a boot.
             *
             * Failure to read any headers is a fatal error.
             */
            if (i > 0 && !require_all) {
                return 0;
            } else {
                return rc;
            }
        }
    }

    return 0;
}

fih_ret
boot_check_image(struct boot_loader_state *state, struct boot_status *bs, int slot)
{
    TARGET_STATIC uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    const struct flash_area *fap = NULL;
    struct image_header *hdr;

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    hdr = boot_img_hdr(state, slot);

    (void)bs;
    (void)rc;

    /* In the case of ram loading the image has already been decrypted as it is
     * decrypted when copied in ram
     */
#if defined(MCUBOOT_ENC_IMAGES) && !defined(MCUBOOT_RAM_LOAD)
    if (MUST_DECRYPT(fap, BOOT_CURR_IMG(state), hdr)) {
        rc = boot_enc_load(state, BOOT_SLOT_SECONDARY, hdr, fap, bs);
        if (rc < 0) {
            FIH_RET(fih_rc);
        }
        if (rc == 0 && boot_enc_set_key(BOOT_CURR_ENC_SLOT(state, BOOT_SLOT_SECONDARY),
                                        bs->enckey[BOOT_SLOT_SECONDARY])) {
            FIH_RET(fih_rc);
        }
    }
#endif

    FIH_CALL(bootutil_img_validate, fih_rc, state, hdr, fap, tmpbuf, BOOT_TMPBUF_SZ,
             NULL, 0, NULL);

    FIH_RET(fih_rc);
}

int
boot_compare_version(const struct image_version *ver1, const struct image_version *ver2)
{
#if !defined(MCUBOOT_VERSION_CMP_USE_BUILD_NUMBER)
    BOOT_LOG_DBG("boot_version_cmp: ver1 %u.%u.%u vs ver2 %u.%u.%u",
                 (unsigned)ver1->iv_major, (unsigned)ver1->iv_minor,
                 (unsigned)ver1->iv_revision, (unsigned)ver2->iv_major,
                 (unsigned)ver2->iv_minor, (unsigned)ver2->iv_revision);
#else
    BOOT_LOG_DBG("boot_version_cmp: ver1 %u.%u.%u.%u vs ver2 %u.%u.%u.%u",
                 (unsigned)ver1->iv_major, (unsigned)ver1->iv_minor,
                 (unsigned)ver1->iv_revision, (unsigned)ver1->iv_build_num,
                 (unsigned)ver2->iv_major, (unsigned)ver2->iv_minor,
                 (unsigned)ver2->iv_revision, (unsigned)ver2->iv_build_num);
#endif

    if (ver1->iv_major > ver2->iv_major) {
        return 1;
    }
    if (ver1->iv_major < ver2->iv_major) {
        return -1;
    }
    /* The major version numbers are equal, continue comparison. */
    if (ver1->iv_minor > ver2->iv_minor) {
        return 1;
    }
    if (ver1->iv_minor < ver2->iv_minor) {
        return -1;
    }
    /* The minor version numbers are equal, continue comparison. */
    if (ver1->iv_revision > ver2->iv_revision) {
        return 1;
    }
    if (ver1->iv_revision < ver2->iv_revision) {
        return -1;
    }

#if defined(MCUBOOT_VERSION_CMP_USE_BUILD_NUMBER)
    /* The revisions are equal, continue comparison. */
    if (ver1->iv_build_num > ver2->iv_build_num) {
        return 1;
    }
    if (ver1->iv_build_num < ver2->iv_build_num) {
        return -1;
    }
#endif

    return 0;
}

#ifdef MCUBOOT_HW_ROLLBACK_PROT
int
boot_update_security_counter(struct boot_loader_state *state, int slot, int hdr_slot_idx)
{
    const struct flash_area *fap = NULL;
    uint32_t img_security_cnt;
    int rc;

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    rc = bootutil_get_img_security_cnt(state, hdr_slot_idx, fap, &img_security_cnt);
    if (rc != 0) {
        goto done;
    }

    rc = boot_nv_security_counter_update(BOOT_CURR_IMG(state), img_security_cnt);
    if (rc != 0) {
        goto done;
    }

done:
    return rc;
}
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

int
boot_add_shared_data(struct boot_loader_state *state, uint8_t active_slot)
{
#if defined(MCUBOOT_MEASURED_BOOT) || defined(MCUBOOT_DATA_SHARING)
    int rc;

#ifdef MCUBOOT_MEASURED_BOOT
    rc = boot_save_boot_status(BOOT_CURR_IMG(state),
                                boot_img_hdr(state, active_slot),
                                BOOT_IMG_AREA(state, active_slot));
    if (rc != 0) {
        BOOT_LOG_ERR("Failed to add image data to shared area");
        return rc;
    }
#endif /* MCUBOOT_MEASURED_BOOT */

#ifdef MCUBOOT_DATA_SHARING
    rc = boot_save_shared_data(boot_img_hdr(state, active_slot),
                                BOOT_IMG_AREA(state, active_slot),
                                active_slot, boot_get_image_max_sizes());
    if (rc != 0) {
        BOOT_LOG_ERR("Failed to add data to shared memory area.");
        return rc;
    }
#endif /* MCUBOOT_DATA_SHARING */

    return 0;

#else /* MCUBOOT_MEASURED_BOOT || MCUBOOT_DATA_SHARING */
    (void) (state);
    (void) (active_slot);

    return 0;
#endif
}

#if !defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD) && !defined(MCUBOOT_SINGLE_APPLICATION_SLOT)
int
boot_open_all_flash_areas(struct boot_loader_state *state)
{
    size_t slot;
    int rc = 0;
    int fa_id;
    int image_index;

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
#if BOOT_IMAGE_NUMBER > 1
        if (state->img_mask[BOOT_CURR_IMG(state)]) {
            continue;
        }
#endif
        image_index = BOOT_CURR_IMG(state);

        for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
            fa_id = flash_area_id_from_multi_image_slot(image_index, slot);
            rc = flash_area_open(fa_id, &BOOT_IMG_AREA(state, slot));
            assert(rc == 0);

            if (rc != 0) {
                BOOT_LOG_ERR("Failed to open flash area ID %d (image %d slot %zu): %d",
                             fa_id, image_index, slot, rc);
                goto out;
            }
        }
    }

#if MCUBOOT_SWAP_USING_SCRATCH
    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &BOOT_SCRATCH_AREA(state));
    assert(rc == 0);

    if (rc != 0) {
        BOOT_LOG_ERR("Failed to open scratch flash area: %d", rc);
        goto out;
    }
#endif

out:
    if (rc != 0) {
        boot_close_all_flash_areas(state);
    }

    return rc;
}

void
boot_close_all_flash_areas(struct boot_loader_state *state)
{
    uint32_t slot;

#if MCUBOOT_SWAP_USING_SCRATCH
    if (BOOT_SCRATCH_AREA(state) != NULL) {
        flash_area_close(BOOT_SCRATCH_AREA(state));
    }
#endif

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
#if BOOT_IMAGE_NUMBER > 1
        if (state->img_mask[BOOT_CURR_IMG(state)]) {
            continue;
        }
#endif
        for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
            if (BOOT_IMG_AREA(state, BOOT_NUM_SLOTS - 1 - slot) != NULL) {
                flash_area_close(BOOT_IMG_AREA(state, BOOT_NUM_SLOTS - 1 - slot));
            }
        }
    }
}
#endif /* !MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD && !MCUBOOT_SINGLE_APPLICATION_SLOT */

void boot_state_init(struct boot_loader_state *state)
{
#if defined(MCUBOOT_ENC_IMAGES)
    int image;
    int slot;
#endif

    memset(state, 0, sizeof(*state));

#if defined(MCUBOOT_ENC_IMAGES)
    for (image = 0; image < BOOT_IMAGE_NUMBER; ++image) {
        for (slot = 0; slot < BOOT_NUM_SLOTS; ++slot) {
            boot_enc_init(&state->enc[image][slot]);
        }
    }
#endif
}
