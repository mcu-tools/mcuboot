/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * Copyright (c) 2026 Infineon Technologies AG
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

#if defined(MCUBOOT_SWAP_USING_OFFSET)
static inline uint32_t
boot_unprotected_tlv_sizes_off(const struct flash_area *fap)
{
    return boot_swap_info_off(fap) - BOOT_MAX_ALIGN;
}

static inline uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return boot_unprotected_tlv_sizes_off(fap) - BOOT_MAX_ALIGN;
}
#else
static inline uint32_t
boot_swap_size_off(const struct flash_area *fap)
{
    return boot_swap_info_off(fap) - BOOT_MAX_ALIGN;
}
#endif

#ifdef MCUBOOT_SWAP_FINGERPRINT
/* Fingerprint partition layout — fixed offsets from partition base (0).
 *   [primary_table: N*32] [secondary_table: N*32]
 *   [table_checksum: 32]  [step_count: ALIGN]
 *   [MAGIC: 16]
 */
static inline uint32_t
boot_fingerprint_pri_table_off(void)
{
    return 0;
}

static inline uint32_t
boot_fingerprint_sec_table_off(void)
{
    return BOOT_FINGERPRINT_MAX_ENTRIES * FINGERPRINT_HASH_SZ;
}

static inline uint32_t
boot_fingerprint_table_checksum_off(void)
{
    return BOOT_FINGERPRINT_MAX_ENTRIES * FINGERPRINT_HASH_SZ * 2;
}

static inline uint32_t
boot_fingerprint_step_count_off(void)
{
    return boot_fingerprint_table_checksum_off() +
           FINGERPRINT_TABLE_CHECKSUM_SZ;
}

static inline uint32_t
boot_fingerprint_magic_off(void)
{
    return boot_fingerprint_step_count_off() + BOOT_MAX_ALIGN;
}
#endif /* MCUBOOT_SWAP_FINGERPRINT */

#endif /* H_BOOTUTIL_MISC_ */
