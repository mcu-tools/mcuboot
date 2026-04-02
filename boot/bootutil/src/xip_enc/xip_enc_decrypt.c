/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * XIP Encryption Library -- Default Decryption (Software AES-CTR)
 *
 * Provides a __weak boot_decrypt_xip() using bootutil AES-CTR wrappers.
 * Works with any crypto backend (tinycrypt, mbedTLS, PSA).
 *
 * Platforms with hardware crypto (e.g., SMIF) should provide a strong
 * override for better performance.
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES_XIP)

#include <stdint.h>
#include <string.h>
#include "bootutil/crypto/aes_ctr.h"
#include "flash_map_backend/flash_map_backend.h"
#include "xip_enc.h"

/*
 * Default (weak) boot_decrypt_xip -- software AES-CTR decryption.
 *
 * Retrieves key/IV from xip_enc_get_key() (populated by the validation
 * hook during ECIES unwrap). Builds AES-CTR nonce and decrypts buf in-place.
 *
 * Nonce format (edgeprotecttools-aligned):
 *   bits  0..31  = absolute XIP address (little-endian)
 *   bits 32..127 = xip_iv[0:12]
 * Counter increments by 16 per AES block (absolute address).
 *
 * Platforms with hardware XIP decryption (e.g., Infineon SMIF) should
 * provide a strong override of this function.
 */
__attribute__((weak))
int boot_decrypt_xip(int image_index, const struct flash_area *fap,
                     uint32_t off, uint32_t sz, uint8_t *buf)
{
    bootutil_aes_ctr_context aes_ctr;
    uint8_t key[XIP_ENC_KEY_SIZE];
    uint8_t iv[XIP_ENC_IV_SIZE];
    uint8_t nonce[16];
    uintptr_t flash_base;
    uint32_t abs_off;
    uint32_t pos;
    uint32_t blk_sz;
    int rc;

    if (sz == 0u) {
        return 0;
    }

    /* Retrieve key/IV stored by boot_image_check_hook */
    if (xip_enc_get_key(image_index, key, iv) != 0) {
        return -1;
    }

    /* Compute absolute XIP address for AES-CTR counter.
     * Matches edgeprotecttools / imgtool: initial_counter = base_address + off
     * where base_address = flash_device_base + fap->fa_off.
     */
    flash_base = 0;
    rc = flash_device_base(flash_area_get_device_id(fap), &flash_base);
    if (rc != 0) {
        xip_enc_zeroize(key, sizeof(key));
        return -1;
    }
    abs_off = (uint32_t)flash_base + flash_area_get_off(fap) + off;

    bootutil_aes_ctr_init(&aes_ctr);
    rc = bootutil_aes_ctr_set_key(&aes_ctr, key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        xip_enc_zeroize(key, sizeof(key));
        xip_enc_zeroize(iv, sizeof(iv));
        return -1;
    }

    /*
     * Per-block AES-CTR using SMIF/edgeprotecttools counter format:
     *   nonce = counter_LE32 || iv[0:12]
     *   counter = abs_off, increments by 16 per block
     *
     * We call bootutil_aes_ctr_encrypt once per block with a fresh nonce.
     * The library's internal counter increment is irrelevant because
     * each call processes exactly one block with a freshly built nonce.
     */
    for (pos = 0; pos < sz; pos += 16u) {
        uint32_t ctr_val = abs_off + pos;

        (void)memset(nonce, 0, sizeof(nonce));
        nonce[0] = (uint8_t)(ctr_val);
        nonce[1] = (uint8_t)(ctr_val >> 8);
        nonce[2] = (uint8_t)(ctr_val >> 16);
        nonce[3] = (uint8_t)(ctr_val >> 24);
        (void)memcpy(&nonce[4], iv, 12);

        blk_sz = sz - pos;
        if (blk_sz > 16u) {
            blk_sz = 16u;
        }

        rc = bootutil_aes_ctr_encrypt(&aes_ctr, nonce,
                                      &buf[pos], blk_sz,
                                      0, &buf[pos]);
        if (rc != 0) {
            break;
        }
    }

    bootutil_aes_ctr_drop(&aes_ctr);

    /* Zeroize all sensitive material from stack */
    xip_enc_zeroize(key, sizeof(key));
    xip_enc_zeroize(iv, sizeof(iv));
    xip_enc_zeroize(nonce, sizeof(nonce));

    return rc;
}

#endif /* MCUBOOT_ENC_IMAGES_XIP */
