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

#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>

#include "sysflash/sysflash.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"

#include "flash_map_backend/flash_map_backend.h"

#include "os/os.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"

int boot_current_slot;

const uint32_t boot_img_magic[] = {
    0xf395c277,
    0x7fefd260,
    0x0f505235,
    0x8079b62c,
};

const uint32_t BOOT_MAGIC_SZ = sizeof boot_img_magic;
const uint32_t BOOT_MAX_ALIGN = MAX_FLASH_ALIGN;

struct boot_swap_table {
    /** * For each field, a value of 0 means "any". */
    uint8_t magic_slot0;
    uint8_t magic_slot1;
    uint8_t image_ok_slot0;
    uint8_t image_ok_slot1;
    uint8_t copy_done_slot0;

    uint8_t swap_type;
};

/**
 * This set of tables maps image trailer contents to swap operation type.
 * When searching for a match, these tables must be iterated sequentially.
 *
 * NOTE: the table order is very important. The settings in Slot 1 always
 * are priority to Slot 0 and should be located earlier in the table.
 *
 * The table lists only states where there is action needs to be taken by
 * the bootloader, as in starting/finishing a swap operation.
 */
static const struct boot_swap_table boot_swap_tables[] = {
    {
        .magic_slot0 =      0,
        .magic_slot1 =      BOOT_MAGIC_GOOD,
        .image_ok_slot0 =   0,
        .image_ok_slot1 =   0xff,
        .copy_done_slot0 =  0,
        .swap_type =        BOOT_SWAP_TYPE_TEST,
    },
    {
        .magic_slot0 =      0,
        .magic_slot1 =      BOOT_MAGIC_GOOD,
        .image_ok_slot0 =   0,
        .image_ok_slot1 =   0x01,
        .copy_done_slot0 =  0,
        .swap_type =        BOOT_SWAP_TYPE_PERM,
    },
    {
        .magic_slot0 =      BOOT_MAGIC_GOOD,
        .magic_slot1 =      BOOT_MAGIC_UNSET,
        .image_ok_slot0 =   0xff,
        .image_ok_slot1 =   0,
        .copy_done_slot0 =  0x01,
        .swap_type =        BOOT_SWAP_TYPE_REVERT,
    },
};

#define BOOT_SWAP_TABLES_COUNT \
    (sizeof boot_swap_tables / sizeof boot_swap_tables[0])

int
boot_magic_code(const uint32_t *magic)
{
    size_t i;

    if (memcmp(magic, boot_img_magic, BOOT_MAGIC_SZ) == 0) {
        return BOOT_MAGIC_GOOD;
    }

    for (i = 0; i < BOOT_MAGIC_SZ / sizeof *magic; i++) {
        if (magic[i] != 0xffffffff) {
            return BOOT_MAGIC_BAD;
        }
    }

    return BOOT_MAGIC_UNSET;
}

uint32_t
boot_slots_trailer_sz(uint8_t min_write_sz)
{
    return /* state for all sectors */
           BOOT_STATUS_MAX_ENTRIES * BOOT_STATUS_STATE_COUNT * min_write_sz +
           BOOT_MAX_ALIGN * 3 /* copy_done + image_ok + swap_size */        +
           BOOT_MAGIC_SZ;
}

static uint32_t
boot_scratch_trailer_sz(uint8_t min_write_sz)
{
    return BOOT_STATUS_STATE_COUNT * min_write_sz +  /* state for one sector */
           BOOT_MAX_ALIGN * 2                     +  /* image_ok + swap_size */
           BOOT_MAGIC_SZ;
}

static uint32_t
boot_magic_off(const struct flash_area *fap)
{
    assert(offsetof(struct image_trailer, magic) == 16);
    return fap->fa_size - BOOT_MAGIC_SZ;
}

int
boot_status_entries(const struct flash_area *fap)
{
    switch (fap->fa_id) {
    case FLASH_AREA_IMAGE_0:
    case FLASH_AREA_IMAGE_1:
        return BOOT_STATUS_STATE_COUNT * BOOT_STATUS_MAX_ENTRIES;
    case FLASH_AREA_IMAGE_SCRATCH:
        return BOOT_STATUS_STATE_COUNT;
    default:
        return BOOT_EBADARGS;
    }
}

uint32_t
boot_status_off(const struct flash_area *fap)
{
    uint32_t off_from_end;
    uint8_t elem_sz;

    elem_sz = flash_area_align(fap);

    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        off_from_end = boot_scratch_trailer_sz(elem_sz);
    } else {
        off_from_end = boot_slots_trailer_sz(elem_sz);
    }

    assert(off_from_end <= fap->fa_size);
    return fap->fa_size - off_from_end;
}

static uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    assert(fap->fa_id != FLASH_AREA_IMAGE_SCRATCH);
    assert(offsetof(struct image_trailer, copy_done) == 0);
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 2;
}

static uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    assert(offsetof(struct image_trailer, image_ok) == 8);
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN;
}

static uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    /*
     * The "swap_size" field if located just before the trailer.
     * The scratch slot doesn't store "copy_done"...
     */
    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 2;
    }

    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 3;
}

int
boot_read_swap_state(const struct flash_area *fap,
                     struct boot_swap_state *state)
{
    uint32_t magic[BOOT_MAGIC_SZ];
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);
    rc = flash_area_read(fap, off, magic, BOOT_MAGIC_SZ);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    state->magic = boot_magic_code(magic);

    if (fap->fa_id != FLASH_AREA_IMAGE_SCRATCH) {
        off = boot_copy_done_off(fap);
        rc = flash_area_read(fap, off, &state->copy_done, sizeof state->copy_done);
        if (rc != 0) {
            return BOOT_EFLASH;
        }
    }

    off = boot_image_ok_off(fap);
    rc = flash_area_read(fap, off, &state->image_ok, sizeof state->image_ok);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

/**
 * Reads the image trailer from the scratch area.
 */
int
boot_read_swap_state_by_id(int flash_area_id, struct boot_swap_state *state)
{
    const struct flash_area *fap;
    int rc;

    switch (flash_area_id) {
    case FLASH_AREA_IMAGE_SCRATCH:
    case FLASH_AREA_IMAGE_0:
    case FLASH_AREA_IMAGE_1:
        rc = flash_area_open(flash_area_id, &fap);
        if (rc != 0) {
            return BOOT_EFLASH;
        }
        break;
    default:
        return BOOT_EBADARGS;
    }

    rc = boot_read_swap_state(fap, state);
    flash_area_close(fap);
    return rc;
}

int
boot_read_swap_size(uint32_t *swap_size)
{
    uint32_t magic[BOOT_MAGIC_SZ];
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    /*
     * In the middle a swap, tries to locate the saved swap size. Looks
     * for a valid magic, first on Slot 0, then on scratch. Both "slots"
     * can end up being temporary storage for a swap and it is assumed
     * that if magic is valid then swap size is too, because magic is
     * always written in the last step.
     */

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    off = boot_magic_off(fap);
    rc = flash_area_read(fap, off, magic, BOOT_MAGIC_SZ);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto out;
    }

    if (memcmp(magic, boot_img_magic, BOOT_MAGIC_SZ) != 0) {
        /*
         * If Slot 0 's magic is not valid, try scratch...
         */

        flash_area_close(fap);

        rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &fap);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        off = boot_magic_off(fap);
        rc = flash_area_read(fap, off, magic, BOOT_MAGIC_SZ);
        if (rc != 0) {
            rc = BOOT_EFLASH;
            goto out;
        }

        assert(memcmp(magic, boot_img_magic, BOOT_MAGIC_SZ) == 0);
    }

    off = boot_swap_size_off(fap);
    rc = flash_area_read(fap, off, swap_size, sizeof *swap_size);
    if (rc != 0) {
        rc = BOOT_EFLASH;
    }

out:
    flash_area_close(fap);
    return rc;
}


int
boot_write_magic(const struct flash_area *fap)
{
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);

    rc = flash_area_write(fap, off, boot_img_magic, BOOT_MAGIC_SZ);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

static int
boot_write_flag(int flag, const struct flash_area *fap)
{
    uint32_t off;
    int rc;
    uint8_t buf[BOOT_MAX_ALIGN];
    uint8_t align;

    switch (flag) {
    case BOOT_FLAG_COPY_DONE:
        off = boot_copy_done_off(fap);
        break;
    case BOOT_FLAG_IMAGE_OK:
        off = boot_image_ok_off(fap);
        break;
    default:
        return BOOT_EBADARGS;
    }

    align = flash_area_align(fap);
    assert(align <= BOOT_MAX_ALIGN);
    memset(buf, 0xFF, BOOT_MAX_ALIGN);
    buf[0] = BOOT_FLAG_SET;

    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_write_copy_done(const struct flash_area *fap)
{
    return boot_write_flag(BOOT_FLAG_COPY_DONE, fap);
}

int
boot_write_image_ok(const struct flash_area *fap)
{
    return boot_write_flag(BOOT_FLAG_IMAGE_OK, fap);
}

int
boot_write_swap_size(const struct flash_area *fap, uint32_t swap_size)
{
    uint32_t off;
    int rc;
    uint8_t buf[BOOT_MAX_ALIGN];
    uint8_t align;

    off = boot_swap_size_off(fap);
    align = flash_area_align(fap);
    assert(align <= BOOT_MAX_ALIGN);
    if (align < sizeof swap_size) {
        align = sizeof swap_size;
    }
    memset(buf, 0xFF, BOOT_MAX_ALIGN);
    memcpy(buf, (uint8_t *)&swap_size, sizeof swap_size);

    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_swap_type(void)
{
    const struct boot_swap_table *table;
    struct boot_swap_state slot0;
    struct boot_swap_state slot1;
    int rc;
    size_t i;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_0, &slot0);
    if (rc) {
        return BOOT_SWAP_TYPE_PANIC;
    }

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_1, &slot1);
    if (rc) {
        return BOOT_SWAP_TYPE_PANIC;
    }

    for (i = 0; i < BOOT_SWAP_TABLES_COUNT; i++) {
        table = boot_swap_tables + i;

        if ((!table->magic_slot0     || table->magic_slot0     == slot0.magic    ) &&
            (!table->magic_slot1     || table->magic_slot1     == slot1.magic    ) &&
            (!table->image_ok_slot0  || table->image_ok_slot0  == slot0.image_ok ) &&
            (!table->image_ok_slot1  || table->image_ok_slot1  == slot1.image_ok ) &&
            (!table->copy_done_slot0 || table->copy_done_slot0 == slot0.copy_done)) {
            BOOT_LOG_INF("Swap type: %s",
                         table->swap_type == BOOT_SWAP_TYPE_TEST   ? "test"   :
                         table->swap_type == BOOT_SWAP_TYPE_PERM   ? "perm"   :
                         table->swap_type == BOOT_SWAP_TYPE_REVERT ? "revert" :
                         "BUG; can't happen");
            assert(table->swap_type == BOOT_SWAP_TYPE_TEST ||
                   table->swap_type == BOOT_SWAP_TYPE_PERM ||
                   table->swap_type == BOOT_SWAP_TYPE_REVERT);
            return table->swap_type;
        }
    }

    BOOT_LOG_INF("Swap type: none");
    return BOOT_SWAP_TYPE_NONE;
}

/**
 * Marks the image in slot 1 as pending.  On the next reboot, the system will
 * perform a one-time boot of the slot 1 image.
 *
 * @param permanent         Whether the image should be used permanently or
 *                              only tested once:
 *                                  0=run image once, then confirm or revert.
 *                                  1=run image forever.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_pending(int permanent)
{
    const struct flash_area *fap;
    struct boot_swap_state state_slot1;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_1, &state_slot1);
    if (rc != 0) {
        return rc;
    }

    switch (state_slot1.magic) {
    case BOOT_MAGIC_GOOD:
        /* Swap already scheduled. */
        return 0;

    case BOOT_MAGIC_UNSET:
        rc = flash_area_open(FLASH_AREA_IMAGE_1, &fap);
        if (rc != 0) {
            rc = BOOT_EFLASH;
        } else {
            rc = boot_write_magic(fap);
        }

        if (rc == 0 && permanent) {
            rc = boot_write_image_ok(fap);
        }

        flash_area_close(fap);
        return rc;

    default:
        /* XXX: Temporary assert. */
        assert(0);
        return -1;
    }
}

/**
 * Marks the image in slot 0 as confirmed.  The system will continue booting into the image in slot 0 until told to boot from a different slot.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_confirmed(void)
{
    const struct flash_area *fap;
    struct boot_swap_state state_slot0;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_0, &state_slot0);
    if (rc != 0) {
        return rc;
    }

    switch (state_slot0.magic) {
    case BOOT_MAGIC_GOOD:
        /* Confirm needed; proceed. */
        break;

    case BOOT_MAGIC_UNSET:
        /* Already confirmed. */
        return 0;

    case BOOT_MAGIC_BAD:
        /* Unexpected state. */
        return BOOT_EBADVECT;
    }

    if (state_slot0.copy_done == BOOT_FLAG_UNSET) {
        /* Swap never completed.  This is unexpected. */
        return BOOT_EBADVECT;
    }

    if (state_slot0.image_ok != BOOT_FLAG_UNSET) {
        /* Already confirmed. */
        return 0;
    }

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = boot_write_image_ok(fap);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}
