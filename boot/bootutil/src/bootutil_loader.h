/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2020 Linaro LTD
 * Copyright (c) 2017-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
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

#ifndef H_BOOTUTIL_LOADER_
#define H_BOOTUTIL_LOADER_

#include <stdbool.h>
#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"

/*
 * This macro allows some control on the allocation of local variables.
 * When running natively on a target, we don't want to allocated huge
 * variables on the stack, so make them global instead. For the simulator
 * we want to run as many threads as there are tests, and it's safer
 * to just make those variables stack allocated.
 */
#if !defined(__BOOTSIM__)
#define TARGET_STATIC static
#else
#define TARGET_STATIC
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check whether the header in the given slot is erased.
 *
 * @param state Boot loader state where the current image's slot will be checked.
 * @param slot  Slot number.
 *
 * @return true if the header is erased, false otherwise.
 */
bool boot_check_header_erased(struct boot_loader_state *state, int slot);

/**
 * Check whether the header is valid.
 *
 * Valid means that the magic is correct, and that the sizes/offsets are "sane".
 * Sane means that there is no overflow on the arithmetic, and that the result fits within the flash
 * area we are in.
 * Also check the flags in the image and class the image as invalid if flags for
 * encryption/compression are present but these features are not enabled.
 *
 * @param state Boot loader state where the current image's slot will be checked.
 * @param slot  Slot number.
 *
 * @return true if the header is valid, false otherwise.
 */
bool boot_check_header_valid(struct boot_loader_state *state, int slot);

/**
 * Reads image headers from all slots for the current image.
 *
 * This function uses boot_read_image_header() to read each slot's image header.
 * In all update scenarios, this function checks if an image contains the valid magic tag.
 * In swap type updates, it also configures/restores the bootloader state as well as status to
 * continue any interrupted swap operations.
 *
 * @param  state        Boot loader status information.
 * @param  require_all  If true, all image headers must be read successfully;
 *                      if false, reading at least the first slot's header is sufficient.
 * @param  bs           Pointer to boot status structure; may be NULL.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_read_image_headers(struct boot_loader_state *state, bool require_all,
                            struct boot_status *bs);

/**
 * Validate image hash/signature and optionally the security counter in a slot.
 *
 * @note This function uses bootutil_img_validate() to perform the actual validation.
 *
 * @param state Boot loader state where the current image's slot will be checked.
 * @param bs    Pointer to the boot status structure. Used only when encrypted images are validated.
 * @param slot  Slot number.
 *
 * @return FIH_SUCCESS if the image is valid, FIH_FAILURE otherwise.
 */
fih_ret boot_check_image(struct boot_loader_state *state, struct boot_status *bs, int slot);

/**
 * Compare image version numbers
 *
 * By default, the comparison does not take build number into account.
 * Enable MCUBOOT_VERSION_CMP_USE_BUILD_NUMBER to take the build number into account.
 *
 * @param ver1  Pointer to the first image version to compare.
 * @param ver2  Pointer to the second image version to compare.
 *
 * @retval -1 If ver1 is less than ver2.
 * @retval 0  If the image version numbers are equal.
 * @retval 1  If ver1 is greater than ver2.
 */
int boot_compare_version(const struct image_version *ver1, const struct image_version *ver2);

#ifdef MCUBOOT_HW_ROLLBACK_PROT
/**
 * Updates the stored security counter value with the image's security counter value which resides
 * in the given slot, only if it's greater than the stored value.
 *
 * @param state         Boot state where the current image's security counter will be updated.
 * @param slot          Slot number of the image.
 * @param hdr_slot_idx  Index of the header in the state current image variable containing the
 *                      pointer to the image header structure of the image that is currently stored
 *                      in the given slot.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_update_security_counter(struct boot_loader_state *state, int slot, int hdr_slot_idx);
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

/**
 * Saves boot status and shared data for current image.
 *
 * @note This function is a helper routine, that uses boot_save_boot_status() and
 *       boot_add_shared_data() to append the respective information, depending on the
 *       configuration.
 *
 * @param  state        Boot loader status information.
 * @param  active_slot  Index of the slot will be loaded for current image.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_add_shared_data(struct boot_loader_state *state, uint8_t active_slot);

/**
 * Opens the flash areas of all images.
 *
 * @note This function opens all areas for all images, including scratch area if
 *       MCUBOOT_SWAP_USING_SCRATCH is defined.
 *
 * @param state Bootloader state.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_open_all_flash_areas(struct boot_loader_state *state);

/**
 * Closes the flash areas of all images.
 *
 * @note This function closes all areas for all images, including scratch area if
 *       MCUBOOT_SWAP_USING_SCRATCH is defined.
 *
 * @param state Bootloader state.
 */
void boot_close_all_flash_areas(struct boot_loader_state *state);

#ifdef __cplusplus
}
#endif

#endif /* H_BOOTUTIL_LOADER_ */
