/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 * Copyright (c) 2024 Cypress Semiconductor Corporation (an Infineon company)
 *
 * XIP Encryption Library - ECIES-P256 Key Unwrap
 *
 * Ported from boot/bootutil/src/encrypted.c (boot_enc_decrypt).
 * Uses bootutil crypto wrappers (portable across tinycrypt/mbedTLS/PSA).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES_XIP)

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "xip_enc.h"

#include "bootutil/sign_key.h"
#include "bootutil/crypto/ecdh_p256.h"
#include "bootutil/crypto/hmac_sha256.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/crypto/aes_ctr.h"

#include "mbedtls/asn1.h"

/* The encryption private key - declared in sign_key.h as bootutil_key,
 * but the encryption key is a separate symbol provided by the platform */
extern const struct bootutil_key bootutil_enc_key;

/* AES-128 key and block sizes */
#define XIP_AES_KEY_SIZE    16u
#define XIP_AES_BLOCK_SIZE  16u

/*
 * ECIES-P256 TLV layout:
 *
 * Standard TLV (113 bytes):
 *   [0..64]     ephemeral EC public key (65 bytes, uncompressed)
 *   [65..96]    HMAC-SHA256 tag (32 bytes)
 *   [97..112]   AES-CTR encrypted AES key (16 bytes)
 *
 * Extended TLV (177 bytes):
 *   [0..112]    same as standard
 *   [113..144]  random HKDF salt (32 bytes)
 *   [145..176]  extra HKDF output: key_iv (16) + xip_iv (16)
 */

/* EC256 constants - reuse definitions from enc_key_public.h and tinycrypt */
#define SHARED_KEY_LEN         NUM_ECC_BYTES
#define PRIV_KEY_LEN           NUM_ECC_BYTES

/* Standard TLV size: 65 + 32 + 16 */
#define ECIES_STD_TLV_SIZE     (EC_CIPHERKEY_INDEX + XIP_AES_KEY_SIZE)
/* Extended TLV adds 32 bytes salt + 32 bytes (key_iv + xip_iv from HKDF) */
#define ECIES_EXT_SALT_SIZE    BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE
#define ECIES_EXT_TLV_SIZE     (ECIES_STD_TLV_SIZE + ECIES_EXT_SALT_SIZE + \
                                (XIP_AES_BLOCK_SIZE * 2u))

/* HKDF info string - same as upstream MCUBoot (16 bytes, no NUL) */
static const uint8_t hkdf_info[] = { 'M','C','U','B','o','o','t','_',
                                     'E','C','I','E','S','_','v','1' };

/*
 * Parse PKCS#8-encoded EC256 private key (ported from encrypted.c parse_ec256_enckey).
 * Extracts the raw 32-byte private key scalar from a DER-encoded PKCS#8 structure.
 */
static int
parse_ec256_enckey(uint8_t **p, uint8_t *end, uint8_t *private_key)
{
    int rc;
    size_t len;
    int version;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (rc != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 0) {
        return -3;
    }

    if ((rc = mbedtls_asn1_get_alg(p, end, &alg, &param)) != 0) {
        return -5;
    }

    if ((rc = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return -8;
    }

    /* RFC5915 - ECPrivateKey */
    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -9;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 1) {
        return -10;
    }

    /* privateKey */
    if ((rc = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return -11;
    }

    if (len != NUM_ECC_BYTES) {
        return -12;
    }

    (void)memcpy(private_key, *p, len);

    return 0;
}

/*
 * HKDF (RFC 5869) using portable HMAC-SHA256 wrappers.
 * Ported from encrypted.c.
 */
static int
hkdf(const uint8_t *ikm, uint16_t ikm_len,
     const uint8_t *info, uint16_t info_len,
     const uint8_t *salt, uint16_t salt_len,
     uint8_t *okm, uint16_t *okm_len)
{
    bootutil_hmac_sha256_context hmac;
    uint8_t prk[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t T[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint16_t off;
    uint16_t len;
    uint8_t counter;
    bool first;
    int rc;

    if (ikm == NULL || okm == NULL || ikm_len == 0) {
        return -1;
    }

    /*
     * Extract: PRK = HMAC-SHA256(salt, IKM)
     */
    bootutil_hmac_sha256_init(&hmac);

    rc = bootutil_hmac_sha256_set_key(&hmac, salt, salt_len);
    if (rc != 0) {
        goto error;
    }

    rc = bootutil_hmac_sha256_update(&hmac, ikm, ikm_len);
    if (rc != 0) {
        goto error;
    }

    rc = bootutil_hmac_sha256_finish(&hmac, prk, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        goto error;
    }

    bootutil_hmac_sha256_drop(&hmac);

    /*
     * Expand: OKM = T(1) || T(2) || ...
     */
    len = *okm_len;
    counter = 1;
    first = true;
    for (off = 0; len > 0; counter++) {
        bootutil_hmac_sha256_init(&hmac);

        rc = bootutil_hmac_sha256_set_key(&hmac, prk, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
        if (rc != 0) {
            goto error;
        }

        if (first) {
            first = false;
        } else {
            rc = bootutil_hmac_sha256_update(&hmac, T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            if (rc != 0) {
                goto error;
            }
        }

        rc = bootutil_hmac_sha256_update(&hmac, info, info_len);
        if (rc != 0) {
            goto error;
        }

        rc = bootutil_hmac_sha256_update(&hmac, &counter, 1);
        if (rc != 0) {
            goto error;
        }

        rc = bootutil_hmac_sha256_finish(&hmac, T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
        if (rc != 0) {
            goto error;
        }

        bootutil_hmac_sha256_drop(&hmac);

        if (len > BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
            memcpy(&okm[off], T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            off += BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
            len -= BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
        } else {
            memcpy(&okm[off], T, len);
            len = 0;
        }
    }

    xip_enc_zeroize(prk, sizeof(prk));
    xip_enc_zeroize(T, sizeof(T));
    return 0;

error:
    xip_enc_zeroize(prk, sizeof(prk));
    xip_enc_zeroize(T, sizeof(T));
    bootutil_hmac_sha256_drop(&hmac);
    return -1;
}

/*
 * xip_enc_ecies_unwrap - ECIES-P256 key unwrap with extended TLV support.
 *
 * Ported from boot_enc_decrypt() in boot/bootutil/src/encrypted.c.
 * Extracts the AES encryption key and XIP IV from an ECIES-P256 TLV envelope.
 *
 * For standard TLV (113 bytes): IV output is all zeros.
 * For extended TLV (177 bytes): IV comes from HKDF output bytes [64..79].
 */
int
xip_enc_ecies_unwrap(const uint8_t *tlv_buf, uint16_t tlv_len,
                     uint8_t *out_key, uint8_t *out_iv)
{
    bootutil_ecdh_p256_context ecdh_p256;
    bootutil_hmac_sha256_context hmac;
    bootutil_aes_ctr_context aes_ctr;

    uint8_t shared[SHARED_KEY_LEN];
    uint8_t private_key[PRIV_KEY_LEN];

    /*
     * derived_key layout:
     *   [0..15]   AES-CTR key for decrypting the wrapped key
     *   [16..47]  HMAC-SHA256 key
     *   [48..63]  key_iv  (AES-CTR IV for key decryption) - extended only
     *   [64..79]  xip_iv  (XIP encryption IV)             - extended only
     */
    uint8_t derived_key[XIP_AES_KEY_SIZE +
                        BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE +
                        XIP_AES_BLOCK_SIZE * 2u];
    uint8_t tag[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t counter[XIP_AES_BLOCK_SIZE];

    uint16_t hkdf_len;
    uint16_t out_len;
    const uint8_t *hkdf_salt;
    uint8_t *key_iv;
    uint8_t *xip_iv_ptr;

    uint8_t zero_salt[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];

    uint8_t *cp;
    uint8_t *cpend;
    int rc = -1;

    /* Validate TLV length */
    if ((tlv_buf == NULL) || (out_key == NULL) || (out_iv == NULL)) {
        return -1;
    }

    if (tlv_len != ECIES_STD_TLV_SIZE && tlv_len != ECIES_EXT_TLV_SIZE) {
        return -1;
    }

    /*
     * Step 1: Load the EC256 decryption private key from bootutil_enc_key.
     */
    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    rc = parse_ec256_enckey(&cp, cpend, private_key);
    if (rc != 0) {
        xip_enc_zeroize(private_key, sizeof(private_key));
        return rc;
    }

    /*
     * Step 2: ECDH key agreement.
     */
    bootutil_ecdh_p256_init(&ecdh_p256);

    rc = bootutil_ecdh_p256_shared_secret(&ecdh_p256,
                                          &tlv_buf[EC_PUBK_INDEX],
                                          private_key,
                                          shared);

    bootutil_ecdh_p256_drop(&ecdh_p256);

    /* Zeroize private key immediately after use */
    xip_enc_zeroize(private_key, sizeof(private_key));

    if (rc != 0) {
        return -1;
    }

    /*
     * Step 3: HKDF to expand the shared secret into crypto keys.
     *
     * Standard mode (113-byte TLV):
     *   - Salt: all zeros (32 bytes)
     *   - Output: 48 bytes = AES key (16) + HMAC key (32)
     *   - key_iv / xip_iv: all zeros
     *
     * Extended mode (177-byte TLV):
     *   - Salt: from TLV bytes [113..144] (32 bytes)
     *   - Output: 80 bytes = AES key (16) + HMAC key (32) + key_iv (16) + xip_iv (16)
     */
    (void)memset(counter, 0, XIP_AES_BLOCK_SIZE);
    (void)memset(zero_salt, 0, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);

    hkdf_len = XIP_AES_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
    hkdf_salt = zero_salt;
    key_iv = counter;        /* default: zeros */
    xip_iv_ptr = NULL;

    if (tlv_len > ECIES_STD_TLV_SIZE) {
        /* Extended TLV: use random salt and derive key_iv + xip_iv */
        hkdf_len += XIP_AES_BLOCK_SIZE * 2u;

        hkdf_salt = &tlv_buf[ECIES_STD_TLV_SIZE];

        key_iv = &derived_key[XIP_AES_KEY_SIZE +
                              BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];

        xip_iv_ptr = &derived_key[XIP_AES_KEY_SIZE +
                                  BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE +
                                  XIP_AES_BLOCK_SIZE];
    }

    out_len = hkdf_len;
    rc = hkdf(shared, SHARED_KEY_LEN,
              hkdf_info, XIP_AES_BLOCK_SIZE,
              hkdf_salt, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE,
              derived_key, &out_len);

    /* Zeroize shared secret */
    xip_enc_zeroize(shared, sizeof(shared));

    if ((rc != 0) || (hkdf_len != out_len)) {
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    /*
     * Step 4: HMAC-SHA256 verification.
     */
    bootutil_hmac_sha256_init(&hmac);

    rc = bootutil_hmac_sha256_set_key(&hmac,
                                      &derived_key[XIP_AES_KEY_SIZE],
                                      BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    rc = bootutil_hmac_sha256_update(&hmac,
                                     &tlv_buf[EC_CIPHERKEY_INDEX],
                                     XIP_AES_KEY_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    rc = bootutil_hmac_sha256_finish(&hmac, tag, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    if (xip_enc_ct_compare(tag, &tlv_buf[EC_TAG_INDEX],
                              BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    bootutil_hmac_sha256_drop(&hmac);

    /*
     * Step 5: Output the XIP IV.
     *
     * Copy only the 12-byte nonce portion of xip_iv.
     * The last 4 bytes are the counter portion -- zeroed here because
     * the hardware computes the counter from the absolute address.
     * Matches edgeprotecttools: enc_nonce = derived_key[64:76] + bytes(4)
     */
    (void)memset(out_iv, 0, XIP_AES_BLOCK_SIZE);
    if (xip_iv_ptr != NULL) {
        (void)memcpy(out_iv, xip_iv_ptr, 12);
    }

    /*
     * Step 6: Decrypt the wrapped AES key using AES-CTR.
     */
    bootutil_aes_ctr_init(&aes_ctr);

    rc = bootutil_aes_ctr_set_key(&aes_ctr, derived_key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        xip_enc_zeroize(derived_key, sizeof(derived_key));
        return -1;
    }

    rc = bootutil_aes_ctr_decrypt(&aes_ctr, key_iv,
                                  &tlv_buf[EC_CIPHERKEY_INDEX],
                                  XIP_AES_KEY_SIZE,
                                  0, out_key);

    bootutil_aes_ctr_drop(&aes_ctr);

    /* Zeroize all sensitive material */
    xip_enc_zeroize(derived_key, sizeof(derived_key));
    xip_enc_zeroize(tag, sizeof(tag));

    if (rc != 0) {
        xip_enc_zeroize(out_key, XIP_AES_KEY_SIZE);
        xip_enc_zeroize(out_iv, XIP_AES_BLOCK_SIZE);
        return -1;
    }

    return 0;
}

#endif /* MCUBOOT_ENC_IMAGES_XIP */
