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

#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "os/os.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"

int boot_current_slot;

const uint32_t boot_img_magic[4] = {
    0xf395c277,
    0x7fefd260,
    0x0f505235,
    0x8079b62c,
};

struct boot_swap_table {
    /** * For each field, a value of 0 means "any". */
    uint8_t bsw_magic_slot0;
    uint8_t bsw_magic_slot1;
    uint8_t bsw_image_ok_slot0;

    uint8_t bsw_swap_type;
};

/**
 * This set of tables maps image trailer contents to swap operation type.
 * When searching for a match, these tables must be iterated sequentially.
 */
static const struct boot_swap_table boot_swap_tables[] = {
    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | Unset      | Unset      |
         * image-ok | Any        | N/A        |
         * ---------+------------+------------'
         * swap: none                         |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      BOOT_MAGIC_UNSET,
        .bsw_magic_slot1 =      BOOT_MAGIC_UNSET,
        .bsw_image_ok_slot0 =   0,
        .bsw_swap_type =        BOOT_SWAP_TYPE_NONE,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | Any        | Good       |
         * image-ok | Any        | N/A        |
         * ---------+------------+------------'
         * swap: test                         |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      0,
        .bsw_magic_slot1 =      BOOT_MAGIC_GOOD,
        .bsw_image_ok_slot0 =   0,
        .bsw_swap_type =        BOOT_SWAP_TYPE_TEST,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | Good       | Unset      |
         * image-ok | 0xff       | N/A        |
         * ---------+------------+------------'
         * swap: revert (test image running)  |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bsw_magic_slot1 =      BOOT_MAGIC_UNSET,
        .bsw_image_ok_slot0 =   0xff,
        .bsw_swap_type =        BOOT_SWAP_TYPE_REVERT,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | Good       | Unset      |
         * image-ok | 0x01       | N/A        |
         * ---------+------------+------------'
         * swap: none (confirmed test image)  |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      BOOT_MAGIC_GOOD,
        .bsw_magic_slot1 =      BOOT_MAGIC_UNSET,
        .bsw_image_ok_slot0 =   0x01,
        .bsw_swap_type =        BOOT_SWAP_TYPE_NONE,
    },
};

#define BOOT_SWAP_TABLES_COUNT \
    (sizeof boot_swap_tables / sizeof boot_swap_tables[0])

int
boot_magic_code(const uint32_t *magic)
{
    int i;

    if (memcmp(magic, boot_img_magic, sizeof boot_img_magic) == 0) {
        return BOOT_MAGIC_GOOD;
    }

    for (i = 0; i < 4; i++) {
        if (magic[i] == 0xffffffff) {
            return BOOT_MAGIC_UNSET;
        }
    }

    return BOOT_MAGIC_BAD;
}

uint32_t
boot_status_sz(uint8_t min_write_sz)
{
    return BOOT_STATUS_MAX_ENTRIES * BOOT_STATUS_STATE_COUNT * min_write_sz;
}

uint32_t
boot_trailer_sz(uint8_t min_write_sz)
{
    return sizeof boot_img_magic            +
           boot_status_sz(min_write_sz)     +
           min_write_sz * 2;
}

static uint32_t
boot_magic_off(const struct flash_area *fap)
{
    uint32_t off_from_end;
    uint8_t elem_sz;

    elem_sz = flash_area_align(fap);

    off_from_end = boot_trailer_sz(elem_sz);

    assert(off_from_end <= fap->fa_size);
    return fap->fa_size - off_from_end;
}

uint32_t
boot_status_off(const struct flash_area *fap)
{
    return boot_magic_off(fap) + sizeof boot_img_magic;
}

static uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    return fap->fa_size - flash_area_align(fap) * 2;
}

static uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    return fap->fa_size - flash_area_align(fap);
}

int
boot_read_swap_state(const struct flash_area *fap,
                     struct boot_swap_state *state)
{
    uint32_t magic[4];
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);
    rc = flash_area_read(fap, off, magic, sizeof magic);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    state->magic = boot_magic_code(magic);

    off = boot_copy_done_off(fap);
    rc = flash_area_read(fap, off, &state->copy_done, 1);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    off = boot_image_ok_off(fap);
    rc = flash_area_read(fap, off, &state->image_ok, 1);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

/**
 * Reads the image trailer from the scratch area.
 */
int
boot_read_swap_state_scratch(struct boot_swap_state *state)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &fap);
    if (rc) {
        rc = BOOT_EFLASH;
        goto done;
    }

    rc = boot_read_swap_state(fap, state);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

/**
 * Reads the image trailer from a given image slot.
 */
int
boot_read_swap_state_img(int slot, struct boot_swap_state *state)
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

    rc = boot_read_swap_state(fap, state);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

int
boot_write_magic(const struct flash_area *fap)
{
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);

    rc = flash_area_write(fap, off, boot_img_magic, sizeof boot_img_magic);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_write_copy_done(const struct flash_area *fap)
{
    uint32_t off;
    uint8_t val;
    int rc;

    off = boot_copy_done_off(fap);

    val = 1;
    rc = flash_area_write(fap, off, &val, 1);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_write_image_ok(const struct flash_area *fap)
{
    uint32_t off;
    uint8_t val;
    int rc;

    off = boot_image_ok_off(fap);

    val = 1;
    rc = flash_area_write(fap, off, &val, 1);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_swap_type(void)
{
    const struct boot_swap_table *table;
    struct boot_swap_state state_slot0;
    struct boot_swap_state state_slot1;
    int rc;
    int i;

    rc = boot_read_swap_state_img(0, &state_slot0);
    assert(rc == 0);

    rc = boot_read_swap_state_img(1, &state_slot1);
    assert(rc == 0);

    for (i = 0; i < BOOT_SWAP_TABLES_COUNT; i++) {
        table = boot_swap_tables + i;

        if ((table->bsw_magic_slot0     == 0    ||
             table->bsw_magic_slot0     == state_slot0.magic)           &&
            (table->bsw_magic_slot1     == 0    ||
             table->bsw_magic_slot1     == state_slot1.magic)           &&
            (table->bsw_image_ok_slot0  == 0    ||
             table->bsw_image_ok_slot0  == state_slot0.image_ok)) {

            return table->bsw_swap_type;
        }
    }

    assert(0);
    return BOOT_SWAP_TYPE_NONE;
}

/**
 * Marks the image in slot 1 as pending.  On the next reboot, the system will
 * perform a one-time boot of the slot 1 image.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_pending(void)
{
    const struct flash_area *fap;
    struct boot_swap_state state_slot1;
    int area_id;
    int rc;

    rc = boot_read_swap_state_img(1, &state_slot1);
    if (rc != 0) {
        return rc;
    }

    switch (state_slot1.magic) {
    case BOOT_MAGIC_GOOD:
        /* Swap already scheduled. */
        return 0;

    case BOOT_MAGIC_UNSET:
        area_id = flash_area_id_from_image_slot(1);
        rc = flash_area_open(area_id, &fap);
        if (rc != 0) {
            rc = BOOT_EFLASH;
        } else {
            rc = boot_write_magic(fap);
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

    rc = boot_read_swap_state_img(0, &state_slot0);
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

    if (state_slot0.copy_done == 0xff) {
        /* Swap never completed.  This is unexpected. */
        return BOOT_EBADVECT;
    }

    if (state_slot0.image_ok != 0xff) {
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
