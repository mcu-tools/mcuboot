/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 * Copyright (c) 2020-2021 Nordic Semiconductor ASA
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

/**
 * @file
 * @brief Public MCUBoot interface API
 *
 * This file contains API which can be combined with the application in order
 * to interact with the MCUBoot bootloader. This API are shared code-base betwen
 * MCUBoot and the application which controls DFU process.
 */

#ifndef H_BOOTUTIL_PUBLIC
#define H_BOOTUTIL_PUBLIC

#include <inttypes.h>
#include <string.h>
#include <flash_map_backend/flash_map_backend.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Attempt to boot the contents of the primary slot. */
#define BOOT_SWAP_TYPE_NONE     1

/**
 * Swap to the secondary slot.
 * Absent a confirm command, revert back on next boot.
 */
#define BOOT_SWAP_TYPE_TEST     2

/**
 * Swap to the secondary slot,
 * and permanently switch to booting its contents.
 */
#define BOOT_SWAP_TYPE_PERM     3

/** Swap back to alternate slot.  A confirm changes this state to NONE. */
#define BOOT_SWAP_TYPE_REVERT   4

/** Swap failed because image to be run is not valid */
#define BOOT_SWAP_TYPE_FAIL     5

/** Swapping encountered an unrecoverable error */
#define BOOT_SWAP_TYPE_PANIC    0xff

#define BOOT_MAX_ALIGN          8

#define BOOT_MAGIC_GOOD     1
#define BOOT_MAGIC_BAD      2
#define BOOT_MAGIC_UNSET    3
#define BOOT_MAGIC_ANY      4  /* NOTE: control only, not dependent on sector */
#define BOOT_MAGIC_NOTGOOD  5  /* NOTE: control only, not dependent on sector */

/*
 * NOTE: leave BOOT_FLAG_SET equal to one, this is written to flash!
 */
#define BOOT_FLAG_SET       1
#define BOOT_FLAG_BAD       2
#define BOOT_FLAG_UNSET     3
#define BOOT_FLAG_ANY       4  /* NOTE: control only, not dependent on sector */

#define BOOT_MAGIC_SZ (sizeof boot_img_magic)

#define BOOT_EFLASH      1
#define BOOT_EFILE       2
#define BOOT_EBADIMAGE   3
#define BOOT_EBADVECT    4
#define BOOT_EBADSTATUS  5
#define BOOT_ENOMEM      6
#define BOOT_EBADARGS    7
#define BOOT_EBADVERSION 8
#define BOOT_EFLASH_SEC  9

/*
 * Extract the swap type and image number from image trailers's swap_info
 * filed.
 */
#define BOOT_GET_SWAP_TYPE(swap_info)    ((swap_info) & 0x0F)
#define BOOT_GET_IMAGE_NUM(swap_info)    ((swap_info) >> 4)

/* Construct the swap_info field from swap type and image number */
#define BOOT_SET_SWAP_INFO(swap_info, image, type)  {                          \
                                                    assert((image) < 0xF);     \
                                                    assert((type)  < 0xF);     \
                                                    (swap_info) = (image) << 4 \
                                                                | (type);      \
                                                    }
#ifdef MCUBOOT_HAVE_ASSERT_H
#include "mcuboot_config/mcuboot_assert.h"
#else
#include <assert.h>
#define ASSERT assert
#endif

struct boot_swap_state {
    uint8_t magic;      /* One of the BOOT_MAGIC_[...] values. */
    uint8_t swap_type;  /* One of the BOOT_SWAP_TYPE_[...] values. */
    uint8_t copy_done;  /* One of the BOOT_FLAG_[...] values. */
    uint8_t image_ok;   /* One of the BOOT_FLAG_[...] values. */
    uint8_t image_num;  /* Boot status belongs to this image */
};

/**
 * @brief Determines the action, if any, that mcuboot will take on a image pair.
 *
 * @param image_index Image pair index.
 *
 * @return a BOOT_SWAP_TYPE_[...] constant on success, negative errno code on
 * fail.
 */
int boot_swap_type_multi(int image_index);

/**
 * @brief Determines the action, if any, that mcuboot will take.
 *
 * Works the same as a boot_swap_type_multi(0) call;
 *
 * @return a BOOT_SWAP_TYPE_[...] constant on success, negative errno code on
 * fail.
 */
int boot_swap_type(void);

/**
 * Marks the image with the given index in the secondary slot as pending. On the
 * next reboot, the system will perform a one-time boot of the the secondary
 * slot image.
 *
 * @param image_index       Image pair index.
 *
 * @param permanent         Whether the image should be used permanently or
 *                          only tested once:
 *                               0=run image once, then confirm or revert.
 *                               1=run image forever.
 *
 * @return                  0 on success; nonzero on failure.
 */
int boot_set_pending_multi(int image_index, int permanent);

/**
 * Marks the image with index 0 in the secondary slot as pending. On the next
 * reboot, the system will perform a one-time boot of the the secondary slot
 * image. Note that this API is kept for compatibility. The
 * boot_set_pending_multi() API is recommended.
 *
 * @param permanent         Whether the image should be used permanently or
 *                          only tested once:
 *                               0=run image once, then confirm or revert.
 *                               1=run image forever.
 *
 * @return                  0 on success; nonzero on failure.
 */
int boot_set_pending(int permanent);

/**
 * Marks the image with the given index in the primary slot as confirmed.  The
 * system will continue booting into the image in the primary slot until told to
 * boot from a different slot.
 *
 * @param image_index       Image pair index.
 *
 * @return                  0 on success; nonzero on failure.
 */
int boot_set_confirmed_multi(int image_index);

/**
 * Marks the image with index 0 in the primary slot as confirmed.  The system
 * will continue booting into the image in the primary slot until told to boot
 * from a different slot.  Note that this API is kept for compatibility. The
 * boot_set_confirmed_multi() API is recommended.
 *
 * @return                  0 on success; nonzero on failure.
 */
int boot_set_confirmed(void);

/**
 * @brief Get offset of the swap info field in the image trailer.
 *
 * @param fap Flash are for which offset is determined.
 *
 * @retval offset of the swap info field.
 */
uint32_t boot_swap_info_off(const struct flash_area *fap);

/**
 * @brief Get value of image-ok flag of the image.
 *
 * If called from chin-loaded image the image-ok flag value can be used to check
 * whether application itself is already confirmed.
 *
 * @param fap Flash area of the image.
 * @param image_ok[out] image-ok value.
 *
 * @return 0 on success; nonzero on failure.
 */
int boot_read_image_ok(const struct flash_area *fap, uint8_t *image_ok);

/**
 * @brief Read the image swap state
 *
 * @param flash_area_id id of flash partition from which state will be read;
 * @param state pointer to structure for storing swap state.
 *
 * @return 0 on success; non-zero error code on failure;
 */
int
boot_read_swap_state_by_id(int flash_area_id, struct boot_swap_state *state);

/**
 * @brief Read the image swap state
 *
 * @param fa pointer to flash_area object;
 * @param state pointer to structure for storing swap state.
 *
 * @return 0 on success; non-zero error code on failure.
 */
int
boot_read_swap_state(const struct flash_area *fa,
                     struct boot_swap_state *state);

#define BOOT_MAGIC_ARR_SZ \
    (sizeof boot_img_magic / sizeof boot_img_magic[0])

extern const uint32_t boot_img_magic[4];

#ifdef __cplusplus
}
#endif

#endif
