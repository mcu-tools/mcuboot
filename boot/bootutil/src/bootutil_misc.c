/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
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

#include <string.h>
#include <inttypes.h>
#include <stddef.h>

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/fault_injection_hardening.h"
#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

BOOT_LOG_MODULE_DECLARE(mcuboot);

/* Currently only used by imgmgr */
int boot_current_slot;

/**
 * @brief Determine if the data at two memory addresses is equal
 *
 * @param s1    The first  memory region to compare.
 * @param s2    The second memory region to compare.
 * @param n     The amount of bytes to compare.
 *
 * @note        This function does not comply with the specification of memcmp,
 *              so should not be considered a drop-in replacement. It has no
 *              constant time execution. The point is to make sure that all the
 *              bytes are compared and detect if loop was abused and some cycles
 *              was skipped due to fault injection.
 *
 * @return      FIH_SUCCESS if memory regions are equal, otherwise FIH_FAILURE
 */
#ifdef MCUBOOT_FIH_PROFILE_OFF
inline
fih_int boot_fih_memequal(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}
#else
fih_int boot_fih_memequal(const void *s1, const void *s2, size_t n)
{
    size_t i;
    uint8_t *s1_p = (uint8_t*) s1;
    uint8_t *s2_p = (uint8_t*) s2;
    fih_int ret = FIH_FAILURE;

    for (i = 0; i < n; i++) {
        if (s1_p[i] != s2_p[i]) {
            goto out;
        }
    }
    if (i == n) {
        ret = FIH_SUCCESS;
    }

out:
    FIH_RET(ret);
}
#endif

/*
 * Amount of space used to save information required when doing a swap,
 * or while a swap is under progress, but not the status of sector swap
 * progress itself.
 */
static inline uint32_t
boot_trailer_info_sz(void)
{
    return (
#ifdef MCUBOOT_ENC_IMAGES
           /* encryption keys */
#  if MCUBOOT_SWAP_SAVE_ENCTLV
           BOOT_ENC_TLV_ALIGN_SIZE * 2            +
#  else
           BOOT_ENC_KEY_ALIGN_SIZE * 2            +
#  endif
#endif
           /* swap_type + copy_done + image_ok + swap_size */
           BOOT_MAX_ALIGN * 4                     +
           BOOT_MAGIC_ALIGN_SIZE
           );
}

/*
 * Amount of space used to maintain progress information for a single swap
 * operation.
 */
static inline uint32_t
boot_status_entry_sz(uint32_t min_write_sz)
{
    return BOOT_STATUS_STATE_COUNT * min_write_sz;
}

uint32_t
boot_status_sz(uint32_t min_write_sz)
{
    return BOOT_STATUS_MAX_ENTRIES * boot_status_entry_sz(min_write_sz);
}

uint32_t
boot_trailer_sz(uint32_t min_write_sz)
{
    return boot_status_sz(min_write_sz) + boot_trailer_info_sz();
}

#if MCUBOOT_SWAP_USING_SCRATCH
/*
 * Similar to `boot_trailer_sz` but this function returns the space used to
 * store status in the scratch partition. The scratch partition only stores
 * status during the swap of the last sector from primary/secondary (which
 * is the first swap operation) and thus only requires space for one swap.
 */
static uint32_t
boot_scratch_trailer_sz(uint32_t min_write_sz)
{
    return boot_status_entry_sz(min_write_sz) + boot_trailer_info_sz();
}
#endif

int
boot_status_entries(int image_index, const struct flash_area *fap)
{
#if MCUBOOT_SWAP_USING_SCRATCH
    if (flash_area_get_id(fap) == FLASH_AREA_IMAGE_SCRATCH) {
        return BOOT_STATUS_STATE_COUNT;
    } else
#endif
    if (flash_area_get_id(fap) == FLASH_AREA_IMAGE_PRIMARY(image_index) ||
        flash_area_get_id(fap) == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
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

#if MCUBOOT_SWAP_USING_SCRATCH
    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        off_from_end = boot_scratch_trailer_sz(elem_sz);
    } else {
#endif
        off_from_end = boot_trailer_sz(elem_sz);
#if MCUBOOT_SWAP_USING_SCRATCH
    }
#endif

    assert(off_from_end <= flash_area_get_size(fap));
    return flash_area_get_size(fap) - off_from_end;
}

static int
boot_magic_decode(const uint8_t *magic)
{
    if (memcmp(magic, BOOT_IMG_MAGIC, BOOT_MAGIC_SZ) == 0) {
        return BOOT_MAGIC_GOOD;
    }
    return BOOT_MAGIC_BAD;
}

static inline uint32_t
boot_magic_off(const struct flash_area *fap)
{
    return flash_area_get_size(fap) - BOOT_MAGIC_SZ;
}

static inline uint32_t
boot_image_ok_off(const struct flash_area *fap)
{
    return ALIGN_DOWN(boot_magic_off(fap) - BOOT_MAX_ALIGN, BOOT_MAX_ALIGN);
}

static inline uint32_t
boot_copy_done_off(const struct flash_area *fap)
{
    return boot_image_ok_off(fap) - BOOT_MAX_ALIGN;
}

static inline uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return boot_swap_info_off(fap) - BOOT_MAX_ALIGN;
}

#ifdef MCUBOOT_ENC_IMAGES
static inline uint32_t
boot_enc_key_off(const struct flash_area *fap, uint8_t slot)
{
#if MCUBOOT_SWAP_SAVE_ENCTLV
    return boot_swap_size_off(fap) - ((slot + 1) * BOOT_ENC_TLV_ALIGN_SIZE);
#else
    return boot_swap_size_off(fap) - ((slot + 1) * BOOT_ENC_KEY_ALIGN_SIZE);
#endif
}
#endif

/**
 * This functions tries to locate the status area after an aborted swap,
 * by looking for the magic in the possible locations.
 *
 * If the magic is successfully found, a flash_area * is returned and it
 * is the responsibility of the called to close it.
 *
 * @returns 0 on success, -1 on errors
 */
static int
boot_find_status(int image_index, const struct flash_area **fap)
{
    uint8_t magic[BOOT_MAGIC_SZ];
    uint32_t off;
    uint8_t areas[2] = {
#if MCUBOOT_SWAP_USING_SCRATCH
        FLASH_AREA_IMAGE_SCRATCH,
#endif
        FLASH_AREA_IMAGE_PRIMARY(image_index),
    };
    unsigned int i;
    int rc;

    /*
     * In the middle a swap, tries to locate the area that is currently
     * storing a valid magic, first on the primary slot, then on scratch.
     * Both "slots" can end up being temporary storage for a swap and it
     * is assumed that if magic is valid then other metadata is too,
     * because magic is always written in the last step.
     */

    for (i = 0; i < sizeof(areas) / sizeof(areas[0]); i++) {
        rc = flash_area_open(areas[i], fap);
        if (rc != 0) {
            return rc;
        }

        off = boot_magic_off(*fap);
        rc = flash_area_read(*fap, off, magic, BOOT_MAGIC_SZ);
        flash_area_close(*fap);

        if (rc != 0) {
            return rc;
        }

        if (BOOT_MAGIC_GOOD == boot_magic_decode(magic)) {
            return 0;
        }

    }

    /* If we got here, no magic was found */
    return -1;
}

int
boot_read_swap_size(int image_index, uint32_t *swap_size)
{
    uint32_t off;
    const struct flash_area *fap;
    int rc;

    rc = boot_find_status(image_index, &fap);
    if (rc == 0) {
        off = boot_swap_size_off(fap);
        rc = flash_area_read(fap, off, swap_size, sizeof *swap_size);
        flash_area_close(fap);
    }

    return rc;
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_read_enc_key(int image_index, uint8_t slot, struct boot_status *bs)
{
    uint32_t off;
    const struct flash_area *fap;
#if MCUBOOT_SWAP_SAVE_ENCTLV
    int i;
#endif
    int rc;

    rc = boot_find_status(image_index, &fap);
    if (rc == 0) {
        off = boot_enc_key_off(fap, slot);
#if MCUBOOT_SWAP_SAVE_ENCTLV
        rc = flash_area_read(fap, off, bs->enctlv[slot], BOOT_ENC_TLV_ALIGN_SIZE);
        if (rc == 0) {
            for (i = 0; i < BOOT_ENC_TLV_ALIGN_SIZE; i++) {
                if (bs->enctlv[slot][i] != 0xff) {
                    break;
                }
            }
            /* Only try to decrypt non-erased TLV metadata */
            if (i != BOOT_ENC_TLV_ALIGN_SIZE) {
                rc = boot_enc_decrypt(bs->enctlv[slot], bs->enckey[slot]);
            }
        }
#else
        rc = flash_area_read(fap, off, bs->enckey[slot], BOOT_ENC_KEY_ALIGN_SIZE);
#endif
        flash_area_close(fap);
    }

    return rc;
}
#endif

int
boot_write_copy_done(const struct flash_area *fap)
{
    uint32_t off;

    off = boot_copy_done_off(fap);
    BOOT_LOG_DBG("writing copy_done; fa_id=%d off=0x%lx (0x%lx)",
                 flash_area_get_id(fap), (unsigned long)off,
                 (unsigned long)(flash_area_get_off(fap) + off));
    return boot_write_trailer_flag(fap, off, BOOT_FLAG_SET);
}

int
boot_write_swap_size(const struct flash_area *fap, uint32_t swap_size)
{
    uint32_t off;

    off = boot_swap_size_off(fap);
    BOOT_LOG_DBG("writing swap_size; fa_id=%d off=0x%lx (0x%lx)",
                 flash_area_get_id(fap), (unsigned long)off,
                 (unsigned long)flash_area_get_off(fap) + off);
    return boot_write_trailer(fap, off, (const uint8_t *) &swap_size, 4);
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_write_enc_key(const struct flash_area *fap, uint8_t slot,
        const struct boot_status *bs)
{
    uint32_t off;
    int rc;

    off = boot_enc_key_off(fap, slot);
    BOOT_LOG_DBG("writing enc_key; fa_id=%d off=0x%lx (0x%lx)",
                 flash_area_get_id(fap), (unsigned long)off,
                 (unsigned long)flash_area_get_off(fap) + off);
#if MCUBOOT_SWAP_SAVE_ENCTLV
    rc = flash_area_write(fap, off, bs->enctlv[slot], BOOT_ENC_TLV_ALIGN_SIZE);
#else
    rc = flash_area_write(fap, off, bs->enckey[slot], BOOT_ENC_KEY_ALIGN_SIZE);
#endif
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}
#endif
