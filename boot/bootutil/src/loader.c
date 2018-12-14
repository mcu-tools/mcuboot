/*
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
 * This file provides an interface to the boot loader.  Functions defined in
 * this file should only be called while the boot loader is running.
 */

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <os/os_malloc.h>
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

#include "mcuboot_config/mcuboot_config.h"

static struct boot_loader_state boot_data;

#if defined(MCUBOOT_VALIDATE_SLOT0) && !defined(MCUBOOT_OVERWRITE_ONLY)
static int boot_status_fails = 0;
#define BOOT_STATUS_ASSERT(x)                \
    do {                                     \
        if (!(x)) {                          \
            boot_status_fails++;             \
        }                                    \
    } while (0)
#else
#define BOOT_STATUS_ASSERT(x) ASSERT(x)
#endif

struct boot_status_table {
    uint8_t bst_magic_slot0;
    uint8_t bst_magic_scratch;
    uint8_t bst_copy_done_slot0;
    uint8_t bst_status_source;
};

/**
 * This set of tables maps swap state contents to boot status location.
 * When searching for a match, these tables must be iterated in order.
 */
static const struct boot_status_table boot_status_tables[] = {
    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Good       | Any        |
         * copy-done | Set        | N/A        |
         * ----------+------------+------------'
         * source: none                        |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bst_magic_scratch =    BOOT_MAGIC_ANY,
        .bst_copy_done_slot0 =  BOOT_FLAG_SET,
        .bst_status_source =    BOOT_STATUS_SOURCE_NONE,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Good       | Any        |
         * copy-done | Unset      | N/A        |
         * ----------+------------+------------'
         * source: slot 0                      |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bst_magic_scratch =    BOOT_MAGIC_ANY,
        .bst_copy_done_slot0 =  BOOT_FLAG_UNSET,
        .bst_status_source =    BOOT_STATUS_SOURCE_SLOT0,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Any        | Good       |
         * copy-done | Any        | N/A        |
         * ----------+------------+------------'
         * source: scratch                     |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_ANY,
        .bst_magic_scratch =    BOOT_MAGIC_GOOD,
        .bst_copy_done_slot0 =  BOOT_FLAG_ANY,
        .bst_status_source =    BOOT_STATUS_SOURCE_SCRATCH,
    },
    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Unset      | Any        |
         * copy-done | Unset      | N/A        |
         * ----------+------------+------------|
         * source: varies                      |
         * ------------------------------------+------------------------------+
         * This represents one of two cases:                                  |
         * o No swaps ever (no status to read, so no harm in checking).       |
         * o Mid-revert; status in slot 0.                                    |
         * -------------------------------------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_UNSET,
        .bst_magic_scratch =    BOOT_MAGIC_ANY,
        .bst_copy_done_slot0 =  BOOT_FLAG_UNSET,
        .bst_status_source =    BOOT_STATUS_SOURCE_SLOT0,
    },
};

#define BOOT_STATUS_TABLES_COUNT \
    (sizeof boot_status_tables / sizeof boot_status_tables[0])

#define BOOT_LOG_SWAP_STATE(area, state)                            \
    BOOT_LOG_INF("%s: magic=%s, copy_done=0x%x, image_ok=0x%x",     \
                 (area),                                            \
                 ((state)->magic == BOOT_MAGIC_GOOD ? "good" :      \
                  (state)->magic == BOOT_MAGIC_UNSET ? "unset" :    \
                  "bad"),                                           \
                 (state)->copy_done,                                \
                 (state)->image_ok)

/**
 * Determines where in flash the most recent boot status is stored.  The boot
 * status is necessary for completing a swap that was interrupted by a boot
 * loader reset.
 *
 * @return                      A BOOT_STATUS_SOURCE_[...] code indicating where *                                  status should be read from.
 */
static int
boot_status_source(void)
{
    const struct boot_status_table *table;
    struct boot_swap_state state_scratch;
    struct boot_swap_state state_slot0;
    int rc;
    size_t i;
    uint8_t source;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_0, &state_slot0);
    assert(rc == 0);

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SCRATCH, &state_scratch);
    assert(rc == 0);

    BOOT_LOG_SWAP_STATE("Image 0", &state_slot0);
    BOOT_LOG_SWAP_STATE("Scratch", &state_scratch);

    for (i = 0; i < BOOT_STATUS_TABLES_COUNT; i++) {
        table = &boot_status_tables[i];

        if ((table->bst_magic_slot0     == BOOT_MAGIC_ANY    ||
             table->bst_magic_slot0     == state_slot0.magic)   &&
            (table->bst_magic_scratch   == BOOT_MAGIC_ANY    ||
             table->bst_magic_scratch   == state_scratch.magic) &&
            (table->bst_copy_done_slot0 == BOOT_FLAG_ANY     ||
             table->bst_copy_done_slot0 == state_slot0.copy_done)) {
            source = table->bst_status_source;
            BOOT_LOG_INF("Boot source: %s",
                         source == BOOT_STATUS_SOURCE_NONE ? "none" :
                         source == BOOT_STATUS_SOURCE_SCRATCH ? "scratch" :
                         source == BOOT_STATUS_SOURCE_SLOT0 ? "slot 0" :
                         "BUG; can't happen");
            return source;
        }
    }

    BOOT_LOG_INF("Boot source: none");
    return BOOT_STATUS_SOURCE_NONE;
}

/**
 * Calculates the type of swap that just completed.
 *
 * This is used when a swap is interrupted by an external event. After
 * finishing the swap operation determines what the initial request was.
 */
static int
boot_previous_swap_type(void)
{
    int post_swap_type;

    post_swap_type = boot_swap_type();

    switch (post_swap_type) {
    case BOOT_SWAP_TYPE_NONE   : return BOOT_SWAP_TYPE_PERM;
    case BOOT_SWAP_TYPE_REVERT : return BOOT_SWAP_TYPE_TEST;
    case BOOT_SWAP_TYPE_PANIC  : return BOOT_SWAP_TYPE_PANIC;
    }

    return BOOT_SWAP_TYPE_FAIL;
}

/*
 * Compute the total size of the given image.  Includes the size of
 * the TLVs.
 */
#if !defined(MCUBOOT_OVERWRITE_ONLY) || defined(MCUBOOT_OVERWRITE_ONLY_FAST)
static int
boot_read_image_size(int slot, struct image_header *hdr, uint32_t *size)
{
    const struct flash_area *fap;
    struct image_tlv_info info;
    int area_id;
    int rc;

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = flash_area_read(fap, hdr->ih_hdr_size + hdr->ih_img_size,
                         &info, sizeof(info));
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }
    if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = BOOT_EBADIMAGE;
        goto done;
    }
    *size = hdr->ih_hdr_size + hdr->ih_img_size + info.it_tlv_tot;
    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

static int
boot_read_image_header(int slot, struct image_header *out_hdr)
{
    const struct flash_area *fap;
    int area_id;
    int rc;

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = flash_area_read(fap, 0, out_hdr, sizeof *out_hdr);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

static int
boot_read_image_headers(bool require_all)
{
    int rc;
    int i;

    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        rc = boot_read_image_header(i, boot_img_hdr(&boot_data, i));
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

static uint8_t
boot_write_sz(void)
{
    uint8_t elem_sz;
    uint8_t align;

    /* Figure out what size to write update status update as.  The size depends
     * on what the minimum write size is for scratch area, active image slot.
     * We need to use the bigger of those 2 values.
     */
    elem_sz = flash_area_align(boot_data.imgs[0].area);
    align = flash_area_align(boot_data.scratch_area);
    if (align > elem_sz) {
        elem_sz = align;
    }

    return elem_sz;
}

static int
boot_slots_compatible(void)
{
    size_t num_sectors_0 = boot_img_num_sectors(&boot_data, 0);
    size_t num_sectors_1 = boot_img_num_sectors(&boot_data, 1);
    size_t size_0, size_1;
    size_t i;

    if (num_sectors_0 > BOOT_MAX_IMG_SECTORS || num_sectors_1 > BOOT_MAX_IMG_SECTORS) {
        BOOT_LOG_WRN("Cannot upgrade: more sectors than allowed");
        return 0;
    }

    /* Ensure both image slots have identical sector layouts. */
    if (num_sectors_0 != num_sectors_1) {
        BOOT_LOG_WRN("Cannot upgrade: number of sectors differ between slots");
        return 0;
    }

    for (i = 0; i < num_sectors_0; i++) {
        size_0 = boot_img_sector_size(&boot_data, 0, i);
        size_1 = boot_img_sector_size(&boot_data, 1, i);
        if (size_0 != size_1) {
            BOOT_LOG_WRN("Cannot upgrade: an incompatible sector was found");
            return 0;
        }
    }

    return 1;
}

/**
 * Determines the sector layout of both image slots and the scratch area.
 * This information is necessary for calculating the number of bytes to erase
 * and copy during an image swap.  The information collected during this
 * function is used to populate the boot_data global.
 */
static int
boot_read_sectors(void)
{
    int rc;

    rc = boot_initialize_area(&boot_data, FLASH_AREA_IMAGE_0);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_initialize_area(&boot_data, FLASH_AREA_IMAGE_1);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    BOOT_WRITE_SZ(&boot_data) = boot_write_sz();

    return 0;
}

static uint32_t
boot_status_internal_off(int idx, int state, int elem_sz)
{
    int idx_sz;

    idx_sz = elem_sz * BOOT_STATUS_STATE_COUNT;

    return (idx - BOOT_STATUS_IDX_0) * idx_sz +
           (state - BOOT_STATUS_STATE_0) * elem_sz;
}

/**
 * Reads the status of a partially-completed swap, if any.  This is necessary
 * to recover in case the boot lodaer was reset in the middle of a swap
 * operation.
 */
static int
boot_read_status_bytes(const struct flash_area *fap, struct boot_status *bs)
{
    uint32_t off;
    uint8_t status;
    int max_entries;
    int found;
    int found_idx;
    int invalid;
    int rc;
    int i;

    off = boot_status_off(fap);
    max_entries = boot_status_entries(fap);

    found = 0;
    found_idx = 0;
    invalid = 0;
    for (i = 0; i < max_entries; i++) {
        rc = flash_area_read_is_empty(fap, off + i * BOOT_WRITE_SZ(&boot_data),
                &status, 1);
        if (rc < 0) {
            return BOOT_EFLASH;
        }

        if (rc == 1) {
            if (found && !found_idx) {
                found_idx = i;
            }
        } else if (!found) {
            found = 1;
        } else if (found_idx) {
            invalid = 1;
            break;
        }
    }

    if (invalid) {
        /* This means there was an error writing status on the last
         * swap. Tell user and move on to validation!
         */
        BOOT_LOG_ERR("Detected inconsistent status!");

#if !defined(MCUBOOT_VALIDATE_SLOT0)
        /* With validation of slot0 disabled, there is no way to be sure the
         * swapped slot0 is OK, so abort!
         */
        assert(0);
#endif
    }

    if (found) {
        if (!found_idx) {
            found_idx = i;
        }
        found_idx--;
        bs->idx = (found_idx / BOOT_STATUS_STATE_COUNT) + 1;
        bs->state = (found_idx % BOOT_STATUS_STATE_COUNT) + 1;
    }

    return 0;
}

/**
 * Reads the boot status from the flash.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present, or it indicates that previous copy finished,
 * there is no operation in progress.
 */
static int
boot_read_status(struct boot_status *bs)
{
    const struct flash_area *fap;
    int status_loc;
    int area_id;
    int rc;

    memset(bs, 0, sizeof *bs);
    bs->idx = BOOT_STATUS_IDX_0;
    bs->state = BOOT_STATUS_STATE_0;

#ifdef MCUBOOT_OVERWRITE_ONLY
    /* Overwrite-only doesn't make use of the swap status area. */
    return 0;
#endif

    status_loc = boot_status_source();
    switch (status_loc) {
    case BOOT_STATUS_SOURCE_NONE:
        return 0;

    case BOOT_STATUS_SOURCE_SCRATCH:
        area_id = FLASH_AREA_IMAGE_SCRATCH;
        break;

    case BOOT_STATUS_SOURCE_SLOT0:
        area_id = FLASH_AREA_IMAGE_0;
        break;

    default:
        assert(0);
        return BOOT_EBADARGS;
    }

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_read_status_bytes(fap, bs);

    flash_area_close(fap);

    return rc;
}

/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param bs                    The boot status to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(struct boot_status *bs)
{
    const struct flash_area *fap;
    uint32_t off;
    int area_id;
    int rc;
    uint8_t buf[BOOT_MAX_ALIGN];
    uint8_t align;
    uint8_t erased_val;

    /* NOTE: The first sector copied (that is the last sector on slot) contains
     *       the trailer. Since in the last step SLOT 0 is erased, the first
     *       two status writes go to the scratch which will be copied to SLOT 0!
     */

    if (bs->use_scratch) {
        /* Write to scratch. */
        area_id = FLASH_AREA_IMAGE_SCRATCH;
    } else {
        /* Write to slot 0. */
        area_id = FLASH_AREA_IMAGE_0;
    }

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    off = boot_status_off(fap) +
          boot_status_internal_off(bs->idx, bs->state,
                                   BOOT_WRITE_SZ(&boot_data));
    align = flash_area_align(fap);
    erased_val = flash_area_erased_val(fap);
    memset(buf, erased_val, BOOT_MAX_ALIGN);
    buf[0] = bs->state;

    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

/*
 * Validate image hash/signature in a slot.
 */
static int
boot_image_check(struct image_header *hdr, const struct flash_area *fap,
        struct boot_status *bs)
{
    static uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    int rc;

#ifndef MCUBOOT_ENC_IMAGES
    (void)bs;
    (void)rc;
#else
    if (fap->fa_id == FLASH_AREA_IMAGE_1 && IS_ENCRYPTED(hdr)) {
        rc = boot_enc_load(hdr, fap, bs->enckey[1]);
        if (rc < 0) {
            return BOOT_EBADIMAGE;
        }
        if (rc == 0 && boot_enc_set_key(1, bs->enckey[1])) {
            return BOOT_EBADIMAGE;
        }
    }
#endif

    if (bootutil_img_validate(hdr, fap, tmpbuf, BOOT_TMPBUF_SZ,
                              NULL, 0, NULL)) {
        return BOOT_EBADIMAGE;
    }
    return 0;
}

static int
split_image_check(struct image_header *app_hdr,
                  const struct flash_area *app_fap,
                  struct image_header *loader_hdr,
                  const struct flash_area *loader_fap)
{
    static void *tmpbuf;
    uint8_t loader_hash[32];

    if (!tmpbuf) {
        tmpbuf = malloc(BOOT_TMPBUF_SZ);
        if (!tmpbuf) {
            return BOOT_ENOMEM;
        }
    }

    if (bootutil_img_validate(loader_hdr, loader_fap, tmpbuf, BOOT_TMPBUF_SZ,
                              NULL, 0, loader_hash)) {
        return BOOT_EBADIMAGE;
    }

    if (bootutil_img_validate(app_hdr, app_fap, tmpbuf, BOOT_TMPBUF_SZ,
                              loader_hash, 32, NULL)) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

static inline int
boot_magic_is_erased(uint8_t erased_val, uint32_t magic)
{
    uint8_t i;
    for (i = 0; i < sizeof(magic); i++) {
        if (erased_val != *(((uint8_t *)&magic) + i)) {
            return 0;
        }
    }
    return 1;
}

static int
boot_validate_slot(int slot, struct boot_status *bs)
{
    const struct flash_area *fap;
    struct image_header *hdr;
    int rc;

    rc = flash_area_open(flash_area_id_from_image_slot(slot), &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    hdr = boot_img_hdr(&boot_data, slot);
    if (boot_magic_is_erased(flash_area_erased_val(fap), hdr->ih_magic) ||
            hdr->ih_flags & IMAGE_F_NON_BOOTABLE) {
        /* No bootable image in slot; continue booting from slot 0. */
        return -1;
    }

    if ((hdr->ih_magic != IMAGE_MAGIC || boot_image_check(hdr, fap, bs) != 0)) {
        if (slot != 0) {
            flash_area_erase(fap, 0, fap->fa_size);
            /* Image in slot 1 is invalid. Erase the image and
             * continue booting from slot 0.
             */
        }
        BOOT_LOG_ERR("Image in slot %d is not valid!", slot);
        return -1;
    }

    flash_area_close(fap);

    /* Image in slot 1 is valid. */
    return 0;
}

/**
 * Determines which swap operation to perform, if any.  If it is determined
 * that a swap operation is required, the image in the second slot is checked
 * for validity.  If the image in the second slot is invalid, it is erased, and
 * a swap type of "none" is indicated.
 *
 * @return                      The type of swap to perform (BOOT_SWAP_TYPE...)
 */
static int
boot_validated_swap_type(struct boot_status *bs)
{
    int swap_type;

    swap_type = boot_swap_type();
    switch (swap_type) {
    case BOOT_SWAP_TYPE_TEST:
    case BOOT_SWAP_TYPE_PERM:
    case BOOT_SWAP_TYPE_REVERT:
        /* Boot loader wants to switch to slot 1. Ensure image is valid. */
        if (boot_validate_slot(1, bs) != 0) {
            swap_type = BOOT_SWAP_TYPE_FAIL;
        }
    }

    return swap_type;
}

/**
 * Calculates the number of sectors the scratch area can contain.  A "last"
 * source sector is specified because images are copied backwards in flash
 * (final index to index number 0).
 *
 * @param last_sector_idx       The index of the last source sector
 *                                  (inclusive).
 * @param out_first_sector_idx  The index of the first source sector
 *                                  (inclusive) gets written here.
 *
 * @return                      The number of bytes comprised by the
 *                                  [first-sector, last-sector] range.
 */
#ifndef MCUBOOT_OVERWRITE_ONLY
static uint32_t
boot_copy_sz(int last_sector_idx, int *out_first_sector_idx)
{
    size_t scratch_sz;
    uint32_t new_sz;
    uint32_t sz;
    int i;

    sz = 0;

    scratch_sz = boot_scratch_area_size(&boot_data);
    for (i = last_sector_idx; i >= 0; i--) {
        new_sz = sz + boot_img_sector_size(&boot_data, 0, i);
        if (new_sz > scratch_sz) {
            break;
        }
        sz = new_sz;
    }

    /* i currently refers to a sector that doesn't fit or it is -1 because all
     * sectors have been processed.  In both cases, exclude sector i.
     */
    *out_first_sector_idx = i + 1;
    return sz;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Erases a region of flash.
 *
 * @param flash_area           The flash_area containing the region to erase.
 * @param off                   The offset within the flash area to start the
 *                                  erase.
 * @param sz                    The number of bytes to erase.
 *
 * @return                      0 on success; nonzero on failure.
 */
static inline int
boot_erase_sector(const struct flash_area *fap, uint32_t off, uint32_t sz)
{
    return flash_area_erase(fap, off, sz);
}

/**
 * Copies the contents of one flash region to another.  You must erase the
 * destination region prior to calling this function.
 *
 * @param flash_area_id_src     The ID of the source flash area.
 * @param flash_area_id_dst     The ID of the destination flash area.
 * @param off_src               The offset within the source flash area to
 *                                  copy from.
 * @param off_dst               The offset within the destination flash area to
 *                                  copy to.
 * @param sz                    The number of bytes to copy.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_sector(const struct flash_area *fap_src,
                 const struct flash_area *fap_dst,
                 uint32_t off_src, uint32_t off_dst, uint32_t sz)
{
    uint32_t bytes_copied;
    int chunk_sz;
    int rc;
#ifdef MCUBOOT_ENC_IMAGES
    uint32_t off;
    size_t blk_off;
    struct image_header *hdr;
    uint16_t idx;
    uint32_t blk_sz;
#endif

    static uint8_t buf[1024];

    bytes_copied = 0;
    while (bytes_copied < sz) {
        if (sz - bytes_copied > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = sz - bytes_copied;
        }

        rc = flash_area_read(fap_src, off_src + bytes_copied, buf, chunk_sz);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

#ifdef MCUBOOT_ENC_IMAGES
        if (fap_src->fa_id == FLASH_AREA_IMAGE_1 ||
                fap_dst->fa_id == FLASH_AREA_IMAGE_1) {
            /* assume slot1 as src, needs decryption */
            hdr = boot_img_hdr(&boot_data, 1);
            off = off_src;
            if (fap_dst->fa_id == FLASH_AREA_IMAGE_1) {
                /* might need encryption (metadata from slot0) */
                hdr = boot_img_hdr(&boot_data, 0);
                off = off_dst;
            }
            if (IS_ENCRYPTED(hdr)) {
                blk_sz = chunk_sz;
                idx = 0;
                if (off + bytes_copied < hdr->ih_hdr_size) {
                    /* do not decrypt header */
                    blk_off = 0;
                    blk_sz = chunk_sz - hdr->ih_hdr_size;
                    idx = hdr->ih_hdr_size;
                } else {
                    blk_off = ((off + bytes_copied) - hdr->ih_hdr_size) & 0xf;
                }
                if (off + bytes_copied + chunk_sz > hdr->ih_hdr_size + hdr->ih_img_size) {
                    /* do not decrypt TLVs */
                    if (off + bytes_copied >= hdr->ih_hdr_size + hdr->ih_img_size) {
                        blk_sz = 0;
                    } else {
                        blk_sz = (hdr->ih_hdr_size + hdr->ih_img_size) - (off + bytes_copied);
                    }
                }
                boot_encrypt(fap_src, (off + bytes_copied + idx) - hdr->ih_hdr_size,
                        blk_sz, blk_off, &buf[idx]);
            }
        }
#endif

        rc = flash_area_write(fap_dst, off_dst + bytes_copied, buf, chunk_sz);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        bytes_copied += chunk_sz;
    }

    return 0;
}

#ifndef MCUBOOT_OVERWRITE_ONLY
static inline int
boot_status_init(const struct flash_area *fap, const struct boot_status *bs)
{
    struct boot_swap_state swap_state;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_1, &swap_state);
    assert(rc == 0);

    if (swap_state.image_ok == BOOT_FLAG_SET) {
        rc = boot_write_image_ok(fap);
        assert(rc == 0);
    }

    rc = boot_write_swap_size(fap, bs->swap_size);
    assert(rc == 0);

#ifdef MCUBOOT_ENC_IMAGES
    rc = boot_write_enc_key(fap, 0, bs->enckey[0]);
    assert(rc == 0);

    rc = boot_write_enc_key(fap, 1, bs->enckey[1]);
    assert(rc == 0);
#endif

    rc = boot_write_magic(fap);
    assert(rc == 0);

    return 0;
}
#endif

#ifndef MCUBOOT_OVERWRITE_ONLY
static int
boot_erase_last_sector(const struct flash_area *fap)
{
    uint8_t slot;
    uint32_t last_sector;
    int rc;

    switch (fap->fa_id) {
    case FLASH_AREA_IMAGE_0:
        slot = 0;
        break;
    case FLASH_AREA_IMAGE_1:
        slot = 1;
        break;
    default:
        return BOOT_EFLASH;
    }

    last_sector = boot_img_num_sectors(&boot_data, slot) - 1;
    rc = boot_erase_sector(fap,
            boot_img_sector_off(&boot_data, slot, last_sector),
            boot_img_sector_size(&boot_data, slot, last_sector));
    assert(rc == 0);

    return rc;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Swaps the contents of two flash regions within the two image slots.
 *
 * @param idx                   The index of the first sector in the range of
 *                                  sectors being swapped.
 * @param sz                    The number of bytes to swap.
 * @param bs                    The current boot status.  This struct gets
 *                                  updated according to the outcome.
 *
 * @return                      0 on success; nonzero on failure.
 */
#ifndef MCUBOOT_OVERWRITE_ONLY
static void
boot_swap_sectors(int idx, uint32_t sz, struct boot_status *bs)
{
    const struct flash_area *fap_slot0;
    const struct flash_area *fap_slot1;
    const struct flash_area *fap_scratch;
    uint32_t copy_sz;
    uint32_t trailer_sz;
    uint32_t img_off;
    uint32_t scratch_trailer_off;
    struct boot_swap_state swap_state;
    size_t last_sector;
    int rc;

    /* Calculate offset from start of image area. */
    img_off = boot_img_sector_off(&boot_data, 0, idx);

    copy_sz = sz;
    trailer_sz = boot_slots_trailer_sz(BOOT_WRITE_SZ(&boot_data));

    /* sz in this function is always is always sized on a multiple of the
     * sector size. The check against the start offset of the last sector
     * is to determine if we're swapping the last sector. The last sector
     * needs special handling because it's where the trailer lives. If we're
     * copying it, we need to use scratch to write the trailer temporarily.
     *
     * NOTE: `use_scratch` is a temporary flag (never written to flash) which
     * controls if special handling is needed (swapping last sector).
     */
    last_sector = boot_img_num_sectors(&boot_data, 0) - 1;
    if (img_off + sz > boot_img_sector_off(&boot_data, 0, last_sector)) {
        copy_sz -= trailer_sz;
    }

    bs->use_scratch = (bs->idx == BOOT_STATUS_IDX_0 && copy_sz != sz);

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap_slot0);
    assert (rc == 0);

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fap_slot1);
    assert (rc == 0);

    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &fap_scratch);
    assert (rc == 0);

    if (bs->state == BOOT_STATUS_STATE_0) {
        rc = boot_erase_sector(fap_scratch, 0, sz);
        assert(rc == 0);

        rc = boot_copy_sector(fap_slot1, fap_scratch, img_off, 0, copy_sz);
        assert(rc == 0);

        if (bs->idx == BOOT_STATUS_IDX_0) {
            if (bs->use_scratch) {
                boot_status_init(fap_scratch, bs);
            } else {
                /* Prepare the status area... here it is known that the
                 * last sector is not being used by the image data so it's
                 * safe to erase.
                 */
                rc = boot_erase_last_sector(fap_slot0);
                assert(rc == 0);

                boot_status_init(fap_slot0, bs);
            }
        }

        bs->state = BOOT_STATUS_STATE_1;
        rc = boot_write_status(bs);
        BOOT_STATUS_ASSERT(rc == 0);
    }

    if (bs->state == BOOT_STATUS_STATE_1) {
        rc = boot_erase_sector(fap_slot1, img_off, sz);
        assert(rc == 0);

        rc = boot_copy_sector(fap_slot0, fap_slot1, img_off, img_off, copy_sz);
        assert(rc == 0);

        if (bs->idx == BOOT_STATUS_IDX_0 && !bs->use_scratch) {
            /* If not all sectors of the slot are being swapped,
             * guarantee here that only slot0 will have the state.
             */
            rc = boot_erase_last_sector(fap_slot1);
            assert(rc == 0);
        }

        bs->state = BOOT_STATUS_STATE_2;
        rc = boot_write_status(bs);
        BOOT_STATUS_ASSERT(rc == 0);
    }

    if (bs->state == BOOT_STATUS_STATE_2) {
        rc = boot_erase_sector(fap_slot0, img_off, sz);
        assert(rc == 0);

        /* NOTE: also copy trailer from scratch (has status info) */
        rc = boot_copy_sector(fap_scratch, fap_slot0, 0, img_off, copy_sz);
        assert(rc == 0);

        if (bs->use_scratch) {
            scratch_trailer_off = boot_status_off(fap_scratch);

            /* copy current status that is being maintained in scratch */
            rc = boot_copy_sector(fap_scratch, fap_slot0, scratch_trailer_off,
                        img_off + copy_sz,
                        BOOT_STATUS_STATE_COUNT * BOOT_WRITE_SZ(&boot_data));
            BOOT_STATUS_ASSERT(rc == 0);

            rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SCRATCH,
                                            &swap_state);
            assert(rc == 0);

            if (swap_state.image_ok == BOOT_FLAG_SET) {
                rc = boot_write_image_ok(fap_slot0);
                assert(rc == 0);
            }

            rc = boot_write_swap_size(fap_slot0, bs->swap_size);
            assert(rc == 0);

#ifdef MCUBOOT_ENC_IMAGES
            rc = boot_write_enc_key(fap_slot0, 0, bs->enckey[0]);
            assert(rc == 0);

            rc = boot_write_enc_key(fap_slot0, 1, bs->enckey[1]);
            assert(rc == 0);
#endif

            rc = boot_write_magic(fap_slot0);
            assert(rc == 0);
        }

        bs->idx++;
        bs->state = BOOT_STATUS_STATE_0;
        bs->use_scratch = 0;
        rc = boot_write_status(bs);
        BOOT_STATUS_ASSERT(rc == 0);
    }

    flash_area_close(fap_slot0);
    flash_area_close(fap_slot1);
    flash_area_close(fap_scratch);
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Overwrite slot 0 with the image contained in slot 1.  If a prior copy
 * operation was interrupted by a system reset, this function redos the
 * copy.
 *
 * @param bs                    The current boot status.  This function reads
 *                                  this struct to determine if it is resuming
 *                                  an interrupted swap operation.  This
 *                                  function writes the updated status to this
 *                                  function on return.
 *
 * @return                      0 on success; nonzero on failure.
 */
#ifdef MCUBOOT_OVERWRITE_ONLY
static int
boot_copy_image(struct boot_status *bs)
{
    size_t sect_count;
    size_t sect;
    int rc;
    size_t size;
    size_t this_size;
    size_t last_sector;
    const struct flash_area *fap_slot0;
    const struct flash_area *fap_slot1;

    (void)bs;

#if defined(MCUBOOT_OVERWRITE_ONLY_FAST)
    uint32_t src_size = 0;
    rc = boot_read_image_size(1, boot_img_hdr(&boot_data, 1), &src_size);
    assert(rc == 0);
#endif

    BOOT_LOG_INF("Image upgrade slot1 -> slot0");
    BOOT_LOG_INF("Erasing slot0");

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap_slot0);
    assert (rc == 0);

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fap_slot1);
    assert (rc == 0);

    sect_count = boot_img_num_sectors(&boot_data, 0);
    for (sect = 0, size = 0; sect < sect_count; sect++) {
        this_size = boot_img_sector_size(&boot_data, 0, sect);
        rc = boot_erase_sector(fap_slot0, size, this_size);
        assert(rc == 0);

        size += this_size;

#if defined(MCUBOOT_OVERWRITE_ONLY_FAST)
        if (size >= src_size) {
            break;
        }
#endif
    }

#ifdef MCUBOOT_ENC_IMAGES
    if (IS_ENCRYPTED(boot_img_hdr(&boot_data, 1))) {
        rc = boot_enc_load(boot_img_hdr(&boot_data, 1), fap_slot1, bs->enckey[1]);
        if (rc < 0) {
            return BOOT_EBADIMAGE;
        }
        if (rc == 0 && boot_enc_set_key(1, bs->enckey[1])) {
            return BOOT_EBADIMAGE;
        }
    }
#endif

    BOOT_LOG_INF("Copying slot 1 to slot 0: 0x%lx bytes", size);
    rc = boot_copy_sector(fap_slot1, fap_slot0, 0, 0, size);

    /*
     * Erases header and trailer. The trailer is erased because when a new
     * image is written without a trailer as is the case when using newt, the
     * trailer that was left might trigger a new upgrade.
     */
    rc = boot_erase_sector(fap_slot1,
                           boot_img_sector_off(&boot_data, 1, 0),
                           boot_img_sector_size(&boot_data, 1, 0));
    assert(rc == 0);
    last_sector = boot_img_num_sectors(&boot_data, 1) - 1;
    rc = boot_erase_sector(fap_slot1,
                           boot_img_sector_off(&boot_data, 1, last_sector),
                           boot_img_sector_size(&boot_data, 1, last_sector));
    assert(rc == 0);

    flash_area_close(fap_slot0);
    flash_area_close(fap_slot1);

    /* TODO: Perhaps verify slot 0's signature again? */

    return 0;
}

#else

/**
 * Swaps the two images in flash.  If a prior copy operation was interrupted
 * by a system reset, this function completes that operation.
 *
 * @param bs                    The current boot status.  This function reads
 *                                  this struct to determine if it is resuming
 *                                  an interrupted swap operation.  This
 *                                  function writes the updated status to this
 *                                  function on return.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_image(struct boot_status *bs)
{
    uint32_t sz;
    int first_sector_idx;
    int last_sector_idx;
    uint32_t swap_idx;
    struct image_header *hdr;
#ifdef MCUBOOT_ENC_IMAGES
    const struct flash_area *fap;
    uint8_t slot;
    uint8_t i;
#endif
    uint32_t size;
    uint32_t copy_size;
    int rc;

    /* FIXME: just do this if asked by user? */

    size = copy_size = 0;

    if (bs->idx == BOOT_STATUS_IDX_0 && bs->state == BOOT_STATUS_STATE_0) {
        /*
         * No swap ever happened, so need to find the largest image which
         * will be used to determine the amount of sectors to swap.
         */
        hdr = boot_img_hdr(&boot_data, 0);
        if (hdr->ih_magic == IMAGE_MAGIC) {
            rc = boot_read_image_size(0, hdr, &copy_size);
            assert(rc == 0);
        }

#ifdef MCUBOOT_ENC_IMAGES
        if (IS_ENCRYPTED(hdr)) {
            fap = BOOT_IMG_AREA(&boot_data, 0);
            rc = boot_enc_load(hdr, fap, bs->enckey[0]);
            assert(rc >= 0);

            if (rc == 0) {
                rc = boot_enc_set_key(0, bs->enckey[0]);
                assert(rc == 0);
            } else {
                rc = 0;
            }
        } else {
            memset(bs->enckey[0], 0xff, BOOT_ENC_KEY_SIZE);
        }
#endif

        hdr = boot_img_hdr(&boot_data, 1);
        if (hdr->ih_magic == IMAGE_MAGIC) {
            rc = boot_read_image_size(1, hdr, &size);
            assert(rc == 0);
        }

#ifdef MCUBOOT_ENC_IMAGES
        hdr = boot_img_hdr(&boot_data, 1);
        if (IS_ENCRYPTED(hdr)) {
            fap = BOOT_IMG_AREA(&boot_data, 1);
            rc = boot_enc_load(hdr, fap, bs->enckey[1]);
            assert(rc >= 0);

            if (rc == 0) {
                rc = boot_enc_set_key(1, bs->enckey[1]);
                assert(rc == 0);
            } else {
                rc = 0;
            }
        } else {
            memset(bs->enckey[1], 0xff, BOOT_ENC_KEY_SIZE);
        }
#endif

        if (size > copy_size) {
            copy_size = size;
        }

        bs->swap_size = copy_size;

    } else {
        /*
         * If a swap was under way, the swap_size should already be present
         * in the trailer...
         */
        rc = boot_read_swap_size(&bs->swap_size);
        assert(rc == 0);

        copy_size = bs->swap_size;

#ifdef MCUBOOT_ENC_IMAGES
        for (slot = 0; slot <= 1; slot++) {
            rc = boot_read_enc_key(slot, bs->enckey[slot]);
            assert(rc == 0);

            for (i = 0; i < BOOT_ENC_KEY_SIZE; i++) {
                if (bs->enckey[slot][i] != 0xff) {
                    break;
                }
            }

            if (i != BOOT_ENC_KEY_SIZE) {
                boot_enc_set_key(slot, bs->enckey[slot]);
            }
        }
#endif
    }

    size = 0;
    last_sector_idx = 0;
    while (1) {
        size += boot_img_sector_size(&boot_data, 0, last_sector_idx);
        if (size >= copy_size) {
            break;
        }
        last_sector_idx++;
    }

    swap_idx = 0;
    while (last_sector_idx >= 0) {
        sz = boot_copy_sz(last_sector_idx, &first_sector_idx);
        if (swap_idx >= (bs->idx - BOOT_STATUS_IDX_0)) {
            boot_swap_sectors(first_sector_idx, sz, bs);
        }

        last_sector_idx = first_sector_idx - 1;
        swap_idx++;
    }

#ifdef MCUBOOT_VALIDATE_SLOT0
    if (boot_status_fails > 0) {
        BOOT_LOG_WRN("%d status write fails performing the swap", boot_status_fails);
    }
#endif

    return 0;
}
#endif

/**
 * Marks the image in slot 0 as fully copied.
 */
#ifndef MCUBOOT_OVERWRITE_ONLY
static int
boot_set_copy_done(void)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_write_copy_done(fap);
    flash_area_close(fap);
    return rc;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Marks a reverted image in slot 0 as confirmed.  This is necessary to ensure
 * the status bytes from the image revert operation don't get processed on a
 * subsequent boot.
 *
 * NOTE: image_ok is tested before writing because if there's a valid permanent
 * image installed on slot0 and the new image to be upgrade to has a bad sig,
 * image_ok would be overwritten.
 */
#ifndef MCUBOOT_OVERWRITE_ONLY
static int
boot_set_image_ok(void)
{
    const struct flash_area *fap;
    struct boot_swap_state state;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_read_swap_state(fap, &state);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto out;
    }

    if (state.image_ok == BOOT_FLAG_UNSET) {
        rc = boot_write_image_ok(fap);
    }

out:
    flash_area_close(fap);
    return rc;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Performs an image swap if one is required.
 *
 * @param out_swap_type         On success, the type of swap performed gets
 *                                  written here.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_swap_if_needed(int *out_swap_type)
{
    struct boot_status bs;
    int swap_type;
    int rc;

    /* Determine if we rebooted in the middle of an image swap
     * operation.
     */
    rc = boot_read_status(&bs);
    assert(rc == 0);
    if (rc != 0) {
        return rc;
    }

    /* If a partial swap was detected, complete it. */
    if (bs.idx != BOOT_STATUS_IDX_0 || bs.state != BOOT_STATUS_STATE_0) {
        rc = boot_copy_image(&bs);
        assert(rc == 0);

        /* NOTE: here we have finished a swap resume. The initial request
         * was either a TEST or PERM swap, which now after the completed
         * swap will be determined to be respectively REVERT (was TEST)
         * or NONE (was PERM).
         */

        /* Extrapolate the type of the partial swap.  We need this
         * information to know how to mark the swap complete in flash.
         */
        swap_type = boot_previous_swap_type();
    } else {
        swap_type = boot_validated_swap_type(&bs);
        switch (swap_type) {
        case BOOT_SWAP_TYPE_TEST:
        case BOOT_SWAP_TYPE_PERM:
        case BOOT_SWAP_TYPE_REVERT:
            rc = boot_copy_image(&bs);
            assert(rc == 0);
            break;
        }
    }

    *out_swap_type = swap_type;
    return 0;
}

/**
 * Prepares the booting process.  This function moves images around in flash as
 * appropriate, and tells you what address to boot from.
 *
 * @param rsp                   On success, indicates how booting should occur.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_go(struct boot_rsp *rsp)
{
    int swap_type;
    size_t slot;
    int rc;
    int fa_id;
    bool reload_headers = false;

    /* The array of slot sectors are defined here (as opposed to file scope) so
     * that they don't get allocated for non-boot-loader apps.  This is
     * necessary because the gcc option "-fdata-sections" doesn't seem to have
     * any effect in older gcc versions (e.g., 4.8.4).
     */
    static boot_sector_t slot0_sectors[BOOT_MAX_IMG_SECTORS];
    static boot_sector_t slot1_sectors[BOOT_MAX_IMG_SECTORS];
    boot_data.imgs[0].sectors = slot0_sectors;
    boot_data.imgs[1].sectors = slot1_sectors;

#ifdef MCUBOOT_ENC_IMAGES
    /* FIXME: remove this after RAM is cleared by sim */
    boot_enc_zeroize();
#endif

    /* Open boot_data image areas for the duration of this call. */
    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        fa_id = flash_area_id_from_image_slot(slot);
        rc = flash_area_open(fa_id, &BOOT_IMG_AREA(&boot_data, slot));
        assert(rc == 0);
    }
    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH,
                         &BOOT_SCRATCH_AREA(&boot_data));
    assert(rc == 0);

    /* Determine the sector layout of the image slots and scratch area. */
    rc = boot_read_sectors();
    if (rc != 0) {
        BOOT_LOG_WRN("Failed reading sectors; BOOT_MAX_IMG_SECTORS=%d - too small?",
                BOOT_MAX_IMG_SECTORS);
        goto out;
    }

    /* Attempt to read an image header from each slot. */
    rc = boot_read_image_headers(false);
    if (rc != 0) {
        goto out;
    }

    /* If the image slots aren't compatible, no swap is possible.  Just boot
     * into slot 0.
     */
    if (boot_slots_compatible()) {
        rc = boot_swap_if_needed(&swap_type);
        assert(rc == 0);
        if (rc != 0) {
            goto out;
        }

        /*
         * The following states need image_ok be explicitly set after the
         * swap was finished to avoid a new revert.
         */
        if (swap_type == BOOT_SWAP_TYPE_REVERT || swap_type == BOOT_SWAP_TYPE_FAIL) {
#ifndef MCUBOOT_OVERWRITE_ONLY
            rc = boot_set_image_ok();
            if (rc != 0) {
                swap_type = BOOT_SWAP_TYPE_PANIC;
            }
#endif /* !MCUBOOT_OVERWRITE_ONLY */
        }
    } else {
        swap_type = BOOT_SWAP_TYPE_NONE;
    }

    switch (swap_type) {
    case BOOT_SWAP_TYPE_NONE:
        slot = 0;
        break;

    case BOOT_SWAP_TYPE_TEST:          /* fallthrough */
    case BOOT_SWAP_TYPE_PERM:          /* fallthrough */
    case BOOT_SWAP_TYPE_REVERT:
        slot = 1;
        reload_headers = true;
#ifndef MCUBOOT_OVERWRITE_ONLY
        rc = boot_set_copy_done();
        if (rc != 0) {
            swap_type = BOOT_SWAP_TYPE_PANIC;
        }
#endif /* !MCUBOOT_OVERWRITE_ONLY */
        break;

    case BOOT_SWAP_TYPE_FAIL:
        /* The image in slot 1 was invalid and is now erased.  Ensure we don't
         * try to boot into it again on the next reboot.  Do this by pretending
         * we just reverted back to slot 0.
         */
        slot = 0;
        reload_headers = true;
        break;

    default:
        swap_type = BOOT_SWAP_TYPE_PANIC;
    }

    if (swap_type == BOOT_SWAP_TYPE_PANIC) {
        BOOT_LOG_ERR("panic!");
        assert(0);

        /* Loop forever... */
        while (1) {}
    }

    if (reload_headers) {
        rc = boot_read_image_headers(false);
        if (rc != 0) {
            goto out;
        }
        /* Since headers were reloaded, it can be assumed we just performed a
         * swap or overwrite. Now the header info that should be used to
         * provide the data for the bootstrap, which previously was at Slot 1,
         * was updated to Slot 0.
         */
        slot = 0;
    }

#ifdef MCUBOOT_VALIDATE_SLOT0
    rc = boot_validate_slot(0, NULL);
    ASSERT(rc == 0);
    if (rc != 0) {
        rc = BOOT_EBADIMAGE;
        goto out;
    }
#else
    /* Even if we're not re-validating slot 0, we could be booting
     * onto an empty flash chip. At least do a basic sanity check that
     * the magic number on the image is OK.
     */
    if (boot_data.imgs[0].hdr.ih_magic != IMAGE_MAGIC) {
        BOOT_LOG_ERR("bad image magic 0x%lx", (unsigned long)boot_data.imgs[0].hdr.ih_magic);
        rc = BOOT_EBADIMAGE;
        goto out;
    }
#endif

    /* Always boot from the primary slot. */
    rsp->br_flash_dev_id = boot_data.imgs[0].area->fa_device_id;
    rsp->br_image_off = boot_img_slot_off(&boot_data, 0);
    rsp->br_hdr = boot_img_hdr(&boot_data, slot);

 out:
    flash_area_close(BOOT_SCRATCH_AREA(&boot_data));
    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        flash_area_close(BOOT_IMG_AREA(&boot_data, BOOT_NUM_SLOTS - 1 - slot));
    }
    return rc;
}

int
split_go(int loader_slot, int split_slot, void **entry)
{
    boot_sector_t *sectors;
    uintptr_t entry_val;
    int loader_flash_id;
    int split_flash_id;
    int rc;

    sectors = malloc(BOOT_MAX_IMG_SECTORS * 2 * sizeof *sectors);
    if (sectors == NULL) {
        return SPLIT_GO_ERR;
    }
    boot_data.imgs[loader_slot].sectors = sectors + 0;
    boot_data.imgs[split_slot].sectors = sectors + BOOT_MAX_IMG_SECTORS;

    loader_flash_id = flash_area_id_from_image_slot(loader_slot);
    rc = flash_area_open(loader_flash_id,
                         &BOOT_IMG_AREA(&boot_data, split_slot));
    assert(rc == 0);
    split_flash_id = flash_area_id_from_image_slot(split_slot);
    rc = flash_area_open(split_flash_id,
                         &BOOT_IMG_AREA(&boot_data, split_slot));
    assert(rc == 0);

    /* Determine the sector layout of the image slots and scratch area. */
    rc = boot_read_sectors();
    if (rc != 0) {
        rc = SPLIT_GO_ERR;
        goto done;
    }

    rc = boot_read_image_headers(true);
    if (rc != 0) {
        goto done;
    }

    /* Don't check the bootable image flag because we could really call a
     * bootable or non-bootable image.  Just validate that the image check
     * passes which is distinct from the normal check.
     */
    rc = split_image_check(boot_img_hdr(&boot_data, split_slot),
                           BOOT_IMG_AREA(&boot_data, split_slot),
                           boot_img_hdr(&boot_data, loader_slot),
                           BOOT_IMG_AREA(&boot_data, loader_slot));
    if (rc != 0) {
        rc = SPLIT_GO_NON_MATCHING;
        goto done;
    }

    entry_val = boot_img_slot_off(&boot_data, split_slot) +
                boot_img_hdr(&boot_data, split_slot)->ih_hdr_size;
    *entry = (void *) entry_val;
    rc = SPLIT_GO_OK;

done:
    flash_area_close(BOOT_IMG_AREA(&boot_data, split_slot));
    flash_area_close(BOOT_IMG_AREA(&boot_data, loader_slot));
    free(sectors);
    return rc;
}
