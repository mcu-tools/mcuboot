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
#if defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) || \
    defined(MCUBOOT_SWAP_USING_SCRATCH)
#include "swap_priv.h"
#endif

#if defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO) || defined(MCUBOOT_SWAP_USING_SCRATCH)
#include "swap_priv.h"
#endif

BOOT_LOG_MODULE_DECLARE(mcuboot);

/* Currently only used by imgmgr */
int boot_current_slot;

#if (!defined(MCUBOOT_DIRECT_XIP) && !defined(MCUBOOT_RAM_LOAD)) || \
defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO)
/* Used for holding static buffers in multiple functions to work around issues
 * in older versions of gcc (e.g. 4.8.4)
 */
static struct boot_sector_buffer sector_buffers;
#endif

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
#if defined(MCUBOOT_SINGLE_APPLICATION_SLOT) ||      \
    defined(MCUBOOT_FIRMWARE_LOADER) ||              \
    defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    /* Single image MCUboot modes do not have a swap status fields */
    return 0;
#else
    return BOOT_STATUS_STATE_COUNT * min_write_sz;
#endif
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

    BOOT_LOG_DBG("boot_trailer_scramble_offset: flash_area %p, alignment %u",
                 fa, (unsigned int)alignment);

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

    BOOT_LOG_DBG("boot_trailer_scramble_offset: final alignment %u, offset %u",
                 (unsigned int)alignment, (unsigned int)*off);

    return ret;
}

int boot_header_scramble_off_sz(const struct flash_area *fa, int slot, size_t *off,
                                size_t *size)
{
    int ret = 0;
    const size_t write_block = flash_area_align(fa);
    size_t loff = 0;
    struct flash_sector sector;

    BOOT_LOG_DBG("boot_header_scramble_off_sz: slot %d", slot);

    (void)slot;
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    /* In case of swap offset, header of secondary slot image is positioned
     * in second sector of slot.
     */
    if (slot == BOOT_SLOT_SECONDARY) {
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

    BOOT_LOG_DBG("boot_header_scramble_off_sz: size %u", (unsigned int)*size);

    return ret;
}

#if MCUBOOT_SWAP_USING_SCRATCH
/*
 * Similar to `boot_trailer_sz` but this function returns the space used to
 * store status in the scratch partition. The scratch partition only stores
 * status during the swap of the last sector from primary/secondary (which
 * is the first swap operation) and thus only requires space for one swap.
 */
uint32_t boot_scratch_trailer_sz(uint32_t min_write_sz)
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
        state->imgs[image_index][BOOT_SLOT_PRIMARY].area,
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
boot_write_enc_keys(const struct flash_area *fap, const struct boot_status *bs)
{
    int slot;

    for (slot = 0; slot < BOOT_NUM_SLOTS; ++slot) {
        uint32_t off = boot_enc_key_off(fap, slot);
        int rc;

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
    }

    return 0;
}
#endif

uint32_t bootutil_max_image_size(struct boot_loader_state *state, const struct flash_area *fap)
{
#if defined(MCUBOOT_SINGLE_APPLICATION_SLOT) ||      \
    defined(MCUBOOT_FIRMWARE_LOADER) ||              \
    defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    (void) state;
    return boot_status_off(fap);
#elif defined(MCUBOOT_SWAP_USING_MOVE) || defined(MCUBOOT_SWAP_USING_OFFSET) \
      || defined(MCUBOOT_SWAP_USING_SCRATCH)
    (void) fap;
    return app_max_size(state);
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

    assert(slot == BOOT_SLOT_PRIMARY || slot == BOOT_SLOT_SECONDARY);

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

    BOOT_LOG_DBG("boot_erase_region: flash_area %p, offset %d, size %d, backwards == %d",
                 fa, off, size, (int)backwards);

    if (off >= flash_area_get_size(fa) || (flash_area_get_size(fa) - off) < size) {
        rc = -1;
        goto end;
    } else if (device_requires_erase(fa)) {
        uint32_t end_offset = 0;
        struct flash_sector sector;

        BOOT_LOG_DBG("boot_erase_region: device with erase");

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
    } else {
        BOOT_LOG_DBG("boot_erase_region: device without erase");
    }

end:
    return rc;
}

#if (!defined(MCUBOOT_DIRECT_XIP) && !defined(MCUBOOT_RAM_LOAD)) || \
defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO)
int
boot_initialize_area(struct boot_loader_state *state, int flash_area)
{
    uint32_t num_sectors = BOOT_MAX_IMG_SECTORS;
    boot_sector_t *out_sectors;
    uint32_t *out_num_sectors;
    int rc;

    num_sectors = BOOT_MAX_IMG_SECTORS;

    if (flash_area == FLASH_AREA_IMAGE_PRIMARY(BOOT_CURR_IMG(state))) {
        out_sectors = BOOT_IMG(state, BOOT_SLOT_PRIMARY).sectors;
        out_num_sectors = &BOOT_IMG(state, BOOT_SLOT_PRIMARY).num_sectors;
#if BOOT_NUM_SLOTS > 1
    } else if (flash_area == FLASH_AREA_IMAGE_SECONDARY(BOOT_CURR_IMG(state))) {
        out_sectors = BOOT_IMG(state, BOOT_SLOT_SECONDARY).sectors;
        out_num_sectors = &BOOT_IMG(state, BOOT_SLOT_SECONDARY).num_sectors;
#if MCUBOOT_SWAP_USING_SCRATCH
    } else if (flash_area == FLASH_AREA_IMAGE_SCRATCH) {
        out_sectors = state->scratch.sectors;
        out_num_sectors = &state->scratch.num_sectors;
#endif
#endif
    } else {
        return BOOT_EFLASH;
    }

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
    rc = flash_area_get_sectors(flash_area, &num_sectors, out_sectors);
#else
    _Static_assert(sizeof(int) <= sizeof(uint32_t), "Fix needed");
    rc = flash_area_to_sectors(flash_area, (int *)&num_sectors, out_sectors);
#endif /* defined(MCUBOOT_USE_FLASH_AREA_GET_SECTORS) */
    if (rc != 0) {
        return rc;
    }
    *out_num_sectors = num_sectors;
    return 0;
}

static uint32_t
boot_write_sz(struct boot_loader_state *state)
{
    uint32_t elem_sz;
#if MCUBOOT_SWAP_USING_SCRATCH
    uint32_t align;
#endif

    /* Figure out what size to write update status update as.  The size depends
     * on what the minimum write size is for scratch area, active image slot.
     * We need to use the bigger of those 2 values.
     */
    elem_sz = flash_area_align(BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY));
#if MCUBOOT_SWAP_USING_SCRATCH
    align = flash_area_align(BOOT_SCRATCH_AREA(state));
    if (align > elem_sz) {
        elem_sz = align;
    }
#endif

    return elem_sz;
}

int
boot_read_sectors(struct boot_loader_state *state, struct boot_sector_buffer *sectors)
{
    uint8_t image_index;
    int rc;

    if (sectors == NULL) {
        sectors = &sector_buffers;
    }

    image_index = BOOT_CURR_IMG(state);

    BOOT_IMG(state, BOOT_SLOT_PRIMARY).sectors =
        sectors->primary[image_index];
#if BOOT_NUM_SLOTS > 1
    BOOT_IMG(state, BOOT_SLOT_SECONDARY).sectors =
        sectors->secondary[image_index];
#if MCUBOOT_SWAP_USING_SCRATCH
    state->scratch.sectors = sectors->scratch;
#endif
#endif

    rc = boot_initialize_area(state, FLASH_AREA_IMAGE_PRIMARY(image_index));
    if (rc != 0) {
        return BOOT_EFLASH;
    }

#if BOOT_NUM_SLOTS > 1
    rc = boot_initialize_area(state, FLASH_AREA_IMAGE_SECONDARY(image_index));
    if (rc != 0) {
        /* We need to differentiate from the primary image issue */
        return BOOT_EFLASH_SEC;
    }

#if MCUBOOT_SWAP_USING_SCRATCH
    rc = boot_initialize_area(state, FLASH_AREA_IMAGE_SCRATCH);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
#endif
#endif

    BOOT_WRITE_SZ(state) = boot_write_sz(state);

    return 0;
}
#endif

#if defined(MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO)
static int
boot_read_sectors_recovery(struct boot_loader_state *state)
{
    uint8_t image_index;
    int rc;

    image_index = BOOT_CURR_IMG(state);

    rc = boot_initialize_area(state, FLASH_AREA_IMAGE_PRIMARY(image_index));
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_initialize_area(state, FLASH_AREA_IMAGE_SECONDARY(image_index));
    if (rc != 0) {
        /* We need to differentiate from the primary image issue */
        return BOOT_EFLASH_SEC;
    }

    return 0;
}

/**
 * Reads image data to find out the maximum application sizes. Only needs to
 * be called in serial recovery mode, as the state information is unpopulated
 * at that time
 */
void boot_fetch_slot_state_sizes(void)
{
    int rc = -1;
    int image_index;
    struct boot_loader_state *state = boot_get_loader_state();

    rc = boot_open_all_flash_areas(state);
    if (rc != 0) {
        BOOT_LOG_DBG("boot_fetch_slot_state_sizes: error %d while opening flash areas", rc);
        goto finish;
    }

    IMAGES_ITER(BOOT_CURR_IMG(state)) {
        int max_size = 0;

        image_index = BOOT_CURR_IMG(state);

        BOOT_IMG(state, BOOT_SLOT_PRIMARY).sectors = sector_buffers.primary[image_index];
#if BOOT_NUM_SLOTS > 1
        BOOT_IMG(state, BOOT_SLOT_SECONDARY).sectors = sector_buffers.secondary[image_index];
#if MCUBOOT_SWAP_USING_SCRATCH
        state->scratch.sectors = sector_buffers.scratch;
#endif
#endif

        /* Determine the sector layout of the image slots and scratch area. */
        rc = boot_read_sectors_recovery(state);

        if (rc == 0) {
            max_size = bootutil_max_image_size(state, BOOT_IMG_AREA(state, 0));

            if (max_size > 0) {
                boot_get_image_max_sizes()[image_index].calculated = true;
                boot_get_image_max_sizes()[image_index].max_size = max_size;
            }
        }
    }

finish:
    boot_close_all_flash_areas(state);
    memset(state, 0, sizeof(struct boot_loader_state));
}
#endif

/**
 * Clears the boot state, so that previous operations have no effect on new
 * ones.
 *
 * @param state                 The state that should be cleared. If the value
 *                              is NULL, the default bootloader state will be
 *                              cleared.
 */
void boot_state_clear(struct boot_loader_state *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(struct boot_loader_state));
    } else {
        memset(boot_get_loader_state(), 0, sizeof(struct boot_loader_state));
    }
}
