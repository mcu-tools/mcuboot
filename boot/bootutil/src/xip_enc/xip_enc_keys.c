/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * XIP Encryption Library -- Key/IV Storage
 * Library-internal storage for per-image encryption keys and IVs.
 * Populated by the validation hook, consumed by boot_xip_populate_rsp.
 */
#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES_XIP)

#include "xip_enc.h"

static struct {
    uint8_t key[XIP_ENC_KEY_SIZE];
    uint8_t iv[XIP_ENC_IV_SIZE];
    bool valid;
} xip_keys[XIP_ENC_MAX_IMAGES];

void xip_enc_store_key(int img_index, const uint8_t *key, const uint8_t *iv)
{
    if ((img_index >= 0) && ((uint32_t)img_index < XIP_ENC_MAX_IMAGES)
        && (key != NULL) && (iv != NULL)) {
        (void)memcpy(xip_keys[img_index].key, key, XIP_ENC_KEY_SIZE);
        (void)memcpy(xip_keys[img_index].iv, iv, XIP_ENC_IV_SIZE);
        xip_keys[img_index].valid = true;
    }
}

int xip_enc_get_key(int img_index, uint8_t *key, uint8_t *iv)
{
    if ((img_index < 0) || ((uint32_t)img_index >= XIP_ENC_MAX_IMAGES) ||
        !xip_keys[img_index].valid) {
        return -1;
    }
    if (key != NULL) {
        (void)memcpy(key, xip_keys[img_index].key, XIP_ENC_KEY_SIZE);
    }
    if (iv != NULL) {
        (void)memcpy(iv, xip_keys[img_index].iv, XIP_ENC_IV_SIZE);
    }
    return 0;
}

void xip_enc_clear_keys(void)
{
    xip_enc_zeroize(xip_keys, sizeof(xip_keys));
}

void boot_xip_populate_rsp(int img_index, struct boot_rsp *rsp)
{
    if ((rsp != NULL) && (img_index >= 0) && ((uint32_t)img_index < XIP_ENC_MAX_IMAGES)) {
        (void)xip_enc_get_key(img_index,
                              (uint8_t *)rsp->br_xip_key,
                              (uint8_t *)rsp->br_xip_iv);
    }
}

#endif /* MCUBOOT_ENC_IMAGES_XIP */
