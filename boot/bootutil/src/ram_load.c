/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016-2020 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2023 Arm Limited
 * Copyright (c) 2024 Nordic Semiconductor ASA
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

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/ramload.h"
#include "bootutil/mcuboot_status.h"

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

#include "mcuboot_config/mcuboot_config.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifndef MULTIPLE_EXECUTABLE_RAM_REGIONS
#if !defined(IMAGE_EXECUTABLE_RAM_START) || !defined(IMAGE_EXECUTABLE_RAM_SIZE)
#error "Platform MUST define executable RAM bounds in case of RAM_LOAD"
#endif
#endif

/**
 * Verifies that the active slot of the current image can be loaded within the
 * predefined bounds that are allowed to be used by executable images.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
static int
boot_verify_ram_load_address(struct boot_loader_state *state)
{
    uint32_t img_dst;
    uint32_t img_sz;
    uint32_t img_end_addr;
    uint32_t exec_ram_start;
    uint32_t exec_ram_size;

    (void)state;

#ifdef MULTIPLE_EXECUTABLE_RAM_REGIONS
    int      rc;

    rc = boot_get_image_exec_ram_info(BOOT_CURR_IMG(state), &exec_ram_start,
                                      &exec_ram_size);
    if (rc != 0) {
        return BOOT_EBADSTATUS;
    }
#else
    exec_ram_start = IMAGE_EXECUTABLE_RAM_START;
    exec_ram_size = IMAGE_EXECUTABLE_RAM_SIZE;
#endif

    img_dst = state->slot_usage[BOOT_CURR_IMG(state)].img_dst;
    img_sz = state->slot_usage[BOOT_CURR_IMG(state)].img_sz;

    if (img_dst < exec_ram_start) {
        return BOOT_EBADIMAGE;
    }

    if (!boot_u32_safe_add(&img_end_addr, img_dst, img_sz)) {
        return BOOT_EBADIMAGE;
    }

    if (img_end_addr > (exec_ram_start + exec_ram_size)) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

#ifdef MCUBOOT_ENC_IMAGES

/**
 * Copies and decrypts an image from a slot in the flash to an SRAM address.
 *
 * @param  state    Boot loader status information.
 * @param  slot     The flash slot of the image to be copied to SRAM.
 * @param  hdr      The image header.
 * @param  src_sz   Size of the image.
 * @param  img_dst  Pointer to the address at which the image needs to be
 *                  copied to SRAM.
 *
 * @return          0 on success; nonzero on failure.
 */
static int
boot_decrypt_and_copy_image_to_sram(struct boot_loader_state *state,
                                    uint32_t slot, struct image_header *hdr,
                                    uint32_t src_sz, uint32_t img_dst)
{
    /* The flow for the decryption and copy of the image is as follows :
     * 1. The whole image is copied to the RAM (header + payload + TLV).
     * 2. The encryption key is loaded from the TLV in flash.
     * 3. The image is then decrypted chunk by chunk in RAM (1 chunk
     * is 1024 bytes). Only the payload section is decrypted.
     * 4. The image is authenticated in RAM.
     */
    const struct flash_area *fap_src = NULL;
    struct boot_status bs;
    uint32_t blk_off;
    uint32_t tlv_off;
    uint32_t blk_sz;
    uint32_t bytes_copied = hdr->ih_hdr_size;
    uint32_t chunk_sz;
    uint32_t max_sz = 1024;
    uint16_t idx;
    uint8_t * cur_dst;
    int area_id;
    int rc;
    uint8_t * ram_dst = (void *)(IMAGE_RAM_BASE + img_dst);

    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);
    rc = flash_area_open(area_id, &fap_src);
    if (rc != 0){
        return BOOT_EFLASH;
    }

    tlv_off = BOOT_TLV_OFF(hdr);

    /* Copying the whole image in RAM */
    rc = flash_area_read(fap_src, 0, ram_dst, src_sz);
    if (rc != 0) {
        goto done;
    }

    rc = boot_enc_load(BOOT_CURR_ENC(state), slot, hdr, fap_src, &bs);
    if (rc < 0) {
        goto done;
    }

    /* if rc > 0 then the key has already been loaded */
    if (rc == 0 && boot_enc_set_key(BOOT_CURR_ENC(state), slot, &bs)) {
        goto done;
    }

    /* Starting at the end of the header as the header section is not encrypted */
    while (bytes_copied < tlv_off) { /* TLV section copied previously */
        if (src_sz - bytes_copied > max_sz) {
            chunk_sz = max_sz;
        } else {
            chunk_sz = src_sz - bytes_copied;
        }

        cur_dst = ram_dst + bytes_copied;
        blk_sz = chunk_sz;
        idx = 0;
        blk_off = ((bytes_copied) - hdr->ih_hdr_size) & 0xf;
        if (bytes_copied + chunk_sz > tlv_off) {
            /* Going over TLV section
             * Part of the chunk is encrypted payload */
            blk_sz = tlv_off - (bytes_copied);
        }
        boot_encrypt(BOOT_CURR_ENC(state), slot,
            (bytes_copied + idx) - hdr->ih_hdr_size, blk_sz,
            blk_off, cur_dst);

        bytes_copied += chunk_sz;
    }
    rc = 0;

done:
    flash_area_close(fap_src);

    return rc;
}

#endif /* MCUBOOT_ENC_IMAGES */
/**
 * Copies a slot of the current image into SRAM.
 *
 * @param  state    Boot loader status information.
 * @param  slot     The flash slot of the image to be copied to SRAM.
 * @param  img_dst  The address at which the image needs to be copied to
 *                  SRAM.
 * @param  img_sz   The size of the image that needs to be copied to SRAM.
 *
 * @return          0 on success; nonzero on failure.
 */
static int
boot_copy_image_to_sram(struct boot_loader_state *state, int slot,
                        uint32_t img_dst, uint32_t img_sz)
{
    int rc;
    const struct flash_area *fap_src = NULL;
    int area_id;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);

    rc = flash_area_open(area_id, &fap_src);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    /* Direct copy from flash to its new location in SRAM. */
    rc = flash_area_read(fap_src, 0, (void *)(IMAGE_RAM_BASE + img_dst), img_sz);
    if (rc != 0) {
        BOOT_LOG_INF("Error whilst copying image %d from Flash to SRAM: %d",
                     BOOT_CURR_IMG(state), rc);
    }

    flash_area_close(fap_src);

    return rc;
}

#if (BOOT_IMAGE_NUMBER > 1)
/**
 * Checks if two memory regions (A and B) are overlap or not.
 *
 * @param  start_a  Start of the A region.
 * @param  end_a    End of the A region.
 * @param  start_b  Start of the B region.
 * @param  end_b    End of the B region.
 *
 * @return          true if there is overlap; false otherwise.
 */
static bool
do_regions_overlap(uint32_t start_a, uint32_t end_a,
                   uint32_t start_b, uint32_t end_b)
{
    if (start_b > end_a) {
        return false;
    } else if (start_b >= start_a) {
        return true;
    } else if (end_b > start_a) {
        return true;
    }

    return false;
}

/**
 * Checks if the image we want to load to memory overlap with an already
 * ramloaded image.
 *
 * @param  state    Boot loader status information.
 *
 * @return                    0 if there is no overlap; nonzero otherwise.
 */
static int
boot_check_ram_load_overlapping(struct boot_loader_state *state)
{
    uint32_t i;

    uint32_t start_a;
    uint32_t end_a;
    uint32_t start_b;
    uint32_t end_b;
    uint32_t image_id_to_check = BOOT_CURR_IMG(state);

    start_a = state->slot_usage[image_id_to_check].img_dst;
    /* Safe to add here, values are already verified in
     * boot_verify_ram_load_address() */
    end_a = start_a + state->slot_usage[image_id_to_check].img_sz;

    for (i = 0; i < BOOT_IMAGE_NUMBER; i++) {
        if (state->slot_usage[i].active_slot == NO_ACTIVE_SLOT
            || i == image_id_to_check) {
            continue;
        }

        start_b = state->slot_usage[i].img_dst;
        /* Safe to add here, values are already verified in
         * boot_verify_ram_load_address() */
        end_b = start_b + state->slot_usage[i].img_sz;

        if (do_regions_overlap(start_a, end_a, start_b, end_b)) {
            return -1;
        }
    }

    return 0;
}
#endif

/**
 * Loads the active slot of the current image into SRAM. The load address and
 * image size is extracted from the image header.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
int
boot_load_image_to_sram(struct boot_loader_state *state)
{
    uint32_t active_slot;
    struct image_header *hdr = NULL;
    uint32_t img_dst;
    uint32_t img_sz;
    int rc;

    active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;
    hdr = boot_img_hdr(state, active_slot);

    if (hdr->ih_flags & IMAGE_F_RAM_LOAD) {

        img_dst = hdr->ih_load_addr;

        rc = boot_read_image_size(state, active_slot, &img_sz);
        if (rc != 0) {
            return rc;
        }

        state->slot_usage[BOOT_CURR_IMG(state)].img_dst = img_dst;
        state->slot_usage[BOOT_CURR_IMG(state)].img_sz = img_sz;

        rc = boot_verify_ram_load_address(state);
        if (rc != 0) {
            BOOT_LOG_INF("Image %d RAM load address 0x%x is invalid.", BOOT_CURR_IMG(state), img_dst);
            return rc;
        }

#if (BOOT_IMAGE_NUMBER > 1)
        rc = boot_check_ram_load_overlapping(state);
        if (rc != 0) {
            BOOT_LOG_INF("Image %d RAM loading to address 0x%x would overlap with\
                         another image.", BOOT_CURR_IMG(state), img_dst);
            return rc;
        }
#endif
#ifdef MCUBOOT_ENC_IMAGES
        /* decrypt image if encrypted and copy it to RAM */
        if (IS_ENCRYPTED(hdr)) {
            rc = boot_decrypt_and_copy_image_to_sram(state, active_slot, hdr, img_sz, img_dst);
        } else {
            rc = boot_copy_image_to_sram(state, active_slot, img_dst, img_sz);
        }
#else
        /* Copy image to the load address from where it currently resides in
         * flash.
         */
        rc = boot_copy_image_to_sram(state, active_slot, img_dst, img_sz);
#endif
        if (rc != 0) {
            BOOT_LOG_INF("Image %d RAM loading to 0x%x is failed.", BOOT_CURR_IMG(state), img_dst);
        } else {
            BOOT_LOG_INF("Image %d RAM loading to 0x%x is succeeded.", BOOT_CURR_IMG(state), img_dst);
        }
    } else {
        /* Only images that support IMAGE_F_RAM_LOAD are allowed if
         * MCUBOOT_RAM_LOAD is set.
         */
        rc = BOOT_EBADIMAGE;
    }

    if (rc != 0) {
        state->slot_usage[BOOT_CURR_IMG(state)].img_dst = 0;
        state->slot_usage[BOOT_CURR_IMG(state)].img_sz = 0;
    }

    return rc;
}

/**
 * Removes an image from SRAM, by overwriting it with zeros.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
int
boot_remove_image_from_sram(struct boot_loader_state *state)
{
    (void)state;

    BOOT_LOG_INF("Removing image %d from SRAM at address 0x%x",
                 BOOT_CURR_IMG(state),
                 state->slot_usage[BOOT_CURR_IMG(state)].img_dst);

    memset((void*)(IMAGE_RAM_BASE + state->slot_usage[BOOT_CURR_IMG(state)].img_dst),
           0, state->slot_usage[BOOT_CURR_IMG(state)].img_sz);

    state->slot_usage[BOOT_CURR_IMG(state)].img_dst = 0;
    state->slot_usage[BOOT_CURR_IMG(state)].img_sz = 0;

    return 0;
}

/**
 * Removes an image from flash by erasing the corresponding flash area
 *
 * @param  state    Boot loader status information.
 * @param  slot     The flash slot of the image to be erased.
 *
 * @return          0 on success; nonzero on failure.
 */
int
boot_remove_image_from_flash(struct boot_loader_state *state, uint32_t slot)
{
    int area_id;
    int rc;
    const struct flash_area *fap;

    (void)state;

    BOOT_LOG_INF("Removing image %d slot %d from flash", BOOT_CURR_IMG(state),
                                                         slot);
    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);
    rc = flash_area_open(area_id, &fap);
    if (rc == 0) {
        flash_area_erase(fap, 0, flash_area_get_size(fap));
        flash_area_close(fap);
    }

    return rc;
}
