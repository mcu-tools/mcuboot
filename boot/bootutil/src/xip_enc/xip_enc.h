/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * XIP Multi-Key Encryption Library
 * Self-contained library for XIP encryption support in MCUBoot.
 * Interfaces with MCUBoot via boot_image_check_hook (existing hook).
 */
#ifndef XIP_ENC_H
#define XIP_ENC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES) && defined(MCUBOOT_ENC_IMAGES_XIP)
#error "MCUBOOT_ENC_IMAGES and MCUBOOT_ENC_IMAGES_XIP are mutually exclusive."
#endif

/**
 * Secure zeroization -- uses volatile pointer to prevent compiler optimization.
 */
static inline void xip_enc_zeroize(void *p, size_t n)
{
    volatile unsigned char *v = (volatile unsigned char *)p;
    for (size_t i = 0; i < n; i++) {
        v[i] = 0;
    }
}

/**
 * Constant-time comparison to avoid timing side-channels.
 * Returns 0 if equal, non-zero otherwise.
 */
static inline int xip_enc_ct_compare(const uint8_t *a, const uint8_t *b,
                                     size_t size)
{
    uint8_t result = 0;
    for (size_t i = 0; i < size; i++) {
        result |= a[i] ^ b[i];
    }
    return (int)result;
}

#define XIP_ENC_KEY_SIZE   16u
#define XIP_ENC_IV_SIZE    16u
#define XIP_ENC_MAX_IMAGES 3u

/*
 * boot_image_check_hook() is declared in bootutil/boot_hooks.h (upstream).
 * The library provides the implementation in xip_enc_validate.c.
 * Do NOT redeclare here.
 */

/**
 * Called by MCUBoot after fill_rsp() to populate xip_key/xip_iv in boot_rsp.
 * Copies key/IV from library-internal storage for the specified image.
 *
 * @param img_index  Image index (passed via BOOT_CURR_IMG from MCUBoot)
 * @param rsp        Boot response to populate
 */
void boot_xip_populate_rsp(int img_index, struct boot_rsp *rsp);

/**
 * Platform-provided: decrypt image payload using SMIF hardware.
 * Used during hash computation to decrypt AES-CTR encrypted payload.
 *
 * @param image_index  Current image index
 * @param fap          Flash area of the image
 * @param off          Offset within flash area
 * @param sz           Size of data to decrypt
 * @param buf          Buffer with data to decrypt (in-place)
 *
 * @return 0 on success, negative on error
 */
int boot_decrypt_xip(int image_index, const struct flash_area *fap,
                     uint32_t off, uint32_t sz, uint8_t *buf);

/**
 * Store key/IV for an image (called by validation hook after ECIES unwrap).
 */
void xip_enc_store_key(int img_index, const uint8_t *key, const uint8_t *iv);

/**
 * Retrieve stored key/IV for an image.
 *
 * @return 0 on success, -1 if not valid
 */
int xip_enc_get_key(int img_index, uint8_t *key, uint8_t *iv);

/**
 * Zeroize all stored keys. Call before launching application.
 */
void xip_enc_clear_keys(void);

/**
 * ECIES-P256 key unwrap with extended TLV support.
 * Extracts AES key and XIP IV from ECIES envelope.
 *
 * @param tlv_buf   ECIES TLV data read from image
 * @param tlv_len   Length of TLV data (113 standard, up to 177 extended)
 * @param out_key   Output: 16-byte AES key
 * @param out_iv    Output: 16-byte XIP IV
 *
 * @return 0 on success, negative on error
 */
int xip_enc_ecies_unwrap(const uint8_t *tlv_buf, uint16_t tlv_len,
                         uint8_t *out_key, uint8_t *out_iv);

#endif /* XIP_ENC_H */
