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

#if defined(__BOOTSIM__)
/* The simulator boots on parallel test threads; keep their keys apart. */
static __thread
#else
static
#endif
struct {
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

void boot_xip_populate_rsp(struct boot_rsp *rsp)
{
    int i;

    if (rsp == NULL) {
        return;
    }

    xip_enc_zeroize(rsp->br_xip_key, sizeof(rsp->br_xip_key));
    xip_enc_zeroize(rsp->br_xip_iv, sizeof(rsp->br_xip_iv));
    rsp->br_xip_valid = 0;

    for (i = 0; i < BOOT_IMAGE_NUMBER; i++) {
        if (xip_enc_get_key(i, (uint8_t *)rsp->br_xip_key[i],
                            (uint8_t *)rsp->br_xip_iv[i]) == 0) {
            rsp->br_xip_valid |= 1u << i;
        }
    }
}

#endif /* MCUBOOT_ENC_IMAGES_XIP */
