/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
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

#ifndef H_BOOTUTIL_AREA_
#define H_BOOTUTIL_AREA_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <flash_map_backend/flash_map_backend.h>
#include <mcuboot_config/mcuboot_config.h>

#if MCUBOOT_SWAP_USING_MOVE
#define BOOT_STATUS_MOVE_STATE_COUNT    1
#define BOOT_STATUS_SWAP_STATE_COUNT    2
#define BOOT_STATUS_STATE_COUNT         (BOOT_STATUS_MOVE_STATE_COUNT + BOOT_STATUS_SWAP_STATE_COUNT)
#elif MCUBOOT_SWAP_USING_OFFSET
#define BOOT_STATUS_SWAP_STATE_COUNT    2
#define BOOT_STATUS_STATE_COUNT         BOOT_STATUS_SWAP_STATE_COUNT
#else
#define BOOT_STATUS_STATE_COUNT         3
#endif

/** Maximum number of image sectors supported by the bootloader. */
#define BOOT_MAX_IMG_SECTORS            MCUBOOT_MAX_IMG_SECTORS
#define BOOT_STATUS_MAX_ENTRIES         BOOT_MAX_IMG_SECTORS

#define BOOT_STATUS_SOURCE_NONE         0
#define BOOT_STATUS_SOURCE_SCRATCH      1
#define BOOT_STATUS_SOURCE_PRIMARY_SLOT 2

/* Helper macro to avoid compile errors with systems that do not
 * provide function to check device type.
 * Note: it used to be inline, but somehow compiler would not
 * optimize out branches that were impossible when this evaluated to
 * just "true".
 */
#if defined(MCUBOOT_SUPPORT_DEV_WITHOUT_ERASE) && defined(MCUBOOT_SUPPORT_DEV_WITH_ERASE)
#define device_requires_erase(fa) (flash_area_erase_required(fa))
#elif defined(MCUBOOT_SUPPORT_DEV_WITHOUT_ERASE)
#define device_requires_erase(fa) (false)
#else
#define device_requires_erase(fa) (true)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Amount of space used to maintain progress information for all swap
 * operations.
 */
uint32_t boot_status_sz(uint32_t min_write_sz);

/**
 * Amount of space used to maintain progress information for all swap
 * operations as well as to save information required when doing a swap,
 * or while a swap is under progress.
 */
uint32_t boot_trailer_sz(uint32_t min_write_sz);

/*
 * Similar to `boot_trailer_sz` but this function returns the space used to
 * store status in the scratch partition. The scratch partition only stores
 * status during the swap of the last sector from primary/secondary (which
 * is the first swap operation) and thus only requires space for one swap.
 */
uint32_t boot_scratch_trailer_sz(uint32_t min_write_sz);

/**
 * Get offset of trailer aligned to either device erase unit or alignment depending on whether
 * device has erase or not.
 *
 * @param fa         The flash_area containing the trailer.
 * @param alignment  The required alignment for the trailer.
 * @param off        Pointer to variable to store the offset of the trailer.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_trailer_scramble_offset(const struct flash_area *fa, size_t alignment, size_t *off);

/**
 * Erases a region of device that requires erase prior to write; does
 * nothing on devices without erase.
 *
 * @param fa         The flash_area containing the region to erase.
 * @param off        The offset within the flash area to start the erase.
 * @param size       The number of bytes to erase.
 * @param backwards  If set to true will erase from end to start addresses, otherwise erases from
 *                   start to end addresses.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_erase_region(const struct flash_area *fap, uint32_t off, uint32_t sz, bool backwards);

/**
 * Removes data from specified region either by writing erase value in place of data or by doing
 * erase, if device has such hardware requirement.
 *
 * @note This function will fail if off or size are not aligned to device write block size or erase
 *       block size.
 *
 * @param fa         The flash_area containing the region to erase.
 * @param off        The offset within the flash area to start the erase.
 * @param size       The number of bytes to erase.
 * @param backwards  If set to true will erase from end to start addresses, otherwise erases from
 *                   start to end addresses.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_scramble_region(const struct flash_area *fap, uint32_t off, uint32_t sz, bool backwards);

/**
 * Removes enough data from slot to mark it as unused.
 *
 * This function either scrambles header magic, header sector or entire slot, depending on
 * configuration.
 *
 * @warning This function assumes that header and trailer are not overlapping on write block or
 *          erase block, if device has erase requirement.
 *
 * @note This function is intended for removing data not preparing device for write.
 * @note Slot is passed here because at this point there is no function matching flash_area object
 *       to slot.
 *
 * @param fa    Pointer to flash area object for slot
 * @param slot  Slot the @p fa represents
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_scramble_slot(const struct flash_area *fap, int slot);

#ifdef __cplusplus
}
#endif

#endif /* H_BOOTUTIL_AREA_ */
