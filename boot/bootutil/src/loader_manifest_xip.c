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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/security_cnt.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/boot_hooks.h"
#include "bootutil_loader.h"

#if defined(MCUBOOT_MANIFEST_UPDATES) && defined(MCUBOOT_DIRECT_XIP)
#include "bootutil/mcuboot_manifest.h"

#if defined(MCUBOOT_DIRECT_XIP) && defined(MCUBOOT_DECOMPRESS_IMAGES)
#error "Image decompression is not supported when MCUBOOT_DIRECT_XIP is selected."
#endif /* MCUBOOT_DIRECT_XIP && MCUBOOT_DECOMPRESS_IMAGES */

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

BOOT_LOG_MODULE_DECLARE(mcuboot);

static struct boot_loader_state boot_data;

#if defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO) || defined(MCUBOOT_DATA_SHARING)
static struct image_max_size image_max_sizes[BOOT_IMAGE_NUMBER] = {0};
#endif

#if BOOT_MAX_ALIGN > 1024
#define BUF_SZ BOOT_MAX_ALIGN
#else
#define BUF_SZ 1024
#endif

struct boot_loader_state *boot_get_loader_state(void)
{
    return &boot_data;
}

#if defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO) || defined(MCUBOOT_DATA_SHARING)
struct image_max_size *boot_get_image_max_sizes(void)
{
    return image_max_sizes;
}
#endif

/**
 * Fills rsp to indicate how booting should occur.
 *
 * @param  state        Boot loader status information.
 * @param  rsp          boot_rsp struct to fill.
 */
static void
fill_rsp(struct boot_loader_state *state, struct boot_rsp *rsp)
{
    uint32_t active_slot;

    /* Always boot from the first image. */
    BOOT_CURR_IMG(state) = 0;
    active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;

    rsp->br_flash_dev_id = flash_area_get_device_id(BOOT_IMG_AREA(state, active_slot));
    rsp->br_image_off = boot_img_slot_off(state, active_slot);
    rsp->br_hdr = boot_img_hdr(state, active_slot);
}

#if defined(MCUBOOT_DIRECT_XIP)
/**
 * Check if image in slot has been set with specific ROM address to run from
 * and whether the slot starts at that address.
 *
 * @retval true  if IMAGE_F_ROM_FIXED flag is not set;
 * @retval true  if IMAGE_F_ROM_FIXED flag is set and ROM address specified in
 *               header matches the slot address;
 * @retval false if IMAGE_F_ROM_FIXED flag is set but ROM address specified in
 *               header does not match the slot address.
 */
static bool
boot_rom_address_check(struct boot_loader_state *state)
{
    uint32_t active_slot;
    const struct image_header *hdr;
    uint32_t f_off;

    active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;
    hdr = boot_img_hdr(state, active_slot);
    f_off = boot_img_slot_off(state, active_slot);

    if (hdr->ih_flags & IMAGE_F_ROM_FIXED && hdr->ih_load_addr != f_off) {
        BOOT_LOG_WRN("Image in %s slot at 0x%x has been built for offset 0x%x"\
                     ", skipping",
                     active_slot == 0 ? "primary" : "secondary", f_off,
                     hdr->ih_load_addr);

        /* The image is not bootable from this slot. */
        return false;
    }

    return true;
}
#endif

/*
 * Check that there is a valid image in a slot
 *
 * @returns
 *         FIH_SUCCESS                      if image was successfully validated
 *         FIH_NO_BOOTABLE_IMAGE            if no bootloable image was found
 *         FIH_FAILURE                      on any errors
 */
static fih_ret
boot_validate_slot(struct boot_loader_state *state, int slot,
                   struct boot_status *bs, int expected_swap_type)
{
    const struct flash_area *fap;
    struct image_header *hdr;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    BOOT_LOG_DBG("boot_validate_slot: slot %d, expected_swap_type %d",
                 slot, expected_swap_type);
    (void)expected_swap_type;

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    hdr = boot_img_hdr(state, slot);
    if (boot_check_header_erased(state, slot) || (hdr->ih_flags & IMAGE_F_NON_BOOTABLE)) {
        /* No bootable image in slot; continue booting from the primary slot. */
        fih_rc = FIH_NO_BOOTABLE_IMAGE;
        goto out;
    }

    if (!boot_check_header_valid(state, slot)) {
        fih_rc = FIH_FAILURE;
    } else {
        BOOT_HOOK_CALL_FIH(boot_image_check_hook, FIH_BOOT_HOOK_REGULAR,
                           fih_rc, BOOT_CURR_IMG(state), slot);
        if (FIH_EQ(fih_rc, FIH_BOOT_HOOK_REGULAR)) {
            FIH_CALL(boot_check_image, fih_rc, state, bs, slot);
        }
    }

    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        if ((slot != BOOT_SLOT_PRIMARY) || ARE_SLOTS_EQUIVALENT()) {
            boot_scramble_slot(fap, slot);
            /* Image is invalid, erase it to prevent further unnecessary
             * attempts to validate and boot it.
             */
        }

#if !defined(__BOOTSIM__)
        BOOT_LOG_ERR("Image in the %s slot is not valid!",
                     (slot == BOOT_SLOT_PRIMARY) ? "primary" : "secondary");
#endif
        fih_rc = FIH_NO_BOOTABLE_IMAGE;
        goto out;
    }

out:
    FIH_RET(fih_rc);
}

/**
 * Opens all flash areas and checks which contain an image with a valid header.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
static int
boot_get_slot_usage(struct boot_loader_state *state)
{
    uint32_t slot;
    int rc;

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
        /* Attempt to read an image header from each slot. */
        rc = boot_read_image_headers(state, false, NULL);
        if (rc != 0) {
            BOOT_LOG_WRN("Failed reading image headers.");
            return rc;
        }

        /* Check headers in all slots */
        for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
            if (boot_check_header_valid(state, slot)) {
                state->slot_usage[BOOT_CURR_IMG(state)].slot_available[slot] = true;
                BOOT_LOG_IMAGE_INFO(slot, boot_img_hdr(state, slot));
            } else {
                state->slot_usage[BOOT_CURR_IMG(state)].slot_available[slot] = false;
                BOOT_LOG_INF("Image %d %s slot: Image not found",
                             BOOT_CURR_IMG(state),
                             (slot == BOOT_SLOT_PRIMARY)
                             ? "Primary" : "Secondary");
            }
        }

        state->slot_usage[BOOT_CURR_IMG(state)].active_slot = BOOT_SLOT_NONE;
    }

    return 0;
}

/**
 * Finds the slot containing the image with the highest version number for the
 * current image.
 *
 * @param  state        Boot loader status information.
 *
 * @return              BOOT_SLOT_NONE if no available slot found, number of
 *                      the found slot otherwise.
 */
static uint32_t
find_slot_with_highest_version(struct boot_loader_state *state)
{
    uint32_t slot;
    uint32_t candidate_slot = BOOT_SLOT_NONE;
    int rc;

    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        if (state->slot_usage[BOOT_CURR_IMG(state)].slot_available[slot]) {
            if (candidate_slot == BOOT_SLOT_NONE) {
                candidate_slot = slot;
            } else {
                rc = boot_compare_version(
                            &boot_img_hdr(state, slot)->ih_ver,
                            &boot_img_hdr(state, candidate_slot)->ih_ver);
                if (rc == 1) {
                    /* The version of the image being examined is greater than
                     * the version of the current candidate.
                     */
                    candidate_slot = slot;
                }
            }
        }
    }

    return candidate_slot;
}

#ifdef MCUBOOT_HAVE_LOGGING
/**
 * Prints the state of the loaded images.
 *
 * @param  state        Boot loader status information.
 */
static void
print_loaded_images(struct boot_loader_state *state)
{
    uint32_t active_slot;

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
        active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;

        BOOT_LOG_INF("Image %d loaded from the %s slot",
                     BOOT_CURR_IMG(state),
                     (active_slot == BOOT_SLOT_PRIMARY) ?
                     "primary" : "secondary");
    }
}
#endif

#if (defined(MCUBOOT_DIRECT_XIP) && defined(MCUBOOT_DIRECT_XIP_REVERT))
/**
 * Checks whether the active slot of the current image was previously selected
 * to run. Erases the image if it was selected but its execution failed,
 * otherwise marks it as selected if it has not been before.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
static int
boot_select_or_erase(struct boot_loader_state *state)
{
    const struct flash_area *fap = NULL;
    int rc;
    uint32_t active_slot;
    struct boot_swap_state* active_swap_state;

    active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;

    fap = BOOT_IMG_AREA(state, active_slot);
    assert(fap != NULL);

    active_swap_state = &(state->slot_usage[BOOT_CURR_IMG(state)].swap_state);

    memset(active_swap_state, 0, sizeof(struct boot_swap_state));
    rc = boot_read_swap_state(fap, active_swap_state);
    assert(rc == 0);

    if (active_swap_state->magic != BOOT_MAGIC_GOOD) {
        /* Image was not selected for test. Skip slot. */
        return -1;
    }

    if (active_swap_state->copy_done == BOOT_FLAG_SET &&
        active_swap_state->image_ok  != BOOT_FLAG_SET) {
        /*
         * A reboot happened without the image being confirmed at
         * runtime or its trailer is corrupted/invalid. Erase the image
         * to prevent it from being selected again on the next reboot.
         */
        BOOT_LOG_DBG("Erasing faulty image in the %s slot.",
                     (active_slot == BOOT_SLOT_PRIMARY) ? "primary" : "secondary");
        rc = boot_scramble_region(fap, 0, flash_area_get_size(fap), false);
        assert(rc == 0);
        rc = -1;
    } else {
        if (active_swap_state->copy_done != BOOT_FLAG_SET) {
            if (active_swap_state->copy_done == BOOT_FLAG_BAD) {
                BOOT_LOG_DBG("The copy_done flag had an unexpected value. Its "
                             "value was neither 'set' nor 'unset', but 'bad'.");
            }
            /*
             * Set the copy_done flag, indicating that the image has been
             * selected to boot. It can be set in advance, before even
             * validating the image, because in case the validation fails, the
             * entire image slot will be erased (including the trailer).
             */
            rc = boot_write_copy_done(fap);
            if (rc != 0) {
                BOOT_LOG_WRN("Failed to set copy_done flag of the image in "
                             "the %s slot.", (active_slot == BOOT_SLOT_PRIMARY) ?
                             "primary" : "secondary");
                rc = 0;
            }
        }
    }

    return rc;
}
#endif /* MCUBOOT_DIRECT_XIP && MCUBOOT_DIRECT_XIP_REVERT */

/**
 * Tries to load and validate a single slot.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
static fih_ret
boot_load_and_validate_current_image(struct boot_loader_state *state)
{
    fih_ret fih_rc;
    uint32_t active_slot = state->slot_usage[BOOT_CURR_IMG(state)].active_slot;

    if (active_slot == BOOT_SLOT_NONE) {
        FIH_RET(FIH_FAILURE);
    }

    BOOT_LOG_INF("Loading image %d from slot %d", BOOT_CURR_IMG(state), active_slot);

#ifdef MCUBOOT_DIRECT_XIP
    if (!boot_rom_address_check(state)) {
        FIH_RET(FIH_FAILURE);
    }
#endif /* MCUBOOT_DIRECT_XIP */

#if defined(MCUBOOT_DIRECT_XIP_REVERT)
    /* The manifest binds images together. The act of validating the manifest
     * image implies that the other images are also validated.
     * Skip this step and ignore those flags for other images, so a sudden power
     * loss after confirming some of the images does not result in partially
     * confirmed state.
     */
    if (BOOT_CURR_IMG(state) == MCUBOOT_MANIFEST_IMAGE_INDEX) {
        if (boot_select_or_erase(state) != 0) {
            FIH_RET(FIH_FAILURE);
        }
    }
#endif /* MCUBOOT_DIRECT_XIP_REVERT */

    FIH_CALL(boot_validate_slot, fih_rc, state, active_slot, NULL, 0);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        /* Image is invalid. */
        FIH_RET(FIH_FAILURE);
    }

    FIH_RET(FIH_SUCCESS);
}

/**
 * Tries to load a slot for all the images with validation.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
fih_ret
boot_load_and_validate_images(struct boot_loader_state *state)
{
    uint32_t active_slot;
    int rc;
    fih_ret fih_rc;

    while (true) {
        BOOT_CURR_IMG(state) = MCUBOOT_MANIFEST_IMAGE_INDEX;

        /* Find the best image with manifest.
         * Use the hook first to allow the implementation to provide a preferred slot.
         */
        rc = BOOT_HOOK_FIND_SLOT_CALL(boot_find_next_slot_hook, BOOT_HOOK_REGULAR,
                                      state, MCUBOOT_MANIFEST_IMAGE_INDEX, &active_slot);
        if (rc == BOOT_HOOK_REGULAR) {
            /* If there is no preferred slot provided by the hook, look for the slot with the
             * highest version.
             */
            active_slot = find_slot_with_highest_version(state);
        }
        /* If both selections failed - there is no usable image with a manifest to use. */
        if (active_slot == BOOT_SLOT_NONE) {
            BOOT_LOG_ERR("No more manifest slots available");
            FIH_RET(FIH_FAILURE);
        }

        /* Save the number of the active manifest slot. */
        state->slot_usage[MCUBOOT_MANIFEST_IMAGE_INDEX].active_slot = active_slot;

        /* Validate the image with manifest and load the manifest into bootloader state. */
        FIH_CALL(boot_load_and_validate_current_image, fih_rc, state);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
            state->slot_usage[MCUBOOT_MANIFEST_IMAGE_INDEX].slot_available[active_slot] = false;
            state->slot_usage[MCUBOOT_MANIFEST_IMAGE_INDEX].active_slot = BOOT_SLOT_NONE;
            BOOT_LOG_INF("No valid manifest in slot %d", active_slot);
            continue;
        }

        BOOT_LOG_INF("Try to validate images using manifest in slot %d", active_slot);

        /* Go over all other images and try to load one */
        IMAGES_ITER(BOOT_CURR_IMG(state)) {
            /* Skip the image with manifest - it's been already verified. */
            if (BOOT_CURR_IMG(state) == MCUBOOT_MANIFEST_IMAGE_INDEX) {
                continue;
            }

            /* Check if there is a matching slot available. */
            if (!state->slot_usage[BOOT_CURR_IMG(state)].slot_available[active_slot]) {
                /* Invalidate manifest */
                FIH_SET(fih_rc, FIH_FAILURE);
                break;
            }

            /* Save the number of the active slot. */
            state->slot_usage[BOOT_CURR_IMG(state)].active_slot = active_slot;

            FIH_CALL(boot_load_and_validate_current_image, fih_rc, state);
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                state->slot_usage[BOOT_CURR_IMG(state)].slot_available[active_slot] = false;
                state->slot_usage[BOOT_CURR_IMG(state)].active_slot = BOOT_SLOT_NONE;
                /* Invalidate manifest */
                break;
            }
        }

        if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
            /* All images have been loaded and validated successfully. */
            FIH_RET(FIH_SUCCESS);
        }

        BOOT_LOG_DBG("Manifest in slot %d is invalid", active_slot);

        /* Invalidate manifest */
        state->slot_usage[MCUBOOT_MANIFEST_IMAGE_INDEX].slot_available[active_slot] = false;
        state->slot_usage[MCUBOOT_MANIFEST_IMAGE_INDEX].active_slot = BOOT_SLOT_NONE;
    }

    FIH_RET(FIH_FAILURE);
}

/**
 * Updates the security counter for the current image.
 *
 * @param  state        Boot loader status information.
 *
 * @return              0 on success; nonzero on failure.
 */
static int
boot_update_hw_rollback_protection(struct boot_loader_state *state)
{
#ifdef MCUBOOT_HW_ROLLBACK_PROT
    int rc;

    /* Update the stored security counter with the newer (active) image's
     * security counter value.
     */
#if (defined(MCUBOOT_DIRECT_XIP) && defined(MCUBOOT_DIRECT_XIP_REVERT))
    /* When the 'revert' mechanism is enabled in direct-xip or RAM load mode,
     * the security counter can be increased only after reboot, if the image
     * has been confirmed at runtime (the image_ok flag has been set).
     * This way a 'revert' can be performed when it's necessary.
     */
    if (state->slot_usage[BOOT_CURR_IMG(state)].swap_state.image_ok == BOOT_FLAG_SET) {
#endif
        rc = boot_update_security_counter(state,
                                          state->slot_usage[BOOT_CURR_IMG(state)].active_slot,
                                          state->slot_usage[BOOT_CURR_IMG(state)].active_slot);
        if (rc != 0) {
            BOOT_LOG_ERR("Security counter update failed after image %d validation.",
                         BOOT_CURR_IMG(state));
            return rc;
        }

#ifdef MCUBOOT_HW_ROLLBACK_PROT_LOCK
        rc = boot_nv_security_counter_lock(BOOT_CURR_IMG(state));
        if (rc != 0) {
            BOOT_LOG_ERR("Security counter lock failed after image %d validation.",
                         BOOT_CURR_IMG(state));
            return rc;
        }
#endif /* MCUBOOT_HW_ROLLBACK_PROT_LOCK */
#if (defined(MCUBOOT_DIRECT_XIP) && defined(MCUBOOT_DIRECT_XIP_REVERT))
    }
#endif

    return 0;

#else /* MCUBOOT_HW_ROLLBACK_PROT */
    (void) (state);
    return 0;
#endif
}

fih_ret
context_boot_go(struct boot_loader_state *state, struct boot_rsp *rsp)
{
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    rc = boot_open_all_flash_areas(state);
    if (rc != 0) {
        goto out;
    }

    rc = boot_get_slot_usage(state);
    if (rc != 0) {
        goto close;
    }

    FIH_CALL(boot_load_and_validate_images, fih_rc, state);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto close;
    }

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
        rc = boot_update_hw_rollback_protection(state);
        if (rc != 0) {
            FIH_SET(fih_rc, FIH_FAILURE);
            goto close;
        }

        rc = boot_add_shared_data(state,
                                  (uint8_t)state->slot_usage[BOOT_CURR_IMG(state)].active_slot);
        if (rc != 0) {
            FIH_SET(fih_rc, FIH_FAILURE);
            goto close;
        }
    }

    /* All image loaded successfully. */
#ifdef MCUBOOT_HAVE_LOGGING
    print_loaded_images(state);
#endif

    fill_rsp(state, rsp);

close:
    boot_close_all_flash_areas(state);

out:
    if (rc != 0) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

    FIH_RET(fih_rc);
}

/**
 * Prepares the booting process. This function moves images around in flash as
 * appropriate, and tells you what address to boot from.
 *
 * @param rsp                   On success, indicates how booting should occur.
 *
 * @return                      FIH_SUCCESS on success; nonzero on failure.
 */
fih_ret
boot_go(struct boot_rsp *rsp)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    boot_state_clear(NULL);

    FIH_CALL(context_boot_go, fih_rc, &boot_data, rsp);
    FIH_RET(fih_rc);
}

int
boot_read_image_header(struct boot_loader_state *state, int slot,
                       struct image_header *out_hdr, struct boot_status *bs)
{
    const struct flash_area *fap = BOOT_IMG_AREA(state, slot);

    assert(fap != NULL);

    if (flash_area_read(fap, 0, out_hdr, sizeof *out_hdr) != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

#endif /* MCUBOOT_MANIFEST_UPDATES && MCUBOOT_DIRECT_XIP */
