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

#ifndef H_BOOTUTIL_PRIV_
#define H_BOOTUTIL_PRIV_

#include "sysflash/sysflash.h"

#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __BOOTSIM__
#include "bootsim.h"
#else
#define ASSERT assert
#endif

struct flash_area;

#define BOOT_EFLASH     1
#define BOOT_EFILE      2
#define BOOT_EBADIMAGE  3
#define BOOT_EBADVECT   4
#define BOOT_EBADSTATUS 5
#define BOOT_ENOMEM     6
#define BOOT_EBADARGS   7

#define BOOT_TMPBUF_SZ  256

/*
 * Maintain state of copy progress.
 */
struct boot_status {
    uint32_t idx;         /* Which area we're operating on */
    uint8_t state;        /* Which part of the swapping process are we at */
    uint8_t use_scratch;  /* Are status bytes ever written to scratch? */
    uint32_t swap_size;   /* Total size of swapped image */
};

#define BOOT_MAGIC_GOOD  1
#define BOOT_MAGIC_BAD   2
#define BOOT_MAGIC_UNSET 3

/**
 * End-of-image slot structure.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ~                                                               ~
 * ~                Swap status (variable, aligned)                ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Swap size                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ~                  0xff padding (MAX ALIGN - 4)                 ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Copy done   |          0xff padding (MAX ALIGN - 1)         ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Image OK    |          0xff padding (MAX ALIGN - 1)         ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ~                        MAGIC (16 octets)                      ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

extern const uint32_t boot_img_magic[4];

struct boot_swap_state {
    uint8_t magic;  /* One of the BOOT_MAGIC_[...] values. */
    uint8_t copy_done;
    uint8_t image_ok;
};

#if defined(__BOOTSIM__)
#define BOOT_MAX_IMG_SECTORS       128
#elif defined(__ZEPHYR__)
#define BOOT_MAX_IMG_SECTORS       CONFIG_BOOT_MAX_IMG_SECTORS
#elif defined(MCUBOOT_MYNEWT)
#define BOOT_MAX_IMG_SECTORS       MYNEWT_VAL(BOOTUTIL_MAX_IMG_SECTORS)
#else
#error "Invalid target OS"
#endif

/*
 * The current flashmap API does not check the amount of space allocated when
 * loading sector data from the flash device, allowing for smaller counts here
 * would most surely incur in overruns.
 *
 * TODO: make flashmap API receive the current sector array size.
 */
#if BOOT_MAX_IMG_SECTORS < 32
#error "Too few sectors, please increase BOOT_MAX_IMG_SECTORS to at least 32"
#endif

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS             2

/** Maximum number of image sectors supported by the bootloader. */
#define BOOT_STATUS_STATE_COUNT    3
#define BOOT_STATUS_MAX_ENTRIES    BOOT_MAX_IMG_SECTORS

#define BOOT_STATUS_SOURCE_NONE    0
#define BOOT_STATUS_SOURCE_SCRATCH 1
#define BOOT_STATUS_SOURCE_SLOT0   2

#define BOOT_FLAG_IMAGE_OK         0
#define BOOT_FLAG_COPY_DONE        1

#define BOOT_FLAG_SET              0x01
#define BOOT_FLAG_UNSET            0xff

extern const uint32_t BOOT_MAGIC_SZ;

/**
 * Compatibility shim for flash sector type.
 *
 * This can be deleted when flash_area_to_sectors() is removed.
 */
#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
typedef struct flash_sector boot_sector_t;
#else
typedef struct flash_area boot_sector_t;
#endif

/** Private state maintained during boot. */
struct boot_loader_state {
    struct {
        struct image_header hdr;
        const struct flash_area *area;
        boot_sector_t *sectors;
        size_t num_sectors;
    } imgs[BOOT_NUM_SLOTS];

    const struct flash_area *scratch_area;

    uint8_t write_sz;
};

int bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig,
                        size_t slen, uint8_t key_id);

uint32_t boot_slots_trailer_sz(uint8_t min_write_sz);
int boot_status_entries(const struct flash_area *fap);
uint32_t boot_status_off(const struct flash_area *fap);
int boot_read_swap_state(const struct flash_area *fap,
                         struct boot_swap_state *state);
int boot_read_swap_state_by_id(int flash_area_id,
                               struct boot_swap_state *state);
int boot_write_magic(const struct flash_area *fap);
int boot_write_status(struct boot_status *bs);
int boot_schedule_test_swap(void);
int boot_write_copy_done(const struct flash_area *fap);
int boot_write_image_ok(const struct flash_area *fap);
int boot_write_swap_size(const struct flash_area *fap, uint32_t swap_size);
int boot_read_swap_size(uint32_t *swap_size);

/*
 * Accessors for the contents of struct boot_loader_state.
 */

/* These are macros so they can be used as lvalues. */
#define BOOT_IMG_AREA(state, slot) ((state)->imgs[(slot)].area)
#define BOOT_SCRATCH_AREA(state) ((state)->scratch_area)
#define BOOT_WRITE_SZ(state) ((state)->write_sz)

static inline struct image_header*
boot_img_hdr(struct boot_loader_state *state, size_t slot)
{
    return &state->imgs[slot].hdr;
}

static inline uint8_t
boot_img_fa_device_id(struct boot_loader_state *state, size_t slot)
{
    return state->imgs[slot].area->fa_device_id;
}

static inline uint8_t
boot_scratch_fa_device_id(struct boot_loader_state *state)
{
    return state->scratch_area->fa_device_id;
}

static inline size_t
boot_img_num_sectors(struct boot_loader_state *state, size_t slot)
{
    return state->imgs[slot].num_sectors;
}

/*
 * Offset of the slot from the beginning of the flash device.
 */
static inline uint32_t
boot_img_slot_off(struct boot_loader_state *state, size_t slot)
{
    return state->imgs[slot].area->fa_off;
}

static inline size_t boot_scratch_area_size(struct boot_loader_state *state)
{
    return state->scratch_area->fa_size;
}

#ifndef MCUBOOT_USE_FLASH_AREA_GET_SECTORS

static inline size_t
boot_img_sector_size(struct boot_loader_state *state,
                     size_t slot, size_t sector)
{
    return state->imgs[slot].sectors[sector].fa_size;
}

/*
 * Offset of the sector from the beginning of the image, NOT the flash
 * device.
 */
static inline uint32_t
boot_img_sector_off(struct boot_loader_state *state, size_t slot,
                    size_t sector)
{
    return state->imgs[slot].sectors[sector].fa_off -
           state->imgs[slot].sectors[0].fa_off;
}

static inline int
boot_initialize_area(struct boot_loader_state *state, int flash_area)
{
    int num_sectors = BOOT_MAX_IMG_SECTORS;
    size_t slot;
    int rc;

    switch (flash_area) {
    case FLASH_AREA_IMAGE_0:
        slot = 0;
        break;
    case FLASH_AREA_IMAGE_1:
        slot = 1;
        break;
    default:
        return BOOT_EFLASH;
    }

    rc = flash_area_to_sectors(flash_area, &num_sectors,
                               state->imgs[slot].sectors);
    if (rc != 0) {
        return rc;
    }
    state->imgs[slot].num_sectors = (size_t)num_sectors;
    return 0;
}

#else  /* defined(MCUBOOT_USE_FLASH_AREA_GET_SECTORS) */

static inline size_t
boot_img_sector_size(struct boot_loader_state *state,
                     size_t slot, size_t sector)
{
    return state->imgs[slot].sectors[sector].fs_size;
}

static inline uint32_t
boot_img_sector_off(struct boot_loader_state *state, size_t slot,
                    size_t sector)
{
    return state->imgs[slot].sectors[sector].fs_off -
           state->imgs[slot].sectors[0].fs_off;
}

static inline int
boot_initialize_area(struct boot_loader_state *state, int flash_area)
{
    uint32_t num_sectors;
    struct flash_sector *out_sectors;
    size_t *out_num_sectors;
    int rc;

    switch (flash_area) {
    case FLASH_AREA_IMAGE_0:
        num_sectors = BOOT_MAX_IMG_SECTORS;
        out_sectors = state->imgs[0].sectors;
        out_num_sectors = &state->imgs[0].num_sectors;
        break;
    case FLASH_AREA_IMAGE_1:
        num_sectors = BOOT_MAX_IMG_SECTORS;
        out_sectors = state->imgs[1].sectors;
        out_num_sectors = &state->imgs[1].num_sectors;
        break;
    default:
        return -1;
    }

    rc = flash_area_get_sectors(flash_area, &num_sectors, out_sectors);
    if (rc != 0) {
        return rc;
    }
    *out_num_sectors = num_sectors;
    return 0;
}

#endif  /* !defined(MCUBOOT_USE_FLASH_AREA_GET_SECTORS) */

#ifdef __cplusplus
}
#endif

#endif
