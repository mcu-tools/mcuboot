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

#ifndef H_SWAP_PRIV_
#define H_SWAP_PRIV_

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_SWAP_USING_SCRATCH) || defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET)

/**
 * Calculates the amount of space required to store the trailer, and erases
 * all sectors required for this storage in the given flash_area.
 * The erase will only happen on devices that require erase as a preparation
 * for write. To just remove swap status, the swap_scramble_trailer_sectors
 * should be called.
 */
int swap_erase_trailer_sectors(const struct boot_loader_state *state,
                               const struct flash_area *fap);

/**
 * Calculate the amount of space required to store the trailer and remove
 * data from trailer. This is similar to swap_erase_trailer_sectors, but
 * is intended to remove swap status not to prepare device with explicit
 * erase requirements before write.
 */
int swap_scramble_trailer_sectors(const struct boot_loader_state *state,
                                  const struct flash_area *fap);
/**
 * Initialize the given flash_area with the metadata required to start a new
 * swap upgrade.
 */
int swap_status_init(struct boot_loader_state *state,
                     const struct flash_area *fap,
                     const struct boot_status *bs);

/**
 * Tries to locate an interrupted swap status (metadata). If not metadata
 * was found returns BOOT_STATUS_SOURCE_NONE.
 *
 * Must return one of:
 *   - BOOT_STATUS_SOURCE_NONE
 *   - BOOT_STATUS_SOURCE_SCRATCH
 *   - BOOT_STATUS_SOURCE_PRIMARY_SLOT
 */
int swap_status_source(struct boot_loader_state *state);

/**
 * Reads the boot status from the flash.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present, or it indicates that previous copy finished,
 * there is no operation in progress.
 */
int swap_read_status(struct boot_loader_state *state, struct boot_status *bs);

/**
 * Iterate over the swap status bytes in the given flash_area and populate
 * the given boot_status with the calculated index where a swap upgrade was
 * interrupted.
 */
int swap_read_status_bytes(const struct flash_area *fap,
                           struct boot_loader_state *state,
                           struct boot_status *bs);

/**
 * Marks the image in the primary slot as fully copied.
 */
int swap_set_copy_done(uint8_t image_index);

/**
 * Marks a reverted image in the primary slot as confirmed. This is necessary to
 * ensure the status bytes from the image revert operation don't get processed
 * on a subsequent boot.
 *
 * NOTE: image_ok is tested before writing because if there's a valid permanent
 * image installed on the primary slot and the new image to be upgrade to has a
 * bad sig, image_ok would be overwritten.
 */
int swap_set_image_ok(uint8_t image_index);

/**
 * Start a new or resume an interrupted swap according to the parameters
 * found in the given boot_status.
 */
void swap_run(struct boot_loader_state *state,
              struct boot_status *bs,
              uint32_t copy_size);

#if MCUBOOT_SWAP_USING_SCRATCH
#define BOOT_SCRATCH_AREA(state) ((state)->scratch.area)

static inline size_t boot_scratch_area_size(const struct boot_loader_state *state)
{
    return flash_area_get_size(BOOT_SCRATCH_AREA(state));
}

/**
 * Calculates the number of bytes to copy in a single swap step, grouping
 * contiguous sectors that fit within the scratch area size.
 *
 * @param state              Current bootloader's state.
 * @param last_sector_idx    Index of the last sector in the group (inclusive).
 * @param out_first_sector_idx  Output: index of the first sector (inclusive).
 *
 * @return  The number of bytes comprised by the [first, last] sector range.
 */
uint32_t boot_copy_sz(const struct boot_loader_state *state,
                      int last_sector_idx, int *out_first_sector_idx);

/**
 * Finds the index of the last sector in the primary slot that needs swapping.
 *
 * @param state      Current bootloader's state.
 * @param copy_size  Total number of bytes to swap.
 *
 * @return  Index of the last sector that needs swapping.
 */
int find_last_sector_idx(const struct boot_loader_state *state,
                         uint32_t copy_size);
#endif

#endif /* defined(MCUBOOT_SWAP_USING_SCRATCH) || defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) */

#if defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET)
/**
 * Check if device write block sizes are as expected, function should emit an error if there is
 * a problem. If true is returned, the slots are marked as compatible, otherwise the slots are
 * marked as incompatible.
 *
 * Requires MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE be set to the write block size of image 0 primary
 * slot and MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE be set to the write block size of image 0 secondary
 * slot.
 */
bool swap_write_block_size_check(struct boot_loader_state *state);
#endif /* defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) */

/**
 * Returns the maximum size of an application that can be loaded to a slot.
 */
int app_max_size(struct boot_loader_state *state);

#endif /* H_SWAP_PRIV_ */
