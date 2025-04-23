/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
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
#include "bootutil_misc.h"
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
fih_ret boot_fih_memequal(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}
#else
fih_ret boot_fih_memequal(const void *s1, const void *s2, size_t n)
{
    size_t i;
    uint8_t *s1_p = (uint8_t*) s1;
    uint8_t *s2_p = (uint8_t*) s2;
    FIH_DECLARE(ret, FIH_FAILURE);

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

int boot_trailer_scramble_offset(const struct flash_area *fa, size_t alignment,
                                 size_t *off)
{
    int ret = 0;

    /* Not allowed to enforce alignment smaller than device allows */
    if (alignment < flash_area_align(fa)) {
        alignment = flash_area_align(fa);
    }

    if (device_requires_erase(fa)) {
        /* For device requiring erase align to erase unit */
        struct flash_sector sector;

        ret = flash_area_get_sector(fa, flash_area_get_size(fa) - boot_trailer_sz(alignment),
                                    &sector);
        if (ret < 0) {
            return ret;
        }

        *off = flash_sector_get_off(&sector);
    } else {
        /* For device not requiring erase align to write block */
        *off = flash_area_get_size(fa) - ALIGN_DOWN(boot_trailer_sz(alignment), alignment);
    }

    return ret;
}

int boot_header_scramble_off_sz(const struct flash_area *fa, int slot, size_t *off,
                                size_t *size)
{
    int ret = 0;
    const size_t write_block = flash_area_align(fa);
    size_t loff = 0;
    struct flash_sector sector;

    (void)slot;
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    /* In case of swap offset, header of secondary slot image is positioned
     * in second sector of slot.
     */
    if (slot == BOOT_SECONDARY_SLOT) {
        ret = flash_area_get_sector(fa, 0, &sector);
        if (ret < 0) {
            return ret;
        }
        loff = flash_sector_get_off(&sector);
    }
#endif

    if (device_requires_erase(fa)) {
        /* For device requiring erase align to erase unit */
        ret = flash_area_get_sector(fa, loff, &sector);
        if (ret < 0) {
            return ret;
        }

        *size = flash_sector_get_size(&sector);
    } else {
        /* For device not requiring erase align to write block */
        *size = ALIGN_UP(sizeof(((struct image_header *)0)->ih_magic), write_block);
    }
    *off = loff;

    return ret;
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
    uint32_t elem_sz;

    elem_sz = flash_area_align(fap);

#if MCUBOOT_SWAP_USING_SCRATCH
    if (flash_area_get_id(fap) == FLASH_AREA_IMAGE_SCRATCH) {
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
 * @returns flash_area pointer on success, NULL on failure.
 */
const struct flash_area *
boot_find_status(const struct boot_loader_state *state, int image_index)
{
    const struct flash_area *fa_p = NULL;
    const struct flash_area *areas[] = {
#if MCUBOOT_SWAP_USING_SCRATCH
        state->scratch.area,
#endif
        state->imgs[image_index][BOOT_PRIMARY_SLOT].area,
    };
    unsigned int i;

    /*
     * In the middle a swap, tries to locate the area that is currently
     * storing a valid magic, first on the primary slot, then on scratch.
     * Both "slots" can end up being temporary storage for a swap and it
     * is assumed that if magic is valid then other metadata is too,
     * because magic is always written in the last step.
     */
    for (i = 0; i < sizeof(areas) / sizeof(areas[0]); i++) {
        uint8_t magic[BOOT_MAGIC_SZ];
        int rc = 0;

        fa_p = areas[i];
        rc = flash_area_read(fa_p, boot_magic_off(fa_p), magic, BOOT_MAGIC_SZ);

        if (rc != 0) {
            BOOT_LOG_ERR("Failed to read status from %d, err %d\n",
                         flash_area_get_id(fa_p), rc);
            fa_p = NULL;
            break;
        }

        if (BOOT_MAGIC_GOOD == boot_magic_decode(magic)) {
            break;
        }
    }

    return fa_p;
}

int
boot_read_swap_size(const struct flash_area *fap, uint32_t *swap_size)
{
    uint32_t off;
    int rc;

    off = boot_swap_size_off(fap);
    rc = flash_area_read(fap, off, swap_size, sizeof *swap_size);

    return rc;
}

#ifdef MCUBOOT_ENC_IMAGES
int
boot_read_enc_key(const struct flash_area *fap, uint8_t slot, struct boot_status *bs)
{
    uint32_t off;
#if MCUBOOT_SWAP_SAVE_ENCTLV
    uint32_t i;
#endif
    int rc;

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
            rc = boot_decrypt_key(bs->enctlv[slot], bs->enckey[slot]);
        }
    }
#else
    rc = flash_area_read(fap, off, bs->enckey[slot], BOOT_ENC_KEY_ALIGN_SIZE);
#endif

    return rc;
}
#endif

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

#ifdef MCUBOOT_SWAP_USING_SCRATCH
size_t
boot_get_first_trailer_sector(struct boot_loader_state *state, size_t slot, size_t trailer_sz)
{
    size_t first_trailer_sector = boot_img_num_sectors(state, slot) - 1;
    size_t sector_sz = boot_img_sector_size(state, slot, first_trailer_sector);
    size_t trailer_sector_sz = sector_sz;

    while (trailer_sector_sz < trailer_sz) {
        /* Consider that the image trailer may span across sectors of different sizes */
        --first_trailer_sector;
        sector_sz = boot_img_sector_size(state, slot, first_trailer_sector);

        trailer_sector_sz += sector_sz;
    }

    return first_trailer_sector;
}

/**
 * Returns the offset to the end of the first sector of a given slot that holds image trailer data.
 *
 * @param state      Current bootloader's state.
 * @param slot       The index of the slot to consider.
 * @param trailer_sz The size of the trailer, in bytes.
 *
 * @return The offset to the end of the first sector of the slot that holds image trailer data.
 */
static uint32_t
get_first_trailer_sector_end_off(struct boot_loader_state *state, size_t slot, size_t trailer_sz)
{
    size_t first_trailer_sector = boot_get_first_trailer_sector(state, slot, trailer_sz);

    return boot_img_sector_off(state, slot, first_trailer_sector) +
           boot_img_sector_size(state, slot, first_trailer_sector);
}
#endif /* MCUBOOT_SWAP_USING_SCRATCH */

uint32_t bootutil_max_image_size(struct boot_loader_state *state, const struct flash_area *fap)
{
#if defined(MCUBOOT_SINGLE_APPLICATION_SLOT) ||      \
    defined(MCUBOOT_FIRMWARE_LOADER) ||              \
    defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    (void) state;
    return boot_status_off(fap);
#elif defined(MCUBOOT_SWAP_USING_SCRATCH)
    size_t slot_trailer_sz = boot_trailer_sz(BOOT_WRITE_SZ(state));
    size_t slot_trailer_off = flash_area_get_size(fap) - slot_trailer_sz;

    /* If the trailer doesn't fit in the last sector of the primary or secondary slot, some padding
     * might have to be inserted between the end of the firmware image and the beginning of the
     * trailer to ensure there is enough space for the trailer in the scratch area when the last
     * sector of the secondary will be copied to the scratch area.
     *
     * The value of the padding depends on the amount of trailer data that is contained in the first
     * trailer containing part of the trailer in the primary and secondary slot.
     */
    size_t trailer_sector_primary_end_off =
        get_first_trailer_sector_end_off(state, BOOT_PRIMARY_SLOT, slot_trailer_sz);
    size_t trailer_sector_secondary_end_off =
        get_first_trailer_sector_end_off(state, BOOT_SECONDARY_SLOT, slot_trailer_sz);

    size_t trailer_sz_in_first_sector;

    if (trailer_sector_primary_end_off > trailer_sector_secondary_end_off) {
        trailer_sz_in_first_sector = trailer_sector_primary_end_off - slot_trailer_off;
    } else {
        trailer_sz_in_first_sector = trailer_sector_secondary_end_off - slot_trailer_off;
    }

    size_t trailer_padding = 0;
    size_t scratch_trailer_sz = boot_scratch_trailer_sz(BOOT_WRITE_SZ(state));

    if (scratch_trailer_sz > trailer_sz_in_first_sector) {
        trailer_padding = scratch_trailer_sz - trailer_sz_in_first_sector;
    }

    return slot_trailer_off - trailer_padding;
#elif defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET)
    (void) state;

    struct flash_sector sector;
    /* get the last sector offset */
    int rc = flash_area_get_sector(fap, boot_status_off(fap), &sector);
    if (rc) {
        BOOT_LOG_ERR("Unable to determine flash sector of the image trailer");
        return 0; /* Returning of zero here should cause any check which uses
                   * this value to fail.
                   */
    }
    return flash_sector_get_off(&sector);
#elif defined(MCUBOOT_OVERWRITE_ONLY)
    (void) state;
    return boot_swap_info_off(fap);
#elif defined(MCUBOOT_DIRECT_XIP)
    (void) state;
    return boot_swap_info_off(fap);
#elif defined(MCUBOOT_RAM_LOAD)
    (void) state;
    return boot_swap_info_off(fap);
#endif
}

/*
 * Compute the total size of the given image.  Includes the size of
 * the TLVs.
 */
#if !defined(MCUBOOT_DIRECT_XIP) && \
    !defined(MCUBOOT_SWAP_USING_OFFSET) && \
    (!defined(MCUBOOT_OVERWRITE_ONLY) || \
    defined(MCUBOOT_OVERWRITE_ONLY_FAST))
int
boot_read_image_size(struct boot_loader_state *state, int slot, uint32_t *size)
{
    const struct flash_area *fap;
    struct image_tlv_info info;
    uint32_t off;
    uint32_t protect_tlv_size;
    int rc;

    assert(slot == BOOT_PRIMARY_SLOT || slot == BOOT_SECONDARY_SLOT);

    fap = BOOT_IMG_AREA(state, slot);
    assert(fap != NULL);

    off = BOOT_TLV_OFF(boot_img_hdr(state, slot));

    if (flash_area_read(fap, off, &info, sizeof(info))) {
        rc = BOOT_EFLASH;
        goto done;
    }

    protect_tlv_size = boot_img_hdr(state, slot)->ih_protect_tlv_size;
    if (info.it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
        if (protect_tlv_size != info.it_tlv_tot) {
            rc = BOOT_EBADIMAGE;
            goto done;
        }

        if (flash_area_read(fap, off + info.it_tlv_tot, &info, sizeof(info))) {
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
    return rc;
}
#endif /* !MCUBOOT_OVERWRITE_ONLY */

/**
 * Erases a region of device that requires erase prior to write; does
 * nothing on devices without erase.
 *
 * @param fa                    The flash_area containing the region to erase.
 * @param off                   The offset within the flash area to start the
 *                              erase.
 * @param size                  The number of bytes to erase.
 * @param backwards             If set to true will erase from end to start
 *                              addresses, otherwise erases from start to end
 *                              addresses.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_erase_region(const struct flash_area *fa, uint32_t off, uint32_t size, bool backwards)
{
    int rc = 0;

    if (off >= flash_area_get_size(fa) || (flash_area_get_size(fa) - off) < size) {
        rc = -1;
        goto end;
    } else if (device_requires_erase(fa)) {
        uint32_t end_offset = 0;
        struct flash_sector sector;

        if (backwards) {
            /* Get the lowest page offset first */
            rc = flash_area_get_sector(fa, off, &sector);

            if (rc < 0) {
                goto end;
            }

            end_offset = flash_sector_get_off(&sector);

            /* Set boundary condition, the highest probable offset to erase, within
             * last sector to erase
             */
            off += size - 1;
        } else {
            /* Get the highest page offset first */
            rc = flash_area_get_sector(fa, (off + size - 1), &sector);

            if (rc < 0) {
                goto end;
            }

            end_offset = flash_sector_get_off(&sector);
        }

        while (true) {
            /* Size to read in this iteration */
            size_t csize;

            /* Get current sector and, also, correct offset */
            rc = flash_area_get_sector(fa, off, &sector);

            if (rc < 0) {
                goto end;
            }

            /* Corrected offset and size of current sector to erase */
            off = flash_sector_get_off(&sector);
            csize = flash_sector_get_size(&sector);

            rc = flash_area_erase(fa, off, csize);

            if (rc < 0) {
                goto end;
            }

            MCUBOOT_WATCHDOG_FEED();

            if (backwards) {
                if (end_offset >= off) {
                    /* Reached the first offset in range and already erased it */
                    break;
                }

                /* Move down to previous sector, the flash_area_get_sector will
                 * correct the value to real page offset
                 */
                off -= 1;
            } else {
                /* Move up to next sector */
                off += csize;

                if (off > end_offset) {
                    /* Reached the end offset in range and already erased it */
                    break;
                }

                /* Workaround for flash_sector_get_off() being broken in mynewt, hangs with
                 * infinite loop if this is not present, should be removed if bug is fixed.
                 */
                off += 1;
            }
        }
    }

end:
    return rc;
}
