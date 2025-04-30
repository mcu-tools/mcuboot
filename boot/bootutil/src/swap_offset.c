/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 JUUL Labs
 * Copyright (c) 2025 Nordic Semiconductor ASA
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

#ifdef MCUBOOT_SWAP_USING_OFFSET

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

#if defined(MCUBOOT_ENC_IMAGES)
#define BOOT_COPY_REGION(state, fap_pri, fap_sec, pri_off, sec_off, sz, sector_off) \
        boot_copy_region(state, fap_pri, fap_sec, pri_off, sec_off, sz, sector_off)
#else
#define BOOT_COPY_REGION(state, fap_pri, fap_sec, pri_off, sec_off, sz, sector_off) \
        boot_copy_region(state, fap_pri, fap_sec, pri_off, sec_off, sz)
#endif

uint32_t find_last_idx(struct boot_loader_state *state, uint32_t swap_size)
{
    uint32_t sector_sz;
    uint32_t sz;
    uint32_t last_idx;

    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    sz = 0;
    last_idx = 0;

    while (1) {
        sz += sector_sz;
        if (sz >= swap_size) {
            break;
        }
        last_idx++;
    }

    return last_idx;
}

int boot_read_image_header(struct boot_loader_state *state, int slot,
                           struct image_header *out_hdr, struct boot_status *bs)
{
    const struct flash_area *fap = NULL;
    uint32_t off = 0;
    uint32_t sz;
    uint32_t last_idx;
    uint32_t swap_size;
    int rc;
    bool check_other_sector = true;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    if (bs == NULL) {
        fap = BOOT_IMG_AREA(state, slot);

        if (slot == BOOT_SECONDARY_SLOT &&
            boot_swap_type_multi(BOOT_CURR_IMG(state)) != BOOT_SWAP_TYPE_REVERT) {
            off = boot_img_sector_size(state, BOOT_SECONDARY_SLOT, 0);
        }
    } else {
        if (!boot_status_is_reset(bs)) {
            check_other_sector = false;
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
            if (bs->swap_type == BOOT_SWAP_TYPE_REVERT ||
                boot_swap_type_multi(BOOT_CURR_IMG(state)) == BOOT_SWAP_TYPE_REVERT) {
                if (slot == 0) {
                    if (((bs->idx - BOOT_STATUS_IDX_0) > last_idx ||
                         ((bs->idx - BOOT_STATUS_IDX_0) == last_idx &&
                          bs->state == BOOT_STATUS_STATE_1))) {
                        slot = 1;
                        off = sz;
                    } else {
                        slot = 0;
                        off = 0;
                    }
                } else if (slot == 1) {
                    if ((bs->idx - BOOT_STATUS_IDX_0) > last_idx ||
                        ((bs->idx - BOOT_STATUS_IDX_0) == last_idx &&
                         bs->state == BOOT_STATUS_STATE_2)) {
                        slot = 0;
                        off = 0;
                    } else {
                        slot = 1;
                        off = 0;
                    }
                }
            } else {
                if (slot == 0) {
                    if ((bs->idx > BOOT_STATUS_IDX_0 ||
                         (bs->idx == BOOT_STATUS_IDX_0 && bs->state == BOOT_STATUS_STATE_1)) &&
                        bs->idx <= last_idx) {
                        slot = 1;
                        off = 0;
                    } else {
                        slot = 0;
                        off = 0;
                    }
                } else if (slot == 1) {
                    if (bs->idx > BOOT_STATUS_IDX_0) {
                        slot = 0;
                        off = 0;
                    } else {
                        slot = 1;
                        off = sz;
                    }
                }
            }

            fap = BOOT_IMG_AREA(state, slot);
        } else {
            fap = BOOT_IMG_AREA(state, slot);

            if (bs->swap_type == BOOT_SWAP_TYPE_REVERT ||
                boot_swap_type_multi(BOOT_CURR_IMG(state)) == BOOT_SWAP_TYPE_REVERT) {
                off = 0;
            }
            else if (slot == BOOT_SECONDARY_SLOT) {
                off = boot_img_sector_size(state, BOOT_SECONDARY_SLOT, 0);
            }
        }
    }

    assert(fap != NULL);

    rc = flash_area_read(fap, off, out_hdr, sizeof *out_hdr);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    if (check_other_sector == true && out_hdr->ih_magic != IMAGE_MAGIC &&
        slot == BOOT_SECONDARY_SLOT) {
        if (boot_swap_type_multi(BOOT_CURR_IMG(state)) != BOOT_SWAP_TYPE_REVERT) {
            off = 0;
        } else {
            off = boot_img_sector_size(state, BOOT_SECONDARY_SLOT, 0);
        }

        rc = flash_area_read(fap, off, out_hdr, sizeof(*out_hdr));
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }
    }

#if defined(MCUBOOT_BOOTSTRAP)
    if (out_hdr->ih_magic == IMAGE_MAGIC && (bs != NULL || state->bootstrap_secondary_offset_set[
                                                               BOOT_CURR_IMG(state)] == false) &&
        slot == BOOT_SECONDARY_SLOT) {
        state->bootstrap_secondary_offset_set[BOOT_CURR_IMG(state)] = true;
#else
    if (out_hdr->ih_magic == IMAGE_MAGIC && bs != NULL && slot == BOOT_SECONDARY_SLOT) {
#endif
        state->secondary_offset[BOOT_CURR_IMG(state)] = off;
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

int swap_read_status_bytes(const struct flash_area *fap, struct boot_loader_state *state,
                           struct boot_status *bs)
{
    uint32_t off;
    uint8_t status;
    int max_entries;
    int found_idx;
    uint8_t write_sz;
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
    /* Skip erased sectors at the end */
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
        /* This means there was an error writing status on the last swap. Tell user and move on
         * to validation!
         */
#if !defined(__BOOTSIM__)
        BOOT_LOG_ERR("Detected inconsistent status!");
#endif

#if !defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
        /* With validation of the primary slot disabled, there is no way to be sure the swapped
         * primary slot is OK, so abort!
         */
        assert(0);
#endif
    }

    if (found_idx == -1) {
        /* no swap status found; nothing to do */
    } else {
        bs->op = BOOT_STATUS_OP_SWAP;
        bs->idx = (found_idx / BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_IDX_0;
        bs->state = (found_idx % BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_STATE_0;
    }

    return 0;
}

uint32_t boot_status_internal_off(const struct boot_status *bs, int elem_sz)
{
    uint32_t off;
    int idx_sz;

    idx_sz = elem_sz * BOOT_STATUS_STATE_COUNT;
    off = (bs->idx - BOOT_STATUS_IDX_0) * idx_sz +
          (bs->state - BOOT_STATUS_STATE_0) * elem_sz;

    return off;
}

static int app_max_sectors(struct boot_loader_state *state)
{
    uint32_t sz = 0;
    uint32_t sector_sz;
    uint32_t trailer_sz;
    uint32_t available_sectors_pri;
    uint32_t available_sectors_sec;
    uint32_t trailer_sectors = 0;

    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));

    while (1) {
        sz += sector_sz;
        ++trailer_sectors;

        if  (sz >= trailer_sz) {
            break;
        }
    }

    available_sectors_pri = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT) - trailer_sectors;
    available_sectors_sec = boot_img_num_sectors(state, BOOT_SECONDARY_SLOT) - 1;

    return (available_sectors_pri < available_sectors_sec ? available_sectors_pri : available_sectors_sec);
}

int boot_slots_compatible(struct boot_loader_state *state)
{
    size_t num_sectors_pri;
    size_t num_sectors_sec;
    size_t sector_sz_pri = 0;
    size_t sector_sz_sec = 0;
    size_t i;
    size_t num_usable_sectors;

    num_sectors_pri = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT);
    num_sectors_sec = boot_img_num_sectors(state, BOOT_SECONDARY_SLOT);
    num_usable_sectors = app_max_sectors(state);

    if (num_sectors_pri != num_sectors_sec &&
        (num_sectors_pri + 1) != num_sectors_sec &&
        num_usable_sectors != (num_sectors_sec - 1)) {
        BOOT_LOG_WRN("Cannot upgrade: not a compatible amount of sectors");
        BOOT_LOG_DBG("slot0 sectors: %d, slot1 sectors: %d, usable sectors: %d",
                     (int)num_sectors_pri, (int)num_sectors_sec,
                     (int)(num_usable_sectors));
        return 0;
    } else if (num_sectors_pri > BOOT_MAX_IMG_SECTORS) {
        BOOT_LOG_WRN("Cannot upgrade: more sectors than allowed");
        return 0;
    }

    if ((num_usable_sectors + 1) != num_sectors_sec) {
        BOOT_LOG_DBG("Non-optimal sector distribution, slot0 has %d usable sectors "
                     "but slot1 has %d usable sectors", (int)(num_usable_sectors),
                     ((int)num_sectors_sec - 1));
    }

    for (i = 0; i < num_usable_sectors; i++) {
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

int swap_status_source(struct boot_loader_state *state)
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
        state_primary_slot.copy_done == BOOT_FLAG_UNSET &&
        state_secondary_slot.magic != BOOT_MAGIC_GOOD) {

        source = BOOT_STATUS_SOURCE_PRIMARY_SLOT;

        BOOT_LOG_INF("Boot source: primary slot");
        return source;
    }

    BOOT_LOG_INF("Boot source: none");
    return BOOT_STATUS_SOURCE_NONE;
}

static void boot_swap_sectors(int idx, uint32_t sz, struct boot_loader_state *state,
                              struct boot_status *bs, const struct flash_area *fap_pri,
                              const struct flash_area *fap_sec, bool skip_primary,
                              bool skip_secondary)
{
    uint32_t pri_off;
    uint32_t sec_off;
    uint32_t sec_up_off;
    int rc = 0;

    pri_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
    sec_off = boot_img_sector_off(state, BOOT_SECONDARY_SLOT, idx);
    sec_up_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, (idx + 1));

    if (bs->state == BOOT_STATUS_STATE_0) {
        if (skip_primary == true) {
            BOOT_LOG_DBG("Skipping erase of secondary 0x%x and copy from primary 0x%x", sec_off,
                         pri_off);
        } else {
            /* Copy from slot 0 X to slot 1 X */
            BOOT_LOG_DBG("Erasing secondary 0x%x of 0x%x", sec_off, sz);
            rc = boot_erase_region(fap_sec, sec_off, sz, false);
            assert(rc == 0);

            BOOT_LOG_DBG("Copying primary 0x%x -> secondary 0x%x of 0x%x", pri_off, sec_off, sz);
            rc = BOOT_COPY_REGION(state, fap_pri, fap_sec, pri_off, sec_off, sz, 0);
            assert(rc == 0);
        }

        rc = boot_write_status(state, bs);
        bs->state = BOOT_STATUS_STATE_1;
        BOOT_STATUS_ASSERT(rc == 0);
    }

    if (bs->state == BOOT_STATUS_STATE_1) {
        if (skip_secondary == true) {
            BOOT_LOG_DBG("Skipping erase of primary 0x%x and copy from secondary 0x%x", pri_off,
                         sec_up_off);
        } else {
            /* Erase slot 0 X */
            BOOT_LOG_DBG("Erasing primary 0x%x of 0x%x", pri_off, sz);
            rc = boot_erase_region(fap_pri, pri_off, sz, false);
            assert(rc == 0);

            /* Copy from slot 1 (X + 1) to slot 0 X */
            BOOT_LOG_DBG("Copying secondary 0x%x -> primary 0x%x of 0x%x", sec_up_off, pri_off,
                         sz);
            rc = BOOT_COPY_REGION(state, fap_sec, fap_pri, sec_up_off, pri_off, sz, 0);
            assert(rc == 0);
        }

        rc = boot_write_status(state, bs);
        bs->idx++;
        bs->state = BOOT_STATUS_STATE_0;
        BOOT_STATUS_ASSERT(rc == 0);
    }
}

static void boot_swap_sectors_revert(int idx, uint32_t sz, struct boot_loader_state *state,
                                     struct boot_status *bs, const struct flash_area *fap_pri,
                                     const struct flash_area *fap_sec, uint32_t sector_sz,
                                     bool skip_primary, bool skip_secondary)
{
    uint32_t pri_off;
    uint32_t sec_off;
    uint32_t sec_up_off;
    int rc = 0;
#if !defined(MCUBOOT_ENC_IMAGES)
    (void)sector_sz;
#endif

    pri_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
    sec_off = boot_img_sector_off(state, BOOT_SECONDARY_SLOT, idx + 1);
    sec_up_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);

    if (bs->state == BOOT_STATUS_STATE_0) {
        if (skip_primary == true) {
            BOOT_LOG_DBG("Skipping erase of secondary 0x%x and copy from primary 0x%x", sec_off,
                         pri_off);
        } else {
            /* Copy from slot 0 X to slot 1 X */
            BOOT_LOG_DBG("Erasing secondary 0x%x of 0x%x", sec_off, sz);
            rc = boot_erase_region(fap_sec, sec_off, sz, false);
            assert(rc == 0);

            BOOT_LOG_DBG("Copying primary 0x%x -> secondary 0x%x of 0x%x", pri_off, sec_off, sz);
            rc = BOOT_COPY_REGION(state, fap_pri, fap_sec, pri_off, sec_off, sz, sector_sz);
            assert(rc == 0);
        }

        rc = boot_write_status(state, bs);
        bs->state = BOOT_STATUS_STATE_1;
        BOOT_STATUS_ASSERT(rc == 0);
    }

    if (bs->state == BOOT_STATUS_STATE_1) {
        if (skip_secondary == true) {
            BOOT_LOG_DBG("Skipping erase of primary 0x%x and copy from secondary 0x%x", pri_off,
                         sec_up_off);
        } else {
            /* Erase slot 0 X */
            BOOT_LOG_DBG("Erasing primary 0x%x of 0x%x", pri_off, sz);
            rc = boot_erase_region(fap_pri, pri_off, sz, false);
            assert(rc == 0);

            /* Copy from slot 1 (X + 1) to slot 0 X */
            BOOT_LOG_DBG("Copying secondary 0x%x -> primary 0x%x of 0x%x", sec_up_off, pri_off,
                         sz);
            rc = BOOT_COPY_REGION(state, fap_sec, fap_pri, sec_up_off, pri_off, sz, 0);
            assert(rc == 0);
        }

        rc = boot_write_status(state, bs);
        bs->idx++;
        bs->state = BOOT_STATUS_STATE_0;
        BOOT_STATUS_ASSERT(rc == 0);
    }
}

/*
 * When starting a revert the swap status exists in the primary slot, and
 * the status in the secondary slot is erased. To start the swap, the status
 * area in the primary slot must be re-initialized; if during the small
 * window of time between re-initializing it and writing the first metadata
 * a reset happens, the swap process is broken and cannot be resumed.
 *
 * This function handles the issue by making the revert look like a permanent
 * upgrade (by initializing the secondary slot).
 */
void fixup_revert(const struct boot_loader_state *state, struct boot_status *bs,
                  const struct flash_area *fap_sec)
{
    struct boot_swap_state swap_state;
    int rc;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    /* No fixup required */
    if (bs->swap_type != BOOT_SWAP_TYPE_REVERT ||
        bs->idx != BOOT_STATUS_IDX_0) {
        return;
    }

    rc = boot_read_swap_state(fap_sec, &swap_state);
    assert(rc == 0);

    BOOT_LOG_SWAP_STATE("Secondary image", &swap_state);

    if (swap_state.magic == BOOT_MAGIC_UNSET) {
        rc = swap_scramble_trailer_sectors(state, fap_sec);
        assert(rc == 0);

        rc = boot_write_copy_done(fap_sec);
        assert(rc == 0);

        rc = swap_status_init(state, fap_sec, bs);
        assert(rc == 0);
    }
}

void swap_run(struct boot_loader_state *state, struct boot_status *bs,
              uint32_t copy_size)
{
    uint32_t sz;
    uint32_t sector_sz;
    uint32_t idx;
    uint32_t trailer_sz;
    uint32_t first_trailer_idx;
    uint32_t last_idx;
    uint32_t used_sectors_pri;
    uint32_t used_sectors_sec;
    const struct flash_area *fap_pri = NULL;
    const struct flash_area *fap_sec = NULL;
    int rc;

    BOOT_LOG_INF("Starting swap using offset algorithm.");

    last_idx = find_last_idx(state, copy_size);
    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);

    /* When starting a new swap upgrade, check that there is enough space */
    if (boot_status_is_reset(bs)) {
        sz = 0;
        trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));
        first_trailer_idx = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT) - 1;

        while (1) {
            sz += sector_sz;
            if  (sz >= trailer_sz) {
                break;
            }
            first_trailer_idx--;
        }

        if (last_idx >= first_trailer_idx) {
            BOOT_LOG_WRN("Not enough free space to run swap upgrade");
            BOOT_LOG_WRN("required %d bytes but only %d are available",
                         (last_idx + 1) * sector_sz,
                         first_trailer_idx * sector_sz);
            bs->swap_type = BOOT_SWAP_TYPE_NONE;
            return;
        }
    }

    fap_pri = BOOT_IMG_AREA(state, BOOT_PRIMARY_SLOT);
    assert(fap_pri != NULL);

    fap_sec = BOOT_IMG_AREA(state, BOOT_SECONDARY_SLOT);
    assert(fap_sec != NULL);

    fixup_revert(state, bs, fap_sec);

    /* Init areas for storing swap status */
    if (bs->idx == BOOT_STATUS_IDX_0) {
        int rc;

        if (bs->source != BOOT_STATUS_SOURCE_PRIMARY_SLOT) {
            rc = swap_scramble_trailer_sectors(state, fap_pri);
            assert(rc == 0);

            rc = swap_status_init(state, fap_pri, bs);
            assert(rc == 0);
        }

        rc = swap_scramble_trailer_sectors(state, fap_sec);
        assert(rc == 0);
    }

    bs->op = BOOT_STATUS_OP_SWAP;
    idx = 0;
    used_sectors_pri = ((state->imgs[BOOT_CURR_IMG(state)][BOOT_PRIMARY_SLOT].hdr.ih_hdr_size +
        state->imgs[BOOT_CURR_IMG(state)][BOOT_PRIMARY_SLOT].hdr.ih_protect_tlv_size +
        state->imgs[BOOT_CURR_IMG(state)][BOOT_PRIMARY_SLOT].hdr.ih_img_size) + sector_sz - 1) /
        sector_sz;
    used_sectors_sec = ((state->imgs[BOOT_CURR_IMG(state)][BOOT_SECONDARY_SLOT].hdr.ih_hdr_size +
        state->imgs[BOOT_CURR_IMG(state)][BOOT_SECONDARY_SLOT].hdr.ih_protect_tlv_size +
        state->imgs[BOOT_CURR_IMG(state)][BOOT_SECONDARY_SLOT].hdr.ih_img_size) + sector_sz - 1) /
        sector_sz;

    if (bs->swap_type == BOOT_SWAP_TYPE_REVERT ||
        boot_swap_type_multi(BOOT_CURR_IMG(state)) == BOOT_SWAP_TYPE_REVERT) {
        while (idx <= last_idx) {
            if (idx >= (bs->idx - BOOT_STATUS_IDX_0)) {
                uint32_t mirror_idx = last_idx - idx;

                boot_swap_sectors_revert(mirror_idx, sector_sz, state, bs, fap_pri, fap_sec,
                                         sector_sz,
                                         (mirror_idx > used_sectors_pri ? true : false),
                                         (mirror_idx > used_sectors_sec ? true : false));
            }

            idx++;
        }

        /* Erase the first sector in the secondary slot before completing revert so that the
         * status is not wrongly used as a valid header. Also erase the trailer in the secondary
         * to allow for a future update to be loaded
         */
        rc = boot_scramble_region(fap_sec, boot_img_sector_off(state, BOOT_SECONDARY_SLOT, 0),
                                  sector_sz, false);
        assert(rc == 0);
        rc = swap_scramble_trailer_sectors(state, fap_sec);
        assert(rc == 0);
    } else {
        while (idx <= last_idx) {
            if (idx >= (bs->idx - BOOT_STATUS_IDX_0)) {
                boot_swap_sectors(idx, sector_sz, state, bs, fap_pri, fap_sec,
                                  (idx > used_sectors_pri ? true : false),
                                  (idx > used_sectors_sec ? true : false));
            }

            idx++;
        }
    }
}

int app_max_size(struct boot_loader_state *state)
{
    uint32_t sector_sz_primary;

    sector_sz_primary = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);

    return app_max_sectors(state) * sector_sz_primary;
}

/* Compute the total size of the given image. Includes the size of the TLVs. */
int boot_read_image_size(struct boot_loader_state *state, int slot, uint32_t *size)
{
    const struct flash_area *fap;
    struct image_tlv_info info;
    uint32_t off;
    uint32_t secondary_slot_off = 0;
    uint32_t protect_tlv_size;
    int rc;

#if (BOOT_IMAGE_NUMBER == 1)
    (void)state;
#endif

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);
    assert(size != NULL);

    *size = 0;

    off = BOOT_TLV_OFF(boot_img_hdr(state, slot));

    if (slot == BOOT_SECONDARY_SLOT) {
        /* Check in the secondary position in the upgrade slot */
        secondary_slot_off = state->secondary_offset[BOOT_CURR_IMG(state)];
    }

    if (flash_area_read(fap, (off + secondary_slot_off), &info, sizeof(info))) {
        rc = BOOT_EFLASH;
        goto done;
    }

    protect_tlv_size = boot_img_hdr(state, slot)->ih_protect_tlv_size;
    if (info.it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
        if (protect_tlv_size != info.it_tlv_tot) {
            rc = BOOT_EBADIMAGE;
            goto done;
        }

        if (flash_area_read(fap, (off + secondary_slot_off + info.it_tlv_tot),
                            &info, sizeof(info))) {
            rc = BOOT_EFLASH;
            goto done;
        }
    } else if (protect_tlv_size != 0) {
        rc = BOOT_EBADIMAGE;
        goto done;
    }

    if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = BOOT_EBADIMAGE;
        goto done;
    }

    *size = off + protect_tlv_size + info.it_tlv_tot;
    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

#endif
