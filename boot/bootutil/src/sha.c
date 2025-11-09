/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Siemens AG
 */

#include "bootutil/crypto/sha.h"
#include "mcuboot_config/mcuboot_config.h"
#include <stddef.h>
#include <stdint.h>

#if defined(MCUBOOT_USE_MBED_TLS)
#if defined(MBEDTLS_HKDF_C)
#include "mbedtls/hkdf.h"
#endif /* MBEDTLS_HKDF_C */
#include "mbedtls/md.h"
#endif

int
bootutil_sha(const uint8_t *const data, size_t data_len,
             uint8_t digest[static IMAGE_HASH_SIZE])
{
    bootutil_sha_context ctx;
    int rc;

    bootutil_sha_init(&ctx);
    bootutil_sha_update(&ctx, data, data_len);
    rc = bootutil_sha_finish(&ctx, digest);
    bootutil_sha_drop(&ctx);
    return rc;
}

#if defined(MCUBOOT_USE_MBED_TLS)

static const mbedtls_md_type_t md_type =
#if defined(MCUBOOT_SHA512)
        MBEDTLS_MD_SHA384;
#elif defined(MCUBOOT_SIGN_EC384)
        MBEDTLS_MD_SHA384;
#else
        MBEDTLS_MD_SHA256;
#endif /* MCUBOOT_SIGN */

int
bootutil_sha_hmac(const uint8_t *const key, size_t key_len,
                  const uint8_t *const data, size_t data_len,
                  uint8_t hmac[static IMAGE_HASH_SIZE])
{
    return mbedtls_md_hmac(mbedtls_md_info_from_type(md_type),
                           key, key_len,
                           data, data_len,
                           hmac);
}

#if defined(MBEDTLS_HKDF_C)
int
bootutil_sha_hkdf_extract(const uint8_t *const salt, size_t salt_len,
                          const uint8_t *const ikm, size_t ikm_len,
                          uint8_t prk[static IMAGE_HASH_SIZE])
{
    return mbedtls_hkdf_extract(mbedtls_md_info_from_type(md_type),
                                salt, salt_len,
                                ikm, ikm_len,
                                prk);
}

int
bootutil_sha_hkdf_expand(const uint8_t *const prk, size_t prk_len,
                         const uint8_t *const info, size_t info_len,
                         uint8_t *const okm, uint_fast16_t okm_len)
{
    return mbedtls_hkdf_expand(mbedtls_md_info_from_type(md_type),
                               prk, prk_len,
                               info, info_len,
                               okm, okm_len);
}

int
bootutil_sha_hkdf(const uint8_t *const salt, size_t salt_len,
                  const uint8_t *const ikm, size_t ikm_len,
                  const uint8_t *const info, size_t info_len,
                  uint8_t *const okm, uint_fast16_t okm_len)
{
    return mbedtls_hkdf(mbedtls_md_info_from_type(md_type),
                        salt, salt_len,
                        ikm, ikm_len,
                        info, info_len,
                        okm, okm_len);
}
#endif /* MBEDTLS_HKDF_C */
#endif /* MCUBOOT_USE_MBED_TLS */
