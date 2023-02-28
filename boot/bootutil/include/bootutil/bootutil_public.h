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

#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <flash_map_backend/flash_map_backend.h>
#include <mcuboot_config/mcuboot_config.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(num, align)    (((num) + ((align) - 1U)) & ~((align) - 1U))
#endif

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(num, align)  ((num) & ~((align) - 1U))
#endif

#ifndef PTR_CAST
#define PTR_CAST(type, source)          \
    ({                                  \
        union                           \
        {                               \
            const void* void_p;         \
            type* type_p;               \
        } res_p;                        \
        res_p.void_p = (source);        \
        (res_p.type_p);                 \
    })
#endif

typedef enum
{
    /** Attempt to boot the contents of the primary slot. */
    BOOT_SWAP_TYPE_NONE = 0x01U,

    /**
     * Swap to the secondary slot.
     * Absent a confirm command, revert back on next boot.
     */
    BOOT_SWAP_TYPE_TEST = 0x02U,

    /**
     * Swap to the secondary slot,
     * and permanently switch to booting its contents.
     */
    BOOT_SWAP_TYPE_PERM = 0x03U,

    /** Swap back to alternate slot.  A confirm changes this state to NONE. */
    BOOT_SWAP_TYPE_REVERT = 0x04U,

    /** Swap failed because image to be run is not valid */
    BOOT_SWAP_TYPE_FAIL = 0x05U,

    /** Swapping encountered an unrecoverable error */
    BOOT_SWAP_TYPE_PANIC = 0xFFU
} boot_swap_type_t;

#define IS_VALID_SWAP_TYPE(swap_type)                            \
    (((uint8_t)(swap_type) == (uint8_t)BOOT_SWAP_TYPE_NONE)   || \
     ((uint8_t)(swap_type) == (uint8_t)BOOT_SWAP_TYPE_TEST)   || \
     ((uint8_t)(swap_type) == (uint8_t)BOOT_SWAP_TYPE_PERM)   || \
     ((uint8_t)(swap_type) == (uint8_t)BOOT_SWAP_TYPE_REVERT) || \
     ((uint8_t)(swap_type) == (uint8_t)BOOT_SWAP_TYPE_FAIL))

#define BOOT_MAGIC_SZ           16U

#ifdef MCUBOOT_BOOT_MAX_ALIGN

_Static_assert(MCUBOOT_BOOT_MAX_ALIGN >= 8 && MCUBOOT_BOOT_MAX_ALIGN <= 32,
               "Unsupported value for MCUBOOT_BOOT_MAX_ALIGN");

#define BOOT_MAX_ALIGN          MCUBOOT_BOOT_MAX_ALIGN
#define BOOT_MAGIC_ALIGN_SIZE   ALIGN_UP(BOOT_MAGIC_SZ, BOOT_MAX_ALIGN)
#else
#define BOOT_MAX_ALIGN          8U
#define BOOT_MAGIC_ALIGN_SIZE   BOOT_MAGIC_SZ
#endif

typedef enum
{
    BOOT_MAGIC_GOOD = 0x01U,
    BOOT_MAGIC_BAD = 0x02U,
    BOOT_MAGIC_UNSET = 0x03U,
    BOOT_MAGIC_ANY = 0x04U,     /* NOTE: control only, not dependent on sector */
    BOOT_MAGIC_NOTGOOD = 0x05U, /* NOTE: control only, not dependent on sector */
} boot_magic_t;

/*
 * NOTE: leave BOOT_FLAG_SET equal to one, this is written to flash!
 */
typedef enum
{
    BOOT_FLAG_SET = 0x01U,
    BOOT_FLAG_BAD = 0x02U,
    BOOT_FLAG_UNSET = 0x03U,
    BOOT_FLAG_ANY = 0x04U,
} boot_flag_t;

#define BOOT_EFLASH      1
#define BOOT_EFILE       2
#define BOOT_EBADIMAGE   3
#define BOOT_EBADVECT    4
#define BOOT_EBADSTATUS  5
#define BOOT_ENOMEM      6
#define BOOT_EBADARGS    7
#define BOOT_EBADVERSION 8
#define BOOT_EFLASH_SEC  9

#define BOOT_HOOK_REGULAR 1
/*
 * Extract the swap type and image number from image trailers's swap_info
 * filed.
 */
#define BOOT_GET_SWAP_TYPE(swap_info)    ((swap_info) & 0x0F)
#define BOOT_GET_IMAGE_NUM(swap_info) ((swap_info) >> 4)

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
#ifndef ASSERT
#define ASSERT assert
#endif
#endif

struct boot_swap_state
{
    boot_magic_t magic;         /* One of the BOOT_MAGIC_[...] values. */
    boot_swap_type_t swap_type; /* One of the BOOT_SWAP_TYPE_[...] values. */
    boot_flag_t copy_done;      /* One of the BOOT_FLAG_[...] values. */
    boot_flag_t image_ok;       /* One of the BOOT_FLAG_[...] values. */
    uint8_t image_num;      /* Boot status belongs to this image */
};

/**
 * @brief Determines the action, if any, that mcuboot will take on a image pair.
 *
 * @param image_index Image pair index.
 *
 * @return a BOOT_SWAP_TYPE_[...] constant on success, negative errno code on
 * fail.
 */
boot_swap_type_t boot_swap_type_multi(int image_index);

/**
 * @brief Determines the action, if any, that mcuboot will take.
 *
 * Works the same as a boot_swap_type_multi(0) call;
 *
 * @return a BOOT_SWAP_TYPE_[...] constant on success, negative errno code on
 * fail.
 */
boot_swap_type_t boot_swap_type(void);

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
int boot_set_pending_multi(int image_index, bool permanent);

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
int boot_set_pending(bool permanent);

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
int boot_read_image_ok(const struct flash_area *fap, boot_flag_t *image_ok);

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

#ifdef __cplusplus
}
#endif

#endif
