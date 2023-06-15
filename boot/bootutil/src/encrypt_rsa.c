/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES) && defined(MCUBOOT_ENCRYPT_RSA)

#define BOOTUTIL_CRYPTO_RSA_CRYPT_ENABLED
#include "bootutil/crypto/rsa.h"
#include "encrypted_priv.h"

/*
 * Decrypt an encryption key TLV.
 *
 * @param buf An encryption TLV read from flash (build time fixed length)
 * @param enckey An AES-128 or AES-256 key sized buffer to store to plain key.
 */
int
boot_enc_decrypt(const uint8_t *buf, uint8_t *enckey)
{
    bootutil_rsa_context rsa;
    uint8_t *cp;
    uint8_t *cpend;
    size_t olen;

    int rc = -1;

    bootutil_rsa_init(&rsa);
    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    /* The enckey is encrypted through RSA so for decryption we need the private key */
    rc = bootutil_rsa_parse_private_key(&rsa, &cp, cpend);
    if (rc) {
        bootutil_rsa_drop(&rsa);
        return rc;
    }

    rc = bootutil_rsa_oaep_decrypt(&rsa, &olen, buf, enckey, BOOT_ENC_KEY_SIZE);
    bootutil_rsa_drop(&rsa);

    return rc;
}

#endif /* MCUBOOT_ENC_IMAGES && MCUBOOT_ENCRYPT_RSA */
