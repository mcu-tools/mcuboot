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

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "swap_priv.h"
#include "swap_status.h"
#include "bootutil/bootutil_log.h"

#include "mcuboot_config/mcuboot_config.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_SWAP_USING_STATUS

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

static uint32_t g_last_idx = UINT32_MAX;

int
boot_read_image_header(struct boot_loader_state *state, int slot,
                       struct image_header *out_hdr, struct boot_status *bs)
{
//    const struct flash_area *fap;
//    uint32_t off;
//    uint32_t sz;
//    int area_id;
    int rc;
//
//#if (BOOT_IMAGE_NUMBER == 1)
//    (void)state;
//#endif
//
//    off = 0;
//    if (bs) {
//        sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
//        if (bs->op == BOOT_STATUS_OP_MOVE) {
//            if (slot == 0 && bs->idx > g_last_idx) {
//                /* second sector */
//                off = sz;
//            }
//        } else if (bs->op == BOOT_STATUS_OP_SWAP) {
//            if (bs->idx > 1 && bs->idx <= g_last_idx) {
//                if (slot == 0) {
//                    slot = 1;
//                } else {
//                    slot = 0;
//                }
//            } else if (bs->idx == 1) {
//                if (slot == 0) {
//                    off = sz;
//                }
//                if (slot == 1 && bs->state == 2) {
//                    slot = 0;
//                }
//            }
//        }
//    }
//
//    area_id = flash_area_id_from_multi_image_slot(BOOT_CURR_IMG(state), slot);
//    rc = flash_area_open(area_id, &fap);
//    if (rc != 0) {
//        rc = BOOT_EFLASH;
//        goto done;
//    }
//
//    rc = flash_area_read(fap, off, out_hdr, sizeof *out_hdr);
//    if (rc != 0) {
//        rc = BOOT_EFLASH;
//        goto done;
//    }
//
//    /* We only know where the headers are located when bs is valid */
//    if (bs != NULL && out_hdr->ih_magic != IMAGE_MAGIC) {
//        rc = -1;
//        goto done;
//    }
//
//    rc = 0;
//
//done:
//    flash_area_close(fap);
    return rc;
}

int
swap_read_status_bytes(const struct flash_area *fap,
        struct boot_loader_state *state, struct boot_status *bs)
{
//    uint32_t off;
//    uint8_t status;
//    int max_entries;
//    int found_idx;
//    uint8_t write_sz;
//    int move_entries;
//    int rc;
//    int last_rc;
//    int erased_sections;
//    int i;
//
//    max_entries = boot_status_entries(BOOT_CURR_IMG(state), fap);
//    if (max_entries < 0) {
//        return BOOT_EBADARGS;
//    }
//
//    erased_sections = 0;
//    found_idx = -1;
//    /* skip erased sectors at the end */
//    last_rc = 1;
//    write_sz = BOOT_WRITE_SZ(state);
//    off = boot_status_off(fap);
//    for (i = max_entries; i > 0; i--) {
//        rc = flash_area_read_is_empty(fap, off + (i - 1) * write_sz, &status, 1);
//        if (rc < 0) {
//            return BOOT_EFLASH;
//        }
//
//        if (rc == 1) {
//            if (rc != last_rc) {
//                erased_sections++;
//            }
//        } else {
//            if (found_idx == -1) {
//                found_idx = i;
//            }
//        }
//        last_rc = rc;
//    }
//
//    if (erased_sections > 1) {
//        /* This means there was an error writing status on the last
//         * swap. Tell user and move on to validation!
//         */
//#if !defined(__BOOTSIM__)
//        BOOT_LOG_ERR("Detected inconsistent status!");
//#endif
//
//#if !defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
//        /* With validation of the primary slot disabled, there is no way
//         * to be sure the swapped primary slot is OK, so abort!
//         */
//        assert(0);
//#endif
//    }
//
//    move_entries = BOOT_MAX_IMG_SECTORS * BOOT_STATUS_MOVE_STATE_COUNT;
//    if (found_idx == -1) {
//        /* no swap status found; nothing to do */
//    } else if (found_idx < move_entries) {
//        bs->op = BOOT_STATUS_OP_MOVE;
//        bs->idx = (found_idx  / BOOT_STATUS_MOVE_STATE_COUNT) + BOOT_STATUS_IDX_0;
//        bs->state = (found_idx % BOOT_STATUS_MOVE_STATE_COUNT) + BOOT_STATUS_STATE_0;;
//    } else {
//        bs->op = BOOT_STATUS_OP_SWAP;
//        bs->idx = ((found_idx - move_entries) / BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_IDX_0;
//        bs->state = ((found_idx - move_entries) % BOOT_STATUS_SWAP_STATE_COUNT) + BOOT_STATUS_STATE_0;
//    }

    return 0;
}

uint32_t
boot_status_internal_off(const struct boot_status *bs, int elem_sz)
{
    uint32_t off;
//    int idx_sz;
//
//    idx_sz = elem_sz * ((bs->op == BOOT_STATUS_OP_MOVE) ?
//            BOOT_STATUS_MOVE_STATE_COUNT : BOOT_STATUS_SWAP_STATE_COUNT);
//
//    off = ((bs->op == BOOT_STATUS_OP_MOVE) ?
//               0 : (BOOT_MAX_IMG_SECTORS * BOOT_STATUS_MOVE_STATE_COUNT * elem_sz)) +
//           (bs->idx - BOOT_STATUS_IDX_0) * idx_sz +
//           (bs->state - BOOT_STATUS_STATE_0) * elem_sz;
//
    return off;
}

int
boot_slots_compatible(struct boot_loader_state *state)
{
//    size_t num_sectors;
//    size_t i;
//
//    num_sectors = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT);
//    if (num_sectors != boot_img_num_sectors(state, BOOT_SECONDARY_SLOT)) {
//        BOOT_LOG_WRN("Cannot upgrade: slots don't have same amount of sectors");
//        return 0;
//    }
//
//    if (num_sectors > BOOT_MAX_IMG_SECTORS) {
//        BOOT_LOG_WRN("Cannot upgrade: more sectors than allowed");
//        return 0;
//    }
//
//    for (i = 0; i < num_sectors; i++) {
//        if (boot_img_sector_size(state, BOOT_PRIMARY_SLOT, i) !=
//                boot_img_sector_size(state, BOOT_SECONDARY_SLOT, i)) {
//            BOOT_LOG_WRN("Cannot upgrade: not same sector layout");
//            return 0;
//        }
//    }

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
//    struct boot_swap_state state_primary_slot;
//    int rc;
//    uint8_t source;
//    uint8_t image_index;
//
//#if (BOOT_IMAGE_NUMBER == 1)
//    (void)state;
//#endif
//
//    image_index = BOOT_CURR_IMG(state);
//
//    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_PRIMARY(image_index),
//            &state_primary_slot);
//    assert(rc == 0);
//
//    BOOT_LOG_SWAP_STATE("Primary image", &state_primary_slot);
//
//    if (state_primary_slot.magic == BOOT_MAGIC_GOOD &&
//            state_primary_slot.copy_done == BOOT_FLAG_UNSET) {
//
//        source = BOOT_STATUS_SOURCE_PRIMARY_SLOT;
//
//        BOOT_LOG_INF("Boot source: primary slot");
//        return source;
//    }
//
//    BOOT_LOG_INF("Boot source: none");
//    return BOOT_STATUS_SOURCE_NONE;
}

/*
 * "Moves" the sector located at idx - 1 to idx.
 */
static void
boot_move_sector_up(int idx, uint32_t sz, struct boot_loader_state *state,
        struct boot_status *bs, const struct flash_area *fap_pri)
{
    uint32_t new_off;
    uint32_t old_off;
//    const struct flash_area *fap_stat;
    int rc;

//    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
//    assert (rc == 0);

    /* Calculate offset from start of image area. */
    new_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
    old_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx - 1);

    if (bs->idx == BOOT_STATUS_IDX_0) {
        if (bs->source != BOOT_STATUS_SOURCE_PRIMARY_SLOT) {
            rc = swap_erase_trailer_sectors(state, fap_pri);
            assert(rc == 0);

            rc = swap_status_init(state, fap_pri, bs);
            assert(rc == 0);
        }

//        rc = swap_erase_trailer_sectors(state, fap_sec);
//        assert(rc == 0);
    }
//
//    rc = boot_erase_region(fap_pri, new_off, sz);
//    assert(rc == 0);
//
//    rc = boot_copy_region(state, fap_pri, fap_pri, old_off, new_off, sz);
//    assert(rc == 0);
//
//    rc = boot_write_status(state, bs);
//
//    bs->idx++;
//    BOOT_STATUS_ASSERT(rc == 0);
}

static void
boot_swap_sectors(int idx, uint32_t sz, struct boot_loader_state *state,
        struct boot_status *bs, const struct flash_area *fap_pri,
        const struct flash_area *fap_sec)
{
//    uint32_t pri_off;
//    uint32_t pri_up_off;
//    uint32_t sec_off;
//    int rc;
//
//    pri_up_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx);
//    pri_off = boot_img_sector_off(state, BOOT_PRIMARY_SLOT, idx - 1);
//    sec_off = boot_img_sector_off(state, BOOT_SECONDARY_SLOT, idx - 1);
//
//    if (bs->state == BOOT_STATUS_STATE_0) {
//        rc = boot_erase_region(fap_pri, pri_off, sz);
//        assert(rc == 0);
//
//        rc = boot_copy_region(state, fap_sec, fap_pri, sec_off, pri_off, sz);
//        assert(rc == 0);
//
//        rc = boot_write_status(state, bs);
//        bs->state = BOOT_STATUS_STATE_1;
//        BOOT_STATUS_ASSERT(rc == 0);
//    }
//
//    if (bs->state == BOOT_STATUS_STATE_1) {
//        rc = boot_erase_region(fap_sec, sec_off, sz);
//        assert(rc == 0);
//
//        rc = boot_copy_region(state, fap_pri, fap_sec, pri_up_off, sec_off, sz);
//        assert(rc == 0);
//
//        rc = boot_write_status(state, bs);
//        bs->idx++;
//        bs->state = BOOT_STATUS_STATE_0;
//        BOOT_STATUS_ASSERT(rc == 0);
//    }
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
void
fixup_revert(const struct boot_loader_state *state, struct boot_status *bs,
        const struct flash_area *fap_sec, uint8_t sec_id)
{
//    struct boot_swap_state swap_state;
//    int rc;
//
//#if (BOOT_IMAGE_NUMBER == 1)
//    (void)state;
//#endif
//
//    /* No fixup required */
//    if (bs->swap_type != BOOT_SWAP_TYPE_REVERT ||
//        bs->op != BOOT_STATUS_OP_MOVE ||
//        bs->idx != BOOT_STATUS_IDX_0) {
//        return;
//    }
//
//    rc = boot_read_swap_state_by_id(sec_id, &swap_state);
//    assert(rc == 0);
//
//    BOOT_LOG_SWAP_STATE("Secondary image", &swap_state);
//
//    if (swap_state.magic == BOOT_MAGIC_UNSET) {
//        rc = swap_erase_trailer_sectors(state, fap_sec);
//        assert(rc == 0);
//
//        rc = boot_write_image_ok(fap_sec);
//        assert(rc == 0);
//
//        rc = boot_write_swap_size(fap_sec, bs->swap_size);
//        assert(rc == 0);
//
//        rc = boot_write_magic(fap_sec);
//        assert(rc == 0);
//    }
}

void
swap_run(struct boot_loader_state *state, struct boot_status *bs,
         uint32_t copy_size)
{
    uint32_t sz;
    uint32_t sector_sz;
    uint32_t idx;
    uint32_t trailer_sz;
    uint32_t first_trailer_idx;
    uint8_t image_index;
    const struct flash_area *fap_pri;
    const struct flash_area *fap_sec;
//    const struct flash_area *fap_stat;
    int rc;

    sz = 0;
    g_last_idx = 0;

    sector_sz = boot_img_sector_size(state, BOOT_PRIMARY_SLOT, 0);
    while (1) {
        sz += sector_sz;
        /* Skip to next sector because all sectors will be moved up. */
        g_last_idx++;
        if (sz >= copy_size) {
            break;
        }
    }

    /*
     * When starting a new swap upgrade, check that there is enough space.
     */
//    if (boot_status_is_reset(bs)) {
//        sz = 0;
//        trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));
//        first_trailer_idx = boot_img_num_sectors(state, BOOT_PRIMARY_SLOT) - 1;
//
//        while (1) {
//            sz += sector_sz;
//            if  (sz >= trailer_sz) {
//                break;
//            }
//            first_trailer_idx--;
//        }
//
//        if (g_last_idx >= first_trailer_idx) {
//            BOOT_LOG_WRN("Not enough free space to run swap upgrade");
//            bs->swap_type = BOOT_SWAP_TYPE_NONE;
//            return;
//        }
//    }

    image_index = BOOT_CURR_IMG(state);

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(image_index), &fap_pri);
    assert (rc == 0);

    rc = flash_area_open(FLASH_AREA_IMAGE_SECONDARY(image_index), &fap_sec);
    assert (rc == 0);

//    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
//    assert (rc == 0);

    // TODO: skipping revert in early development
//    fixup_revert(state, bs, fap_sec, FLASH_AREA_IMAGE_SECONDARY(image_index));

    if (bs->op == BOOT_STATUS_OP_MOVE) {
        idx = g_last_idx;
        while (idx > 0) {
            if (idx <= (g_last_idx - bs->idx + 1)) {
                boot_move_sector_up(idx, sector_sz, state, bs, fap_pri, fap_sec);
            }
            idx--;
        }
        bs->idx = BOOT_STATUS_IDX_0;
    }

    bs->op = BOOT_STATUS_OP_SWAP;

    idx = 1;
    while (idx <= g_last_idx) {
        if (idx >= bs->idx) {
            boot_swap_sectors(idx, sector_sz, state, bs, fap_pri, fap_sec);
        }
        idx++;
    }

    flash_area_close(fap_pri);
    flash_area_close(fap_sec);
//    flash_area_close(fap_stat);
}


/* MISC - section, early development */
/* Offset Section */

/*rnok: this function can be reused from bootutil_misc.c, 
        if not defined as static there */
static inline uint32_t
boot_magic_off(const struct flash_area *fap)
{
    return BOOT_SWAP_STATUS_D_SIZE_RAW - BOOT_MAGIC_SZ;
}

static inline uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    return boot_magic_off(fap) - 1;
}

static inline uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    return boot_image_ok_off(fap) - 1;
}

uint32_t
boot_swap_info_off(const struct flash_area *fap)
{
    return boot_copy_done_off(fap) - 1;
}

static inline uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return boot_swap_info_off(fap) - 4;
}

uint32_t
boot_status_off(const struct flash_area *fap)
{   
    /* this offset is equal to 0, because swap status fields
       in this implementation count from the start of partion */
    return 0;
}

#ifdef MCUBOOT_ENC_IMAGES
static inline uint32_t
boot_enc_key_off(const struct flash_area *fap, uint8_t slot)
{
//#if MCUBOOT_SWAP_SAVE_ENCTLV
//    return boot_swap_size_off(fap) - ((slot + 1) *
//            ((((BOOT_ENC_TLV_SIZE - 1) / BOOT_MAX_ALIGN) + 1) * BOOT_MAX_ALIGN));
//#else
//    return boot_swap_size_off(fap) - ((slot + 1) * BOOT_ENC_KEY_SIZE);
//#endif
}
#endif

/* Write Section */
int
boot_write_magic(const struct flash_area *fap)
{
//    uint32_t off;
//    int rc;
//
//    off = boot_magic_off(fap);
//
//    BOOT_LOG_DBG("writing magic; fa_id=%d off=0x%lx (0x%lx)",
//                 fap->fa_id, (unsigned long)off,
//                 (unsigned long)(fap->fa_off + off));
//    rc = flash_area_write(fap, off, boot_img_magic, BOOT_MAGIC_SZ);
//    if (rc != 0) {
//        return BOOT_EFLASH;
//    }

    return 0;
}

/**
 * Write trailer data; status bytes, swap_size, etc
 *
 * @returns 0 on success, != 0 on error.
 */
static int
boot_write_trailer(const struct flash_area *fap, uint32_t off,
        const uint8_t *inbuf, uint8_t inlen)
{
    // TODO: add call to swap_status_update() here ?
    int rc;

    rc = swap_status_update(fap->fa_id, off, inbuf, inlen);

//    uint8_t buf[BOOT_MAX_ALIGN];
//    uint8_t align;
//    uint8_t erased_val;
//    int rc;
//
//    align = flash_area_align(fap);
//    if (inlen > BOOT_MAX_ALIGN || align > BOOT_MAX_ALIGN) {
//        return -1;
//    }
//    erased_val = flash_area_erased_val(fap);
//    if (align < inlen) {
//        align = inlen;
//    }
//    memcpy(buf, inbuf, inlen);
//    memset(&buf[inlen], erased_val, align - inlen);
//
//    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return rc;
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_write_enc_key(const struct flash_area *fap, uint8_t slot,
        const struct boot_status *bs)
{
//    uint32_t off;
//    int rc;
//
//    off = boot_enc_key_off(fap, slot);
//    BOOT_LOG_DBG("writing enc_key; fa_id=%d off=0x%lx (0x%lx)",
//                 fap->fa_id, (unsigned long)off,
//                 (unsigned long)fap->fa_off + off);
//#if MCUBOOT_SWAP_SAVE_ENCTLV
//    rc = flash_area_write(fap, off, bs->enctlv[slot], BOOT_ENC_TLV_ALIGN_SIZE);
//#else
//    rc = flash_area_write(fap, off, bs->enckey[slot], BOOT_ENC_KEY_SIZE);
//#endif
//    if (rc != 0) {
//        return BOOT_EFLASH;
//    }

    return 0;
}
#endif

/* OTHER APIs */
static inline size_t
boot_status_sector_size(const struct boot_loader_state *state, size_t sector)
{
    return state->status.sectors[sector].fs_size;
}

int boot_status_num_sectors(const struct boot_loader_state *state)
{
    return (int)(BOOT_SWAP_STATUS_SIZE / boot_status_sector_size(state, 0));
}

static inline uint32_t
boot_status_sector_off(const struct boot_loader_state *state,
                    size_t sector)
{
    return state->status.sectors[sector].fs_off -
           state->status.sectors[0].fs_off;
}

#endif
