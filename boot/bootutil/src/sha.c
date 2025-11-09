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
#else /* MCUBOOT_USE_MBED_TLS */
#include <string.h>
#endif /* MCUBOOT_USE_MBED_TLS */

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
#else /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_SHA512) || defined(MCUBOOT_SIGN_EC384)
#define BLOCK_SIZE 128
#else
#define BLOCK_SIZE BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE
#endif

struct hmac_context {
    bootutil_sha_context sha_ctx;
    uint8_t opad[BLOCK_SIZE];
    int status;
};

static void
hmac_init(struct hmac_context *const hmac_ctx,
          const uint8_t *key, size_t key_len)
{
    uint8_t hashed_key[IMAGE_HASH_SIZE];
    uint_fast8_t i;
    uint8_t ipad[BLOCK_SIZE];

    if (key_len > BLOCK_SIZE) {
        hmac_ctx->status = bootutil_sha(key, key_len, hashed_key);
        if (hmac_ctx->status) {
          return;
        }
        key_len = IMAGE_HASH_SIZE;
        key = hashed_key;
    } else {
        hmac_ctx->status = 0;
    }
    for (i = 0; i < key_len; i++) {
        ipad[i] = key[i] ^ 0x36;
        hmac_ctx->opad[i] = key[i] ^ 0x5c;
    }
    for (; i < BLOCK_SIZE; i++) {
        ipad[i] = 0x36;
        hmac_ctx->opad[i] = 0x5c;
    }
    bootutil_sha_init(&hmac_ctx->sha_ctx);
    bootutil_sha_update(&hmac_ctx->sha_ctx, ipad, sizeof(ipad));
}

static void
hmac_update(struct hmac_context *const hmac_ctx,
            const uint8_t *const data, size_t data_len)
{
    if (hmac_ctx->status) {
        return;
    }
    bootutil_sha_update(&hmac_ctx->sha_ctx, data, data_len);
}

static int
hmac_finish(struct hmac_context *const hmac_ctx,
            uint8_t hmac[static IMAGE_HASH_SIZE])
{
    if (hmac_ctx->status) {
        return hmac_ctx->status;
    }
    hmac_ctx->status = bootutil_sha_finish(&hmac_ctx->sha_ctx, hmac);
    bootutil_sha_drop(&hmac_ctx->sha_ctx);
    if (hmac_ctx->status) {
        return hmac_ctx->status;
    }
    bootutil_sha_init(&hmac_ctx->sha_ctx);
    bootutil_sha_update(&hmac_ctx->sha_ctx, hmac_ctx->opad, sizeof(hmac_ctx->opad));
    bootutil_sha_update(&hmac_ctx->sha_ctx, hmac, IMAGE_HASH_SIZE);
    hmac_ctx->status = bootutil_sha_finish(&hmac_ctx->sha_ctx, hmac);
    bootutil_sha_drop(&hmac_ctx->sha_ctx);
    return hmac_ctx->status;
}

int
bootutil_sha_hmac(const uint8_t *const key, size_t key_len,
                  const uint8_t *const data, size_t data_len,
                  uint8_t hmac[static IMAGE_HASH_SIZE])
{
    struct hmac_context hmac_ctx;

    hmac_init(&hmac_ctx, key, key_len);
    hmac_update(&hmac_ctx, data, data_len);
    return hmac_finish(&hmac_ctx, hmac);
}

int
bootutil_sha_hkdf_extract(const uint8_t *const salt, size_t salt_len,
                          const uint8_t *const ikm, size_t ikm_len,
                          uint8_t prk[static IMAGE_HASH_SIZE])
{
    return bootutil_sha_hmac(salt, salt_len, ikm, ikm_len, prk);
}


int
bootutil_sha_hkdf_expand(const uint8_t *const prk, size_t prk_len,
                         const uint8_t *const info, size_t info_len,
                         uint8_t *const okm, uint_fast16_t okm_len)
{
    uint_fast8_t n;
    uint8_t i;
    struct hmac_context hmac_ctx;
    uint8_t t_i[IMAGE_HASH_SIZE];
    int rc;

    if (okm_len > 255 * IMAGE_HASH_SIZE) {
        return 1;
    }
    n = okm_len / IMAGE_HASH_SIZE + (okm_len % IMAGE_HASH_SIZE ? 1 : 0);

    for (i = 1; i <= n; i++) {
        hmac_init(&hmac_ctx, prk, prk_len);
        if (i != 1) {
            hmac_update(&hmac_ctx, t_i, sizeof(t_i));
        }
        hmac_update(&hmac_ctx, info, info_len);
        hmac_update(&hmac_ctx, &i, sizeof(i));
        rc = hmac_finish(&hmac_ctx, t_i);
        if (rc) {
            return rc;
        }
        memcpy(okm + ((i - 1) * IMAGE_HASH_SIZE),
               t_i,
               okm_len > IMAGE_HASH_SIZE ? IMAGE_HASH_SIZE : okm_len);
        okm_len -= IMAGE_HASH_SIZE;
    }
    return 0;
}

int
bootutil_sha_hkdf(const uint8_t *const salt, size_t salt_len,
                  const uint8_t *const ikm, size_t ikm_len,
                  const uint8_t *const info, size_t info_len,
                  uint8_t *const okm, uint_fast16_t okm_len)
{
    int rc;
    uint8_t prk[IMAGE_HASH_SIZE];

    rc = bootutil_sha_hkdf_extract(salt, salt_len, ikm, ikm_len, prk);
    if (rc) {
        return rc;
    }
    return bootutil_sha_hkdf_expand(prk, sizeof(prk), info, info_len, okm, okm_len);
}
#endif /* MCUBOOT_USE_MBED_TLS */
