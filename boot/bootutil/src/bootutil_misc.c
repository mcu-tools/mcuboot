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

/*
 * Modifications are Copyright (c) 2019 Arm Limited.
 */

#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"
#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

/* Currently only used by imgmgr */
int boot_current_slot;

const uint32_t boot_img_magic[] = {
    0xf395c277,
    0x7fefd260,
    0x0f505235,
    0x8079b62c,
};

#define BOOT_MAGIC_ARR_SZ \
    (sizeof boot_img_magic / sizeof boot_img_magic[0])

const uint32_t BOOT_MAGIC_SZ = sizeof boot_img_magic;
const uint32_t BOOT_MAX_ALIGN = MAX_FLASH_ALIGN;

struct boot_swap_table {
    uint8_t magic_primary_slot;
    uint8_t magic_secondary_slot;
    uint8_t image_ok_primary_slot;
    uint8_t image_ok_secondary_slot;
    uint8_t copy_done_primary_slot;

    uint8_t swap_type;
};

/**
 * This set of tables maps image trailer contents to swap operation type.
 * When searching for a match, these tables must be iterated sequentially.
 *
 * NOTE: the table order is very important. The settings in the secondary
 * slot always are priority to the primary slot and should be located
 * earlier in the table.
 *
 * The table lists only states where there is action needs to be taken by
 * the bootloader, as in starting/finishing a swap operation.
 */
static const struct boot_swap_table boot_swap_tables[] = {
    {
        .magic_primary_slot =       BOOT_MAGIC_ANY,
        .magic_secondary_slot =     BOOT_MAGIC_GOOD,
        .image_ok_primary_slot =    BOOT_FLAG_ANY,
        .image_ok_secondary_slot =  BOOT_FLAG_UNSET,
        .copy_done_primary_slot =   BOOT_FLAG_ANY,
        .swap_type =                BOOT_SWAP_TYPE_TEST,
    },
    {
        .magic_primary_slot =       BOOT_MAGIC_ANY,
        .magic_secondary_slot =     BOOT_MAGIC_GOOD,
        .image_ok_primary_slot =    BOOT_FLAG_ANY,
        .image_ok_secondary_slot =  BOOT_FLAG_SET,
        .copy_done_primary_slot =   BOOT_FLAG_ANY,
        .swap_type =                BOOT_SWAP_TYPE_PERM,
    },
    {
        .magic_primary_slot =       BOOT_MAGIC_GOOD,
        .magic_secondary_slot =     BOOT_MAGIC_UNSET,
        .image_ok_primary_slot =    BOOT_FLAG_UNSET,
        .image_ok_secondary_slot =  BOOT_FLAG_ANY,
        .copy_done_primary_slot =   BOOT_FLAG_SET,
        .swap_type =                BOOT_SWAP_TYPE_REVERT,
    },
};

#define BOOT_SWAP_TABLES_COUNT \
    (sizeof boot_swap_tables / sizeof boot_swap_tables[0])

static int
boot_magic_decode(const uint32_t *magic)
{
    if (memcmp(magic, boot_img_magic, BOOT_MAGIC_SZ) == 0) {
        return BOOT_MAGIC_GOOD;
    }
    return BOOT_MAGIC_BAD;
}

static int
boot_flag_decode(uint8_t flag)
{
    if (flag != BOOT_FLAG_SET) {
        return BOOT_FLAG_BAD;
    }
    return BOOT_FLAG_SET;
}

/**
 * Determines if a status source table is satisfied by the specified magic
 * code.
 *
 * @param tbl_val               A magic field from a status source table.
 * @param val                   The magic value in a trailer, encoded as a
 *                                  BOOT_MAGIC_[...].
 *
 * @return                      1 if the two values are compatible;
 *                              0 otherwise.
 */
int
boot_magic_compatible_check(uint8_t tbl_val, uint8_t val)
{
    switch (tbl_val) {
    case BOOT_MAGIC_ANY:
        return 1;

    case BOOT_MAGIC_NOTGOOD:
        return val != BOOT_MAGIC_GOOD;

    default:
        return tbl_val == val;
    }
}

uint32_t
boot_trailer_sz(uint8_t min_write_sz)
{
    return /* state for all sectors */
           BOOT_STATUS_MAX_ENTRIES * BOOT_STATUS_STATE_COUNT * min_write_sz +
#ifdef MCUBOOT_ENC_IMAGES
           /* encryption keys */
           BOOT_ENC_KEY_SIZE * 2                  +
#endif
           /* swap_type + copy_done + image_ok + swap_size */
           BOOT_MAX_ALIGN * 4                     +
           BOOT_MAGIC_SZ;
}

static uint32_t
boot_magic_off(const struct flash_area *fap)
{
    return fap->fa_size - BOOT_MAGIC_SZ;
}

int
boot_status_entries(int image_index, const struct flash_area *fap)
{
    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        return BOOT_STATUS_STATE_COUNT;
    } else if (fap->fa_id == FLASH_AREA_IMAGE_PRIMARY(image_index) ||
               fap->fa_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return BOOT_STATUS_STATE_COUNT * BOOT_STATUS_MAX_ENTRIES;
    }
    return -1;
}

uint32_t
boot_status_off(const struct flash_area *fap)
{
    uint32_t off_from_end;
    uint8_t elem_sz;

    elem_sz = flash_area_align(fap);

    off_from_end = boot_trailer_sz(elem_sz);

    assert(off_from_end <= fap->fa_size);
    return fap->fa_size - off_from_end;
}

uint32_t
boot_swap_info_off(const struct flash_area *fap)
{
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 3;
}

static uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 2;
}

static uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN;
}

static uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 4;
}

#ifdef MCUBOOT_ENC_IMAGES
static uint32_t
boot_enc_key_off(const struct flash_area *fap, uint8_t slot)
{
    return fap->fa_size - BOOT_MAGIC_SZ - BOOT_MAX_ALIGN * 4 -
                          ((slot + 1) * BOOT_ENC_KEY_SIZE);
}
#endif

int
boot_read_swap_state(const struct flash_area *fap,
                     struct boot_swap_state *state)
{
    uint32_t magic[BOOT_MAGIC_ARR_SZ];
    uint32_t off;
    uint8_t swap_info;
    int rc;

    off = boot_magic_off(fap);
    rc = flash_area_read_is_empty(fap, off, magic, BOOT_MAGIC_SZ);
    if (rc < 0) {
        return BOOT_EFLASH;
    }
    if (rc == 1) {
        state->magic = BOOT_MAGIC_UNSET;
    } else {
        state->magic = boot_magic_decode(magic);
    }

    off = boot_swap_info_off(fap);
    rc = flash_area_read_is_empty(fap, off, &swap_info, sizeof swap_info);
    if (rc < 0) {
        return BOOT_EFLASH;
    }

    /* Extract the swap type and image number */
    state->swap_type = BOOT_GET_SWAP_TYPE(swap_info);
    state->image_num = BOOT_GET_IMAGE_NUM(swap_info);

    if (rc == 1 || state->swap_type > BOOT_SWAP_TYPE_REVERT) {
        state->swap_type = BOOT_SWAP_TYPE_NONE;
        state->image_num = 0;
    }

    off = boot_copy_done_off(fap);
    rc = flash_area_read_is_empty(fap, off, &state->copy_done,
            sizeof state->copy_done);
    if (rc < 0) {
        return BOOT_EFLASH;
    }
    if (rc == 1) {
        state->copy_done = BOOT_FLAG_UNSET;
    } else {
        state->copy_done = boot_flag_decode(state->copy_done);
    }

    off = boot_image_ok_off(fap);
    rc = flash_area_read_is_empty(fap, off, &state->image_ok,
                                  sizeof state->image_ok);
    if (rc < 0) {
        return BOOT_EFLASH;
    }
    if (rc == 1) {
        state->image_ok = BOOT_FLAG_UNSET;
    } else {
        state->image_ok = boot_flag_decode(state->image_ok);
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

    rc = flash_area_open(flash_area_id, &fap);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_read_swap_state(fap, state);
    flash_area_close(fap);
    return rc;
}

int
boot_read_swap_size(int image_index, uint32_t *swap_size)
{
    uint32_t magic[BOOT_MAGIC_ARR_SZ];
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    /*
     * In the middle a swap, tries to locate the saved swap size. Looks
     * for a valid magic, first on the primary slot, then on scratch.
     * Both "slots" can end up being temporary storage for a swap and it
     * is assumed that if magic is valid then swap size is too, because
     * magic is always written in the last step.
     */

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(image_index), &fap);
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
         * If the primary slot's magic is not valid, try scratch...
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

#ifdef MCUBOOT_ENC_IMAGES
int
boot_read_enc_key(int image_index, uint8_t slot, uint8_t *enckey)
{
    uint32_t magic[BOOT_MAGIC_SZ];
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(image_index), &fap);
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
         * If the primary slot's magic is not valid, try scratch...
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

    off = boot_enc_key_off(fap, slot);
    rc = flash_area_read(fap, off, enckey, BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        rc = BOOT_EFLASH;
    }

out:
    flash_area_close(fap);
    return rc;
}
#endif

int
boot_write_magic(const struct flash_area *fap)
{
    uint32_t off;
    int rc;

    off = boot_magic_off(fap);

    BOOT_LOG_DBG("writing magic; fa_id=%d off=0x%x (0x%x)",
                 fap->fa_id, off, fap->fa_off + off);
    rc = flash_area_write(fap, off, boot_img_magic, BOOT_MAGIC_SZ);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

static int
boot_write_trailer_byte(const struct flash_area *fap, uint32_t off,
                        uint8_t val)
{
    uint8_t buf[BOOT_MAX_ALIGN];
    uint8_t align;
    uint8_t erased_val;
    int rc;

    align = flash_area_align(fap);
    assert(align <= BOOT_MAX_ALIGN);
    erased_val = flash_area_erased_val(fap);
    memset(buf, erased_val, BOOT_MAX_ALIGN);
    buf[0] = val;

    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

int
boot_write_copy_done(const struct flash_area *fap)
{
    uint32_t off;

    off = boot_copy_done_off(fap);
    BOOT_LOG_DBG("writing copy_done; fa_id=%d off=0x%x (0x%x)",
                 fap->fa_id, off, fap->fa_off + off);
    return boot_write_trailer_byte(fap, off, BOOT_FLAG_SET);
}

int
boot_write_image_ok(const struct flash_area *fap)
{
    uint32_t off;

    off = boot_image_ok_off(fap);
    BOOT_LOG_DBG("writing image_ok; fa_id=%d off=0x%x (0x%x)",
                 fap->fa_id, off, fap->fa_off + off);
    return boot_write_trailer_byte(fap, off, BOOT_FLAG_SET);
}

/**
 * Writes the specified value to the `swap-type` field of an image trailer.
 * This value is persisted so that the boot loader knows what swap operation to
 * resume in case of an unexpected reset.
 */
int
boot_write_swap_info(const struct flash_area *fap, uint8_t swap_type,
                     uint8_t image_num)
{
    uint32_t off;
    uint8_t swap_info;

    BOOT_SET_SWAP_INFO(swap_info, image_num, swap_type);
    off = boot_swap_info_off(fap);
    BOOT_LOG_DBG("writing swap_info; fa_id=%d off=0x%x (0x%x), swap_type=0x%x"
                 " image_num=0x%x",
                 fap->fa_id, off, fap->fa_off + off, swap_type, image_num);
    return boot_write_trailer_byte(fap, off, swap_info);
}

int
boot_write_swap_size(const struct flash_area *fap, uint32_t swap_size)
{
    uint32_t off;
    int rc;
    uint8_t buf[BOOT_MAX_ALIGN];
    uint8_t align;
    uint8_t erased_val;

    off = boot_swap_size_off(fap);
    align = flash_area_align(fap);
    assert(align <= BOOT_MAX_ALIGN);
    if (align < sizeof swap_size) {
        align = sizeof swap_size;
    }
    erased_val = flash_area_erased_val(fap);
    memset(buf, erased_val, BOOT_MAX_ALIGN);
    memcpy(buf, (uint8_t *)&swap_size, sizeof swap_size);

    BOOT_LOG_DBG("writing swap_size; fa_id=%d off=0x%x (0x%x)",
                 fap->fa_id, off, fap->fa_off + off);

    rc = flash_area_write(fap, off, buf, align);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_write_enc_key(const struct flash_area *fap, uint8_t slot, const uint8_t *enckey)
{
    uint32_t off;
    int rc;

    off = boot_enc_key_off(fap, slot);
    rc = flash_area_write(fap, off, enckey, BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}
#endif

int
boot_swap_type_multi(int image_index)
{
    const struct boot_swap_table *table;
    struct boot_swap_state primary_slot;
    struct boot_swap_state secondary_slot;
    int rc;
    size_t i;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_PRIMARY(image_index),
                                    &primary_slot);
    if (rc) {
        return BOOT_SWAP_TYPE_PANIC;
    }

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SECONDARY(image_index),
                                    &secondary_slot);
    if (rc) {
        return BOOT_SWAP_TYPE_PANIC;
    }

    for (i = 0; i < BOOT_SWAP_TABLES_COUNT; i++) {
        table = boot_swap_tables + i;

        if (boot_magic_compatible_check(table->magic_primary_slot,
                                        primary_slot.magic) &&
            boot_magic_compatible_check(table->magic_secondary_slot,
                                        secondary_slot.magic) &&
            (table->image_ok_primary_slot == BOOT_FLAG_ANY   ||
                table->image_ok_primary_slot == primary_slot.image_ok) &&
            (table->image_ok_secondary_slot == BOOT_FLAG_ANY ||
                table->image_ok_secondary_slot == secondary_slot.image_ok) &&
            (table->copy_done_primary_slot == BOOT_FLAG_ANY  ||
                table->copy_done_primary_slot == primary_slot.copy_done)) {
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

int
boot_swap_type(void)
{
    return boot_swap_type_multi(0);
}


/**
 * Marks the image in the secondary slot as pending.  On the next reboot,
 * the system will perform a one-time boot of the the secondary slot image.
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
    struct boot_swap_state state_secondary_slot;
    uint8_t swap_type;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SECONDARY(0),
                                    &state_secondary_slot);
    if (rc != 0) {
        return rc;
    }

    switch (state_secondary_slot.magic) {
    case BOOT_MAGIC_GOOD:
        /* Swap already scheduled. */
        return 0;

    case BOOT_MAGIC_UNSET:
        rc = flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap);
        if (rc != 0) {
            rc = BOOT_EFLASH;
        } else {
            rc = boot_write_magic(fap);
        }

        if (rc == 0 && permanent) {
            rc = boot_write_image_ok(fap);
        }

        if (rc == 0) {
            if (permanent) {
                swap_type = BOOT_SWAP_TYPE_PERM;
            } else {
                swap_type = BOOT_SWAP_TYPE_TEST;
            }
            rc = boot_write_swap_info(fap, swap_type, 0);
        }

        flash_area_close(fap);
        return rc;

    case BOOT_MAGIC_BAD:
        /* The image slot is corrupt.  There is no way to recover, so erase the
         * slot to allow future upgrades.
         */
        rc = flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        flash_area_erase(fap, 0, fap->fa_size);
        flash_area_close(fap);
        return BOOT_EBADIMAGE;

    default:
        assert(0);
        return BOOT_EBADIMAGE;
    }
}

/**
 * Marks the image in the primary slot as confirmed.  The system will continue
 * booting into the image in the primary slot until told to boot from a
 * different slot.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_confirmed(void)
{
    const struct flash_area *fap;
    struct boot_swap_state state_primary_slot;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_IMAGE_PRIMARY(0),
                                    &state_primary_slot);
    if (rc != 0) {
        return rc;
    }

    switch (state_primary_slot.magic) {
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

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &fap);
    if (rc) {
        rc = BOOT_EFLASH;
        goto done;
    }

    if (state_primary_slot.copy_done == BOOT_FLAG_UNSET) {
        /* Swap never completed.  This is unexpected. */
        rc = BOOT_EBADVECT;
        goto done;
    }

    if (state_primary_slot.image_ok != BOOT_FLAG_UNSET) {
        /* Already confirmed. */
        goto done;
    }

    rc = boot_write_image_ok(fap);

done:
    flash_area_close(fap);
    return rc;
}

#if (BOOT_IMAGE_NUMBER > 1)
/**
 * Check if the version of the image is not older than required.
 *
 * @param req         Required minimal image version.
 * @param ver         Version of the image to be checked.
 *
 * @return            0 if the version is sufficient, nonzero otherwise.
 */
int
boot_is_version_sufficient(struct image_version *req,
                           struct image_version *ver)
{
    if (ver->iv_major > req->iv_major) {
        return 0;
    }
    if (ver->iv_major < req->iv_major) {
        return BOOT_EBADVERSION;
    }
    /* The major version numbers are equal. */
    if (ver->iv_minor > req->iv_minor) {
        return 0;
    }
    if (ver->iv_minor < req->iv_minor) {
        return BOOT_EBADVERSION;
    }
    /* The minor version numbers are equal. */
    if (ver->iv_revision < req->iv_revision) {
        return BOOT_EBADVERSION;
    }

    return 0;
}
#endif /* BOOT_IMAGE_NUMBER > 1 */
