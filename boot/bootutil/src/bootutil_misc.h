/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 */
#ifndef H_BOOTUTIL_MISC_
#define H_BOOTUTIL_MISC_

#include <string.h>
#include <inttypes.h>
#include <stddef.h>

#include "bootutil/bootutil_public.h"
#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif

static boot_magic_t
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

#endif /* H_BOOTUTIL_MISC_ */
