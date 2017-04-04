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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "sysflash/sysflash.h"
#include "flash_map/flash_map.h"
#include <hal/hal_flash.h>
#include <os/os_malloc.h>
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#include "bootutil/bootutil_log.h"

#define BOOT_MAX_IMG_SECTORS        120

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS              2

static struct {
    struct {
        struct image_header hdr;
        struct flash_area *sectors;
        int num_sectors;
    } imgs[BOOT_NUM_SLOTS];

    struct flash_area scratch_sector;

    uint8_t write_sz;
} boot_data;

struct boot_status_table {
    /**
     * For each field, a value of 0 means "any".
     */
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
         * copy-done | 0x01       | N/A        |
         * ----------+------------+------------'
         * source: none                        |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0x01,
        .bst_status_source =    BOOT_STATUS_SOURCE_NONE,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Good       | Any        |
         * copy-done | 0xff       | N/A        |
         * ----------+------------+------------'
         * source: slot 0                      |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0xff,
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
        .bst_magic_slot0 =      0,
        .bst_magic_scratch =    BOOT_MAGIC_GOOD,
        .bst_copy_done_slot0 =  0,
        .bst_status_source =    BOOT_STATUS_SOURCE_SCRATCH,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | Unset      | Any        |
         * copy-done | 0xff       | N/A        |
         * ----------+------------+------------|
         * source: varies                      |
         * ------------------------------------+------------------------------+
         * This represents one of two cases:                                  |
         * o No swaps ever (no status to read, so no harm in checking).       |
         * o Mid-revert; status in slot 0.                                    |
         * -------------------------------------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_MAGIC_UNSET,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0xff,
        .bst_status_source =    BOOT_STATUS_SOURCE_SLOT0,
    },
};

#define BOOT_STATUS_TABLES_COUNT \
    (sizeof boot_status_tables / sizeof boot_status_tables[0])

/**
 * This table indicates the next swap type that should be performed.  The first
 * column contains the current swap type.  The second column contains the swap
 * type that should be effected after the first completes.
 */
static const uint8_t boot_swap_trans_table[][2] = {
    /*     From                     To             */
    { BOOT_SWAP_TYPE_REVERT,    BOOT_SWAP_TYPE_NONE },
    { BOOT_SWAP_TYPE_PERM,      BOOT_SWAP_TYPE_NONE },
    { BOOT_SWAP_TYPE_TEST,      BOOT_SWAP_TYPE_REVERT },
};

#define BOOT_SWAP_TRANS_TABLE_SIZE   \
    (sizeof boot_swap_trans_table / sizeof boot_swap_trans_table[0])

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
    struct boot_swap_state state_slot1;
    int rc;
    int i;
    uint8_t source;

    rc = boot_read_swap_state_img(0, &state_slot0);
    assert(rc == 0);

    rc = boot_read_swap_state_img(1, &state_slot1);
    assert(rc == 0);

    rc = boot_read_swap_state_scratch(&state_scratch);
    assert(rc == 0);

    BOOT_LOG_SWAP_STATE("Image 0", &state_slot0);
    BOOT_LOG_SWAP_STATE("Image 1", &state_slot1);
    BOOT_LOG_SWAP_STATE("Scratch", &state_scratch);

    for (i = 0; i < BOOT_STATUS_TABLES_COUNT; i++) {
        table = boot_status_tables + i;

        if ((table->bst_magic_slot0     == 0    ||
             table->bst_magic_slot0     == state_slot0.magic)   &&
            (table->bst_magic_scratch   == 0    ||
             table->bst_magic_scratch   == state_scratch.magic) &&
            (table->bst_copy_done_slot0 == 0    ||
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
 */
static int
boot_previous_swap_type(void)
{
    int post_swap_type;
    int i;

    post_swap_type = boot_swap_type();

    for (i = 0; i < BOOT_SWAP_TRANS_TABLE_SIZE; i++){
        if (boot_swap_trans_table[i][1] == post_swap_type) {
            return boot_swap_trans_table[i][0];
        }
    }

    /* XXX: Temporary assert. */
    assert(0);

    return BOOT_SWAP_TYPE_REVERT;
}

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
boot_read_image_headers(void)
{
    int rc;
    int i;

    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        rc = boot_read_image_header(i, &boot_data.imgs[i].hdr);
        if (rc != 0) {
            /* If at least one header was read successfully, then the boot
             * loader can attempt a boot.  Failure to read any headers is a
             * fatal error.
             */
            if (i > 0) {
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
    elem_sz = hal_flash_align(boot_data.imgs[0].sectors[0].fa_device_id);
    align = hal_flash_align(boot_data.scratch_sector.fa_device_id);
    if (align > elem_sz) {
        elem_sz = align;
    }

    return elem_sz;
}

static int
boot_slots_compatible(void)
{
    const struct flash_area *sector0;
    const struct flash_area *sector1;
    int i;

    /* Ensure both image slots have identical sector layouts. */
    if (boot_data.imgs[0].num_sectors != boot_data.imgs[1].num_sectors) {
        return 0;
    }
    for (i = 0; i < boot_data.imgs[0].num_sectors; i++) {
        sector0 = boot_data.imgs[0].sectors + i;
        sector1 = boot_data.imgs[1].sectors + i;
        if (sector0->fa_size != sector1->fa_size) {
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
    const struct flash_area *scratch;
    int num_sectors_slot0;
    int num_sectors_slot1;
    int rc;

    num_sectors_slot0 = BOOT_MAX_IMG_SECTORS;
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &num_sectors_slot0,
                               boot_data.imgs[0].sectors);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    boot_data.imgs[0].num_sectors = num_sectors_slot0;

    num_sectors_slot1 = BOOT_MAX_IMG_SECTORS;
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_1, &num_sectors_slot1,
                               boot_data.imgs[1].sectors);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    boot_data.imgs[1].num_sectors = num_sectors_slot1;

    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &scratch);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    boot_data.scratch_sector = *scratch;

    boot_data.write_sz = boot_write_sz();

    return 0;
}

static uint32_t
boot_status_internal_off(int idx, int state, int elem_sz)
{
    int idx_sz;

    idx_sz = elem_sz * BOOT_STATUS_STATE_COUNT;

    return idx * idx_sz + state * elem_sz;
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
    int found;
    int end_index;
    int assert_ff;
    int rc;
    int i;

    off = boot_status_off(fap);
    found = 0;
    end_index = BOOT_STATUS_MAX_ENTRIES;
    assert_ff = 0;

    for (i = 0; i < BOOT_STATUS_MAX_ENTRIES; i++) {
        rc = flash_area_read(fap, off + i * boot_data.write_sz, &status, 1);
        assert(rc == 0);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        if (!found && status != 0xff) {
            found = 1;
        } else if (found && !assert_ff && status == 0xff) {
            end_index = i;
            assert_ff = 1;
        } else if (assert_ff) {
            assert(status == 0xff);
        }
    }

    if (found) {
        end_index--;
        bs->idx = end_index / BOOT_STATUS_STATE_COUNT;
        bs->state = end_index % BOOT_STATUS_STATE_COUNT;
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
    assert(rc == 0);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return 0;
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
    uint8_t buf[8];
    uint8_t align;

    if (bs->idx == 0) {
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
          boot_status_internal_off(bs->idx, bs->state, boot_data.write_sz);

    align = hal_flash_align(fap->fa_device_id);
    memset(buf, 0xFF, 8);
    buf[0] = bs->state;

    rc = flash_area_write(fap, off, buf, align);
    assert(rc == 0);
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
boot_image_check(struct image_header *hdr, const struct flash_area *fap)
{
    static uint8_t tmpbuf[BOOT_TMPBUF_SZ];

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

static int
boot_validate_slot(int slot)
{
    const struct flash_area *fap;
    int rc;

    if (boot_data.imgs[slot].hdr.ih_magic == 0xffffffff ||
        boot_data.imgs[slot].hdr.ih_flags & IMAGE_F_NON_BOOTABLE) {

        /* No bootable image in slot 1; continue booting from slot 0. */
        return -1;
    }

    rc = flash_area_open(flash_area_id_from_image_slot(slot), &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if ((boot_data.imgs[slot].hdr.ih_magic != IMAGE_MAGIC ||
         boot_image_check(&boot_data.imgs[slot].hdr, fap) != 0)) {

        if (slot != 0) {
            assert(flash_area_erase(fap, 0, fap->fa_size) == 0);
            /* Image in slot 1 is invalid. Erase the image and
             * continue booting from slot 0.
             */
        }
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
boot_validated_swap_type(void)
{
    int swap_type;
    int rc;

    swap_type = boot_swap_type();
    if (swap_type == BOOT_SWAP_TYPE_NONE) {
        /* Continue using slot 0. */
        return BOOT_SWAP_TYPE_NONE;
    }

    /* Boot loader wants to switch to slot 1.  Ensure image is valid. */
    rc = boot_validate_slot(1);
    if (rc != 0) {
        return BOOT_SWAP_TYPE_FAIL;
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
static uint32_t
boot_copy_sz(int last_sector_idx, int *out_first_sector_idx)
{
    uint32_t new_sz;
    uint32_t sz;
    int i;

    sz = 0;

    for (i = last_sector_idx; i >= 0; i--) {
        new_sz = sz + boot_data.imgs[0].sectors[i].fa_size;
        if (new_sz > boot_data.scratch_sector.fa_size) {
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

/**
 * Erases a region of flash.
 *
 * @param flash_area_idx        The ID of the flash area containing the region
 *                                  to erase.
 * @param off                   The offset within the flash area to start the
 *                                  erase.
 * @param sz                    The number of bytes to erase.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_erase_sector(int flash_area_id, uint32_t off, uint32_t sz)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(flash_area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = flash_area_erase(fap, off, sz);
    assert(rc == 0);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
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
boot_copy_sector(int flash_area_id_src, int flash_area_id_dst,
                 uint32_t off_src, uint32_t off_dst, uint32_t sz)
{
    const struct flash_area *fap_src;
    const struct flash_area *fap_dst;
    uint32_t magic_off;
    uint32_t seek_off;
    int chunk_sz;
    int trailer_sz;
    int has_trailer;
    int rc;

    static uint8_t buf[1024];

    fap_src = NULL;
    fap_dst = NULL;

    rc = flash_area_open(flash_area_id_src, &fap_src);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = flash_area_open(flash_area_id_dst, &fap_dst);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    /* To avoid inconsistent state if there's reset while in the
     * copy loop, it is done in 3 steps:
     *
     * 1) First copy data without trailer (boot status + magic).
     * 2) Next copy the trailer.
     * 3) Finally copy the boot magic.
     */

    trailer_sz = boot_trailer_sz(boot_data.write_sz);
    seek_off = 0;
    has_trailer = 0;

    if (trailer_sz < sz) {
        sz -= trailer_sz;
        has_trailer = 1;
    }

    while (seek_off < sz) {
        chunk_sz = MIN(sz - seek_off, sizeof buf);

        rc = flash_area_read(fap_src, off_src + seek_off, buf, chunk_sz);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }

        rc = flash_area_write(fap_dst, off_dst + seek_off, buf, chunk_sz);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }

        seek_off += chunk_sz;
    }

    if (has_trailer) {
        sz += trailer_sz;
        magic_off = seek_off;
        seek_off += sizeof boot_img_magic;

        while (seek_off < sz) {
            chunk_sz = MIN(sz - seek_off, sizeof buf);

            rc = flash_area_read(fap_src, off_src + seek_off, buf, chunk_sz);
            if (rc != 0) {
                rc = BOOT_EFLASH;
                goto done;
            }

            rc = flash_area_write(fap_dst, off_dst + seek_off, buf, chunk_sz);
            if (rc != 0) {
                rc = BOOT_EFLASH;
                goto done;
            }

            seek_off += chunk_sz;
        }

        chunk_sz = sizeof boot_img_magic;
        rc = flash_area_read(fap_src, off_src + magic_off, buf, chunk_sz);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }

        rc = flash_area_write(fap_dst, off_dst + magic_off, buf, chunk_sz);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto done;
        }
    }

    rc = 0;

done:
    flash_area_close(fap_src);
    flash_area_close(fap_dst);
    return rc;
}

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
static void
boot_swap_sectors(int idx, uint32_t sz, struct boot_status *bs)
{
    uint32_t copy_sz;
    uint32_t img_off;
    int rc;

    /* Calculate offset from start of image area. */
    img_off = boot_data.imgs[0].sectors[idx].fa_off -
              boot_data.imgs[0].sectors[0].fa_off;

    if (bs->state == 0) {
        rc = boot_erase_sector(FLASH_AREA_IMAGE_SCRATCH, 0, sz);
        assert(rc == 0);

        rc = boot_copy_sector(FLASH_AREA_IMAGE_1, FLASH_AREA_IMAGE_SCRATCH,
                              img_off, 0, sz);
        assert(rc == 0);

        bs->state = 1;
        rc = boot_write_status(bs);
        assert(rc == 0);
    }
    if (bs->state == 1) {
        rc = boot_erase_sector(FLASH_AREA_IMAGE_1, img_off, sz);
        assert(rc == 0);

        copy_sz = sz;
        if (boot_data.imgs[0].sectors[idx].fa_off + sz >=
            boot_data.imgs[1].sectors[0].fa_off) {

            /* This is the end of the area.  Don't copy the image state into
             * slot 1.
             */
            copy_sz -= boot_trailer_sz(boot_data.write_sz);
        }

        rc = boot_copy_sector(FLASH_AREA_IMAGE_0, FLASH_AREA_IMAGE_1,
                              img_off, img_off, copy_sz);
        assert(rc == 0);

        bs->state = 2;
        rc = boot_write_status(bs);
        assert(rc == 0);
    }
    if (bs->state == 2) {
        rc = boot_erase_sector(FLASH_AREA_IMAGE_0, img_off, sz);
        assert(rc == 0);

        rc = boot_copy_sector(FLASH_AREA_IMAGE_SCRATCH, FLASH_AREA_IMAGE_0,
                              0, img_off, sz);
        assert(rc == 0);

        bs->idx++;
        bs->state = 0;
        rc = boot_write_status(bs);
        assert(rc == 0);
    }
}

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
    int swap_idx;

    swap_idx = 0;
    last_sector_idx = boot_data.imgs[0].num_sectors - 1;
    while (last_sector_idx >= 0) {
        sz = boot_copy_sz(last_sector_idx, &first_sector_idx);
        if (swap_idx >= bs->idx) {
            boot_swap_sectors(first_sector_idx, sz, bs);
        }

        last_sector_idx = first_sector_idx - 1;
        swap_idx++;
    }

    return 0;
}

/**
 * Marks a test image in slot 0 as fully copied.
 */
static int
boot_finalize_test_swap(void)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_write_copy_done(fap);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Marks a reverted image in slot 0 as confirmed.  This is necessary to ensure
 * the status bytes from the image revert operation don't get processed on a
 * subsequent boot.
 */
static int
boot_finalize_revert_swap(void)
{
    const struct flash_area *fap;
    struct boot_swap_state state_slot0;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_read_swap_state(fap, &state_slot0);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if (state_slot0.magic == BOOT_MAGIC_UNSET) {
        rc = boot_write_magic(fap);
        if (rc != 0) {
            return rc;
        }
    }

    if (state_slot0.copy_done == 0xff) {
        rc = boot_write_copy_done(fap);
        if (rc != 0) {
            return rc;
        }
    }

    if (state_slot0.image_ok == 0xff) {
        rc = boot_write_image_ok(fap);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

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
    if (rc != 0) {
        return rc;
    }

    /* If a partial swap was detected, complete it. */
    if (bs.idx != 0 || bs.state != 0) {
        rc = boot_copy_image(&bs);
        assert(rc == 0);

        /* Extrapolate the type of the partial swap.  We need this
         * information to know how to mark the swap complete in flash.
         */
        swap_type = boot_previous_swap_type();
    } else {
        swap_type = boot_validated_swap_type();
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
    int slot;
    int rc;

    /* The array of slot sectors are defined here (as opposed to file scope) so
     * that they don't get allocated for non-boot-loader apps.  This is
     * necessary because the gcc option "-fdata-sections" doesn't seem to have
     * any effect in older gcc versions (e.g., 4.8.4).
     */
    static struct flash_area slot0_sectors[BOOT_MAX_IMG_SECTORS];
    static struct flash_area slot1_sectors[BOOT_MAX_IMG_SECTORS];
    boot_data.imgs[0].sectors = slot0_sectors;
    boot_data.imgs[1].sectors = slot1_sectors;

    /* Determine the sector layout of the image slots and scratch area. */
    rc = boot_read_sectors();
    if (rc != 0) {
        return rc;
    }

    /* Attempt to read an image header from each slot. */
    rc = boot_read_image_headers();
    if (rc != 0) {
        return rc;
    }

    /* If the image slots aren't compatible, no swap is possible.  Just boot
     * into slot 0.
     */
    if (boot_slots_compatible()) {
        rc = boot_swap_if_needed(&swap_type);
        if (rc != 0) {
            return rc;
        }
    } else {
        swap_type = BOOT_SWAP_TYPE_NONE;
    }

    switch (swap_type) {
    case BOOT_SWAP_TYPE_NONE:
#ifdef BOOTUTIL_VALIDATE_SLOT0
        rc = boot_validate_slot(0);
        if (rc != 0) {
            return BOOT_EBADIMAGE;
        }
#endif
        slot = 0;
        break;

    case BOOT_SWAP_TYPE_TEST:
    case BOOT_SWAP_TYPE_PERM:
        slot = 1;
        boot_finalize_test_swap();
        break;

    case BOOT_SWAP_TYPE_REVERT:
        slot = 1;
        boot_finalize_revert_swap();
        break;

    case BOOT_SWAP_TYPE_FAIL:
        /* The image in slot 1 was invalid and is now erased.  Ensure we don't
         * try to boot into it again on the next reboot.  Do this by pretending
         * we just reverted back to slot 0.
         */
        slot = 0;
        boot_finalize_revert_swap();
        break;

    default:
        assert(0);
        slot = 0;
        break;
    }

    /* Always boot from the primary slot. */
    rsp->br_flash_id = boot_data.imgs[0].sectors[0].fa_device_id;
    rsp->br_image_addr = boot_data.imgs[0].sectors[0].fa_off;
    rsp->br_hdr = &boot_data.imgs[slot].hdr;

    return 0;
}

int
split_go(int loader_slot, int split_slot, void **entry)
{
    const struct flash_area *loader_fap;
    const struct flash_area *app_fap;
    struct flash_area *sectors;
    uintptr_t entry_val;
    int loader_flash_id;
    int app_flash_id;
    int rc;

    app_fap = NULL;
    loader_fap = NULL;

    sectors = malloc(BOOT_MAX_IMG_SECTORS * 2 * sizeof *sectors);
    if (sectors == NULL) {
        rc = SPLIT_GO_ERR;
        goto done;
    }
    boot_data.imgs[0].sectors = sectors + 0;
    boot_data.imgs[1].sectors = sectors + BOOT_MAX_IMG_SECTORS;

    /* Determine the sector layout of the image slots and scratch area. */
    rc = boot_read_sectors();
    if (rc != 0) {
        rc = SPLIT_GO_ERR;
        goto done;
    }

    rc = boot_read_image_headers();
    if (rc != 0) {
        goto done;
    }

    app_flash_id = flash_area_id_from_image_slot(split_slot);
    rc = flash_area_open(app_flash_id, &app_fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    loader_flash_id = flash_area_id_from_image_slot(loader_slot);
    rc = flash_area_open(loader_flash_id, &loader_fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    /* Don't check the bootable image flag because we could really call a
     * bootable or non-bootable image.  Just validate that the image check
     * passes which is distinct from the normal check.
     */
    rc = split_image_check(&boot_data.imgs[split_slot].hdr,
                           app_fap,
                           &boot_data.imgs[loader_slot].hdr,
                           loader_fap);
    if (rc != 0) {
        rc = SPLIT_GO_NON_MATCHING;
        goto done;
    }

    entry_val = boot_data.imgs[split_slot].sectors[0].fa_off +
                boot_data.imgs[split_slot].hdr.ih_hdr_size;
    *entry = (void *) entry_val;
    rc = SPLIT_GO_OK;

done:
    free(sectors);
    flash_area_close(app_fap);
    flash_area_close(loader_fap);
    return rc;
}
