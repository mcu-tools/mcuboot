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

#include "flash_map/flash_map.h"
#include "bootutil/image.h"

#ifdef __cplusplus
extern "C" {
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

#define BOOT_MAX_ALIGN  8

/*
 * Maintain state of copy progress.
 */
struct boot_status {
    uint32_t idx;         /* Which area we're operating on */
    uint8_t state;        /* Which part of the swapping process are we at */
    uint8_t use_scratch;  /* Are status bytes ever written to scratch? */
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
 * ~                        MAGIC (16 octets)                      ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ~                                                               ~
 * ~                Swap status (variable, aligned)                ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Copy done   |     0xff padding (up to min-write-sz - 1)     ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Image OK    |     0xff padding (up to min-write-sz - 1)     ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

extern const uint32_t boot_img_magic[4];

struct boot_swap_state {
    uint8_t magic;  /* One of the BOOT_MAGIC_[...] values. */
    uint8_t copy_done;
    uint8_t image_ok;
};

#define BOOT_STATUS_STATE_COUNT 3
#define BOOT_STATUS_MAX_ENTRIES 128

#define BOOT_STATUS_SOURCE_NONE    0
#define BOOT_STATUS_SOURCE_SCRATCH 1
#define BOOT_STATUS_SOURCE_SLOT0   2

#define BOOT_FLAG_IMAGE_OK         0
#define BOOT_FLAG_COPY_DONE        1

#define BOOT_FLAG_SET              0x01
#define BOOT_FLAG_UNSET            0xff

extern const uint32_t BOOT_MAGIC_SZ;

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS              2

/** Private state maintained during boot. */
struct boot_loader_state {
    struct {
        struct image_header hdr;
        struct flash_area *sectors;
        int num_sectors;
    } imgs[BOOT_NUM_SLOTS];

    struct flash_area scratch_sector;

    uint8_t write_sz;
};

int bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, int slen,
    uint8_t key_id);

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

#ifdef __cplusplus
}
#endif

#endif

