/*
 *  Copyright (c) 2025 Nordic Semiconductor ASA
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_MANIFEST_H__
#define __MCUBOOT_MANIFEST_H__

/**
 * @file mcuboot_manifest.h
 *
 * @note This file is only used when MCUBOOT_MANIFEST_UPDATES is enabled.
 */

#include <stdint.h>
#include "bootutil/bootutil.h"
#include "bootutil/crypto/sha.h"

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Manifest structure for image updates. */
struct mcuboot_manifest {
        uint32_t format;
        uint32_t image_count;
        /* Skip a digest of the MCUBOOT_MANIFEST_IMAGE_NUMBER image. */
        uint8_t image_hash[BOOT_IMAGE_NUMBER - 1][IMAGE_HASH_SIZE];
} __packed;

/**
 * @brief Check if the specified manifest has the correct format.
 *
 * @param[in] manifest  The reference to the manifest structure.
 *
 * @return true on success.
 */
static inline bool bootutil_verify_manifest(const struct mcuboot_manifest *manifest)
{
    if (manifest == NULL) {
        return false;
    }

    /* Currently only the simplest manifest format is supported */
    if (manifest->format != 0x1) {
        return false;
    }

    if (manifest->image_count != BOOT_IMAGE_NUMBER - 1) {
        return false;
    }

    return true;
}

/**
 * @brief Get the image hash from the manifest.
 *
 * @param[in] manifest     The reference to the manifest structure.
 * @param[in] image_index  The index of the image to get the hash for.
 *                         Must be in range <0, BOOT_IMAGE_NUMBER - 1>, but
 *                         must not be equal to MCUBOOT_MANIFEST_IMAGE_NUMBER.
 *
 * @return A pointer to the image hash, or NULL if the image_index is out of range
 *         of allowed values.
 */
static inline const uint8_t *bootutil_get_image_hash(const struct mcuboot_manifest *manifest,
                                                     uint32_t image_index)
{
    if (!bootutil_verify_manifest(manifest)) {
        return NULL;
    }

    if (image_index >= BOOT_IMAGE_NUMBER) {
        return NULL;
    }

    if (image_index < MCUBOOT_MANIFEST_IMAGE_NUMBER) {
        return manifest->image_hash[image_index];
    } else if (image_index > MCUBOOT_MANIFEST_IMAGE_NUMBER) {
        return manifest->image_hash[image_index - 1];
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* __MCUBOOT_MANIFEST_H__ */
