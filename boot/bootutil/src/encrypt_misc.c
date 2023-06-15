/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)

#include "bootutil/crypto/aes_ctr.h"
#include "encrypted_priv.h"

#if defined(MCUBOOT_ENCRYPT_RSA)
#include "bootutil/crypto/rsa.h"
#endif /* MCUBOOT_ENCRYPT_RSA */

#if ( (defined(MCUBOOT_ENCRYPT_RSA) && defined(MCUBOOT_USE_MBED_TLS) && !defined(MCUBOOT_USE_PSA_CRYPTO)) || \
      (defined(MCUBOOT_ENCRYPT_EC256) && defined(MCUBOOT_USE_MBED_TLS)) )
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
int fake_rng(void *p_rng, unsigned char *output, size_t len)
{
    size_t i;

    (void)p_rng;
    for (i = 0; i < len; i++) {
        output[i] = (char)i;
    }

    return 0;
}
#endif /* MBEDTLS_VERSION_NUMBER */
#endif /* (MCUBOOT_ENCRYPT_RSA && MCUBOOT_USE_MBED_TLS && !MCUBOOT_USE_PSA_CRYPTO) ||
          (MCUBOOT_ENCRYPT_EC256 && MCUBOOT_USE_MBED_TLS) */

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)

#if defined(_compare)
static inline int bootutil_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t size)
{
    return _compare(a, b, size);
}
#else
static int bootutil_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t size)
{
    const uint8_t *tempa = a;
    const uint8_t *tempb = b;
    uint8_t result = 0;
    unsigned int i;

    for (i = 0; i < size; i++) {
        result |= tempa[i] ^ tempb[i];
    }
    return result;
}
#endif /* _compare */

/*
 * HKDF as described by RFC5869.
 *
 * @param ikm       The input data to be derived.
 * @param ikm_len   Length of the input data.
 * @param info      An information tag.
 * @param info_len  Length of the information tag.
 * @param okm       Output of the KDF computation.
 * @param okm_len   On input the requested length; on output the generated length
 */
static int
hkdf(uint8_t *ikm, uint16_t ikm_len, uint8_t *info, uint16_t info_len,
        uint8_t *okm, uint16_t *okm_len)
{
    bootutil_hmac_sha256_context hmac;
    uint8_t salt[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t prk[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t T[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint16_t off;
    uint16_t len;
    uint8_t counter;
    bool first;
    int rc;

    /*
     * Extract
     */

    if (ikm == NULL || okm == NULL || ikm_len == 0) {
        return -1;
    }

    bootutil_hmac_sha256_init(&hmac);

    memset(salt, 0, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    rc = bootutil_hmac_sha256_set_key(&hmac, salt, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
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

    /*
     * Expand
     */

    len = *okm_len;
    counter = 1;
    first = true;
    for (off = 0; len > 0; off += BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE, ++counter) {
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

        if (len > BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
            memcpy(&okm[off], T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            len -= BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
        } else {
            memcpy(&okm[off], T, len);
            len = 0;
        }
    }

    bootutil_hmac_sha256_drop(&hmac);
    return 0;

error:
    bootutil_hmac_sha256_drop(&hmac);
    return -1;
}

/*
* Expand shared secret to create keys for AES-128-CTR + HMAC-SHA256
*/
int expand_secret(uint8_t *derived_key, uint8_t *shared)
{
    uint16_t len = BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
    int rc = hkdf(shared, SHARED_KEY_LEN, (uint8_t *)"MCUBoot_ECIES_v1", 16,
            derived_key, &len);
    return rc != 0 || len != (BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
}

/*
* HMAC the key and check that our received MAC matches the generated tag
*/
int HMAC_key(const uint8_t *buf, uint8_t *derived_key)
{
    uint8_t tag[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    bootutil_hmac_sha256_context hmac;
    int rc = -1;

    bootutil_hmac_sha256_init(&hmac);

    rc = bootutil_hmac_sha256_set_key(&hmac, &derived_key[BOOT_ENC_KEY_SIZE], 32);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    rc = bootutil_hmac_sha256_update(&hmac, &buf[EC_CIPHERKEY_INDEX], BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    /* Assumes the tag buffer is at least sizeof(hmac_tag_size(state)) bytes */
    rc = bootutil_hmac_sha256_finish(&hmac, tag, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    if (bootutil_constant_time_compare(tag, &buf[EC_TAG_INDEX], 32) != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    bootutil_hmac_sha256_drop(&hmac);

    return rc;
}

/*
* Finally decrypt the received ciphered key
*/
int decrypt_ciphered_key( const uint8_t *buf, uint8_t *counter,
    uint8_t *derived_key, uint8_t *enckey)
{
    bootutil_aes_ctr_context aes_ctr;
    int rc = -1;

    bootutil_aes_ctr_init(&aes_ctr);
    
    rc = bootutil_aes_ctr_set_key(&aes_ctr, derived_key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    memset(counter, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    rc = bootutil_aes_ctr_decrypt(&aes_ctr, counter, &buf[EC_CIPHERKEY_INDEX],
        BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE, 0, enckey);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    bootutil_aes_ctr_drop(&aes_ctr);

    return rc;
}

#endif /* MCUBOOT_ENCRYPT_EC256 || MCUBOOT_ENCRYPT_X25519 */

#endif /* MCUBOOT_ENC_IMAGES */
