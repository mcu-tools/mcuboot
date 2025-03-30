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

#ifdef MCUBOOT_SWAP_USING_MOVE

#if defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
/*
 * FIXME: this might have to be updated for threaded sim
 */
int boot_status_fails = 0;
#define BOOT_STATUS_ASSERT(x)                \
    do {                                     \
        if (!(x)) {                          \
            boot_status_fails++;             \
        }                                    \
    } while (0)
#else
#define BOOT_STATUS_ASSERT(x) ASSERT(x)
#endif

uint32_t
find_last_idx(struct boot_loader_state *state, uint32_t swap_size)
{
    uint32_t sector_sz;
    uint32_t sz;
    uint32_t last_idx;

    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    sz = 0;
    last_idx = 0;
    while (1) {
        sz += sector_sz;
        last_idx++;
        if (sz >= swap_size) {
            break;
        }
    }

    return last_idx;
}

int
boot_read_image_header(struct boot_loader_state *state, int slot,
                       struct image_header *out_hdr, struct boot_status *bs)
{
    const struct flash_area *fap;
    uint32_t off;
    uint32_t sz;
    uint32_t last_idx;
    uint32_t swap_size;
    int rc;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    off = 0;
    if (bs && !boot_status_is_reset(bs)) {
        fap = boot_find_status(state, BOOT_CURR_IMG(state));
        if (fap == NULL || boot_read_swap_size(fap, &swap_size)) {
            rc = BOOT_EFLASH;
            goto done;
        }

        last_idx = find_last_idx(state, swap_size);
        sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);

        /*
         * Find the correct offset or slot where the image header is expected to
         * be found for the steps where it is moved or swapped.
         */
        if (bs->op == BOOT_STATUS_OP_MOVE && slot == 0 && bs->idx > last_idx) {
            off = sz;
        } else if (bs->op == BOOT_STATUS_OP_SWAP) {
            if (bs->idx > 1 && bs->idx <= last_idx) {
                slot = (slot == 0) ? 1 : 0;
            } else if (bs->idx == 1) {
                if (slot == 0) {
                    off = sz;
                } else if (slot == 1 && bs->state == 2) {
                    slot = 0;
                }
            }
        }
    }

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    rc = flash_area_read(fap, off, out_hdr, sizeof *out_hdr);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    /* We only know where the headers are located when bs is valid */
    if (bs != NULL && out_hdr->ih_magic != IMAGE_MAGIC) {
        rc = -1;
        goto done;
    }

    rc = 0;

done:
    return rc;
}

int
swap_read_status_bytes(const struct flash_area *fap,
        struct boot_loader_state *state, struct boot_status *bs)
{
    uint32_t off;
    uint8_t status;
    int max_entries;
    int found_idx;
    uint8_t write_sz;
    int move_entries;
    int rc;
    int last_rc;
    int erased_sections;
    int i;

    max_entries = boot_status_entries(BOOT_CURR_IMG(state), fap);
    if (max_entries < 0) {
        return BOOT_EBADARGS;
    }

    erased_sections = 0;
    found_idx = -1;
    /* skip erased sectors at the end */
    last_rc = 1;
    write_sz = BOOT_WRITE_SZ(state);
    off = boot_status_off(fap);
    for (i = max_entries; i > 0; i--) {
        rc = flash_area_read(fap, off + (i - 1) * write_sz, &status, 1);
        if (rc < 0) {
            return BOOT_EFLASH;
        }

        if (bootutil_buffer_is_erased(fap, &status, 1)) {
            if (rc != last_rc) {
                erased_sections++;
            }
        } else {
            if (found_idx == -1) {
                found_idx = i;
            }
        }
        last_rc = rc;
    }

    if (erased_sections > 1) {
        /* This means there was an error writing status on the last
         * swap. Tell user and move on to validation!
         */
#if !defined(__BOOTSIM__)
        BOOT_LOG_ERR("Detected inconsistent status!");
#endif

#if !defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
        /* With validation of the primary slot disabled, there is no way
         * to be sure the swapped primary slot is OK, so abort!
         */
        assert(0);
#endif
    }

    move_entries = BOOT_MAX_IMG_SECTORS * BOOT_STATUS_MOVE_STATE_COUNT;
    if (found_idx == -1) {
        /* no swap status found; nothing to do */
    } else if (found_idx < move_entries) {
        bs->op = BOOT_STATUS_OP_MOVE;
        bs->idx = (found_idx  / BOOT_STATUS_MOVE_STATE_COUNT) + BOOT_STATUS_IDX_0;
        bs->state = (found_idx % BOOT_STATUS_MOVE_STATE_COUNT) + BOOT_STATUS_STATE_0;;
    } else {
        bs->op = BOOT_STATUS_OP_SWAP;
        bs->idx = ((found_idx - move_entries) / BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_IDX_0;
        bs->state = ((found_idx - move_entries) % BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_STATE_0;
    }

    return 0;
}

uint32_t
boot_status_internal_off(const struct boot_status *bs, int elem_sz)
{
    uint32_t off;
    int idx_sz;

    idx_sz = elem_sz * ((bs->op == BOOT_STATUS_OP_MOVE) ?
            BOOT_STATUS_MOVE_STATE_COUNT : BOOT_STATUS_SWAP_STATE_COUNT);

    off = ((bs->op == BOOT_STATUS_OP_MOVE) ?
               0 : (BOOT_MAX_IMG_SECTORS * BOOT_STATUS_MOVE_STATE_COUNT * elem_sz)) +
           (bs->idx - BOOT_STATUS_IDX_0) * idx_sz +
           (bs->state - BOOT_STATUS_STATE_0) * elem_sz;

    return off;
}

static int app_max_sectors(struct boot_loader_state *state)
{
    uint32_t sz = 0;
    uint32_t sector_sz;
    uint32_t trailer_sz;
    uint32_t first_trailer_idx;

    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));
    first_trailer_idx = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT) - 1;

    while (1) {
        sz += sector_sz;
        if  (sz >= trailer_sz) {
            break;
        }
        first_trailer_idx--;
    }

    return first_trailer_idx;
}

int
boot_slots_compatible(struct boot_loader_state *state)
{
    size_t num_sectors_pri;
    size_t num_sectors_sec;
    size_t sector_sz_pri = 0;
    size_t sector_sz_sec = 0;
    size_t i;
    size_t num_usable_sectors_pri;

    num_sectors_pri = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT);
    num_sectors_sec = boot_img_num_sectors(state, BOOT_SECONDARY_SLOT);
    num_usable_sectors_pri = app_max_sectors(state);

    if ((num_sectors_pri != num_sectors_sec) &&
            (num_sectors_pri != (num_sectors_sec + 1)) &&
            (num_usable_sectors_pri != (num_sectors_sec + 1))) {
        BOOT_LOG_WRN("Cannot upgrade: not a compatible amount of sectors");
        BOOT_LOG_DBG("slot0 sectors: %d, slot1 sectors: %d, usable slot0 sectors: %d",
                     (int)num_sectors_pri, (int)num_sectors_sec,
                     (int)(num_usable_sectors_pri - 1));
        return 0;
    } else if (num_sectors_pri > BOOT_MAX_IMG_SECTORS) {
        BOOT_LOG_WRN("Cannot upgrade: more sectors than allowed");
        return 0;
    }

    if (num_usable_sectors_pri != (num_sectors_sec + 1)) {
        BOOT_LOG_DBG("Non-optimal sector distribution, slot0 has %d usable sectors (%d assigned) "
                     "but slot1 has %d assigned", (int)(num_usable_sectors_pri - 1),
                     (int)num_sectors_pri, (int)num_sectors_sec);
    }

    for (i = 0; i < num_sectors_sec; i++) {
        sector_sz_pri = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, i);
        sector_sz_sec = boot_img_sector_size(state, BOOT_SECONDARY_SLOT, i);
        if (sector_sz_pri != sector_sz_sec) {
            BOOT_LOG_WRN("Cannot upgrade: not same sector layout");
            return 0;
        }
    }

#ifdef MCUBOOT_SLOT0_EXPECTED_ERASE_SIZE
    if (sector_sz_pri != MCUBOOT_SLOT0_EXPECTED_ERASE_SIZE) {
        BOOT_LOG_DBG("Discrepancy, slot0 expected erase size: %d, actual: %d",
                     MCUBOOT_SLOT0_EXPECTED_ERASE_SIZE, sector_sz_pri);
    }
#endif
#ifdef MCUBOOT_SLOT1_EXPECTED_ERASE_SIZE
    if (sector_sz_sec != MCUBOOT_SLOT1_EXPECTED_ERASE_SIZE) {
        BOOT_LOG_DBG("Discrepancy, slot1 expected erase size: %d, actual: %d",
                     MCUBOOT_SLOT1_EXPECTED_ERASE_SIZE, sector_sz_sec);
    }
#endif

#if defined(MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE) || defined(MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE)
    if (!swap_write_block_size_check(state)) {
        BOOT_LOG_WRN("Cannot upgrade: slot write sizes are not compatible");
        return 0;
    }
#endif

    if (num_sectors_pri > num_sectors_sec) {
        if (sector_sz_pri != boot_img_sector_size(state, BOOT_PRIMARY_SLOT, i)) {
            BOOT_LOG_WRN("Cannot upgrade: not same sector layout");
            return 0;
        }
    }

    return 1;
}

#define BOOT_LOG_SWAP_STATE(area, state)                            \
    BOOT_LOG_INF("%s: magic=%s, swap_type=0x%x, copy_done=0x%x, "   \
                 "image_ok=0x%x",                                   \
                 (area),                                            \
                 ((state)->magic == BOOT_MAGIC_GOOD ? "good" :      \
                  (state)->magic == BOOT_MAGIC_UNSET ? "unset" :    \
                  "bad"),                                           \
                 (state)->swap_type,                                \
                 (state)->copy_done,                                \
                 (state)->image_ok)

int
swap_status_source(struct boot_loader_state *state)
{
    struct boot_swap_state state_primary_slot;
    struct boot_swap_state state_secondary_slot;
    int rc;
    uint8_t source;
    uint8_t image_index;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    image_index = BOOT_CURR_IMG(state);

    rc = boot_read_swap_state(state->imgs[image_index][BOOT_PRIMARY_SLOT].area,
                              &state_primary_slot);
    assert(rc == 0);

    BOOT_LOG_SWAP_STATE("Primary image", &state_primary_slot);

    rc = boot_read_swap_state(state->imgs[image_index][BOOT_SECONDARY_SLOT].area,
                              &state_secondary_slot);
    assert(rc == 0);

    BOOT_LOG_SWAP_STATE("Secondary image", &state_secondary_slot);

    if (state_primary_slot.magic == BOOT_MAGIC_GOOD &&
            state_primary_slot.copy_done == BOOT_FLAG_UNSET) {
        /* In this case, either:
         *   - A swap operation was interrupted and can be resumed from the status stored in the
         *     primary slot's trailer.
         *   - No swap was ever made and the initial firmware image has been written with a MCUboot
         *     trailer. In this case, the status in the primary slot's trailer is empty and there is
         *     no harm in loading it.
         */
        source = BOOT_STATUS_SOURCE_PRIMARY_SLOT;

        BOOT_LOG_INF("Boot source: primary slot");
        return source;
    }

    BOOT_LOG_INF("Boot source: none");
    return BOOT_STATUS_SOURCE_NONE;
}

/*
 * "Moves" the sector located at idx - 1 to idx.
 */
static void
boot_move_sector_up(size_t idx, uint32_t sz, struct boot_loader_state *state,
        struct boot_status *bs, const struct flash_area *fap_pri)
{
    uint32_t new_off;
    uint32_t old_off;
    uint32_t copy_sz;
    int rc;
    bool sector_erased_with_trailer;

    /*
     * FIXME: assuming sectors of size == sz, a single off variable
     * would be enough
     */

    /* Calculate offset from start of image area. */
    new_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
    old_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx - 1);

    copy_sz = sz;
    sector_erased_with_trailer = false;

    if (bs->idx == BOOT_STATUS_IDX_0) {
        rc = swap_scramble_trailer_sectors(state, fap_pri);
        assert(rc == 0);

        rc = swap_status_init(state, fap_pri, bs);
        assert(rc == 0);

        /* The first sector to be moved is the last sector containing part of the firmware image. If
         * the trailer size is not a multiple of the sector size, the destination sector will
         * contain both firmware and trailer data. In that case:
         *   - Only the firmware data must be copied to the destination sector to avoid overwriting
         *     the trailer data.
         *   - The destination sector has already been erased with the trailer.
         */
        size_t first_trailer_idx = boot_get_first_trailer_sector(state, BOOT_PRIMARY_SLOT);

        if (idx == first_trailer_idx) {
            /* Swap-move => constant sector size, so 'sz' is the size of a sector and 'swap_size %
             * sz' gives the number of bytes used by the largest firmware image in the last sector
             * to be moved.
             */
            copy_sz = bs->swap_size % sz;
            sector_erased_with_trailer = true;
        }
    }

    if (!sector_erased_with_trailer) {
        rc = boot_erase_region(fap_pri, new_off, sz);
        assert(rc == 0);
    }

    rc = boot_copy_region(state, fap_pri, fap_pri, old_off, new_off, copy_sz);
    assert(rc == 0);

    rc = boot_write_status(state, bs);

    bs->idx++;
    BOOT_STATUS_ASSERT(rc == 0);
}

static void
boot_swap_sectors(size_t idx, size_t last_idx, uint32_t sz, struct boot_loader_state *state,
        struct boot_status *bs, const struct flash_area *fap_pri,
        const struct flash_area *fap_sec)
{
    uint32_t pri_off;
    uint32_t pri_up_off;
    uint32_t sec_off;
    int rc;

    pri_up_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
    pri_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx - 1);
    sec_off = boot_img_sector_off(state, BOOT_SECONDARY_SLOT, idx - 1);

    if (bs->state == BOOT_STATUS_STATE_0) {
        rc = boot_erase_region(fap_pri, pri_off, sz);
        assert(rc == 0);

        rc = boot_copy_region(state, fap_sec, fap_pri, sec_off, pri_off, sz);
        assert(rc == 0);

        rc = boot_write_status(state, bs);
        bs->state = BOOT_STATUS_STATE_1;
        BOOT_STATUS_ASSERT(rc == 0);
    }

    if (bs->state == BOOT_STATUS_STATE_1) {
        bool sector_erased_with_trailer = false;
        uint32_t copy_sz = sz;

        if (idx == last_idx) {
            rc = swap_scramble_trailer_sectors(state, fap_sec);
            assert(rc == 0);

            /* When starting a revert the swap status of the primary slot is erased then
             * re-initialized. If a reset occurs after the erasure but before the re-initialization
             * is complete, there has to be, somewhere, a flag indicating a revert is in progress.
             * Otherwise, the bootloader wouldn't be able to resume the revert operation.
             *
             * Initially, the swap-move algorithm was assuming the trailers were stored in dedicated
             * sectors and it was therefore possible to rewrite the secondary trailer before
             * starting the revert process, to make the revert look like a permanent upgrade in case
             * an unfortunate reset occurs during the rewriting of the primary trailer.
             *
             * However, considering now the first trailer sector could also hold firmware data, this
             * trick is no more possible since it could potentially erase part of the firmware image
             * to be restored. A solution is to rewrite here the secondary trailer with the
             * 'copy_done' flag set, meaning after an upgrade the secondary trailer is no more
             * erased. The bootloader will consider a revert must be started or resumed if the
             * primary image is not confirmed and the 'copy_done' flag is set in the secondary
             * trailer.
             */
            if (bs->swap_type != BOOT_SWAP_TYPE_REVERT) {
                rc = boot_write_copy_done(fap_sec);
                assert(rc == 0);

                rc = boot_write_magic(fap_sec);
                assert(rc == 0);
            }

            size_t first_trailer_sector_pri =
                boot_get_first_trailer_sector(state, BOOT_PRIMARY_SLOT);
            size_t first_trailer_sector_sec =
                boot_get_first_trailer_sector(state, BOOT_SECONDARY_SLOT);

            if (first_trailer_sector_sec == idx - 1) {
                /* The destination sector was containing part of the trailer and has therefore
                 * already been erased.
                 */
                sector_erased_with_trailer = true;
            }

            if (first_trailer_sector_pri == idx) {
                /* The source sector contains both firmware and trailer data, so only the firmware
                 * data must be copied to the destination sector.
                 *
                 * Swap-move => constant sector size, so 'sz' is the size of a sector and 'swap_size
                 * % sz' gives the number of bytes used by the largest firmware image in the last
                 * sector to be moved.
                 */
                copy_sz = bs->swap_size % sz;
            }
        }

        if (!sector_erased_with_trailer) {
            rc = boot_erase_region(fap_sec, sec_off, sz);
            assert(rc == 0);
        }

        rc = boot_copy_region(state, fap_pri, fap_sec, pri_up_off, sec_off, copy_sz);
        assert(rc == 0);

        rc = boot_write_status(state, bs);
        bs->idx++;
        bs->state = BOOT_STATUS_STATE_0;
        BOOT_STATUS_ASSERT(rc == 0);
    }
}

void
swap_run(struct boot_loader_state *state, struct boot_status *bs,
         uint32_t copy_size)
{
    uint32_t sector_sz;
    uint32_t idx;
    uint32_t last_idx;
    const struct flash_area *fap_pri;
    const struct flash_area *fap_sec;

    BOOT_LOG_INF("Starting swap using move algorithm.");

    last_idx = find_last_idx(state, copy_size);
    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);

    fap_pri = BOOT_IMG_AREA(state, BOOT_PRIMARY_SLOT);
    assert(fap_pri != NULL);

    fap_sec = BOOT_IMG_AREA(state, BOOT_SECONDARY_SLOT);
    assert(fap_sec != NULL);

    if (bs->op == BOOT_STATUS_OP_MOVE) {
        idx = last_idx;
        while (idx > 0) {
            if (idx <= (last_idx - bs->idx + 1)) {
                boot_move_sector_up(idx, sector_sz, state, bs, fap_pri);
            }
            idx--;
        }
        bs->idx = BOOT_STATUS_IDX_0;
    }

    bs->op = BOOT_STATUS_OP_SWAP;

    idx = 1;
    while (idx <= last_idx) {
        if (idx >= bs->idx) {
            boot_swap_sectors(idx, last_idx, sector_sz, state, bs, fap_pri, fap_sec);
        }
        idx++;
    }
}

int app_max_size(struct boot_loader_state *state)
{
    uint32_t sector_sz_primary;
    uint32_t sector_sz_secondary;
    uint32_t sz_primary;
    uint32_t sz_secondary;

    sector_sz_primary = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    sector_sz_secondary = boot_img_sector_size(state, BOOT_SECONDARY_SLOT, 0);

    /* Account for image flags and move sector */
    sz_primary = app_max_sectors(state) * sector_sz_primary - sector_sz_primary;
    sz_secondary = boot_img_num_sectors(state, BOOT_SECONDARY_SLOT) * sector_sz_secondary;

    return (sz_primary <= sz_secondary ? sz_primary : sz_secondary);
}

#endif
