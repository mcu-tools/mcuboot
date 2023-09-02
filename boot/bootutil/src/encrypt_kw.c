/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES) && defined(MCUBOOT_ENCRYPT_KW)

#include "bootutil/crypto/aes_kw.h"
#include "encrypted_priv.h"

static int key_unwrap(const uint8_t *wrapped, uint8_t *enckey)
{
    bootutil_aes_kw_context aes_kw;
    int rc;

    bootutil_aes_kw_init(&aes_kw);
    rc = bootutil_aes_kw_set_unwrap_key(&aes_kw, bootutil_enc_key.key, *bootutil_enc_key.len);
    if (rc != 0) {
        goto done;
    }
    rc = bootutil_aes_kw_unwrap(&aes_kw, wrapped, TLV_ENC_KW_SZ, enckey, BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        goto done;
    }

done:
    bootutil_aes_kw_drop(&aes_kw);
    return rc;
}

int
boot_enc_decrypt(const uint8_t *buf, uint8_t *enckey)
{
    assert(*bootutil_enc_key.len == BOOT_ENC_KEY_SIZE);
    return key_unwrap(buf, enckey);
}


#endif /* MCUBOOT_ENC_IMAGES && MCUBOOT_ENCRYPT_KW */

