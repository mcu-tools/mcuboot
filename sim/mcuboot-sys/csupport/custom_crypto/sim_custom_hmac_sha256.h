/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Simulator HMAC-SHA256 backend for MCUBOOT_USE_CUSTOM_CRYPTO.
 * Implements the bootutil HMAC-SHA256 abstraction using mbedTLS.
 */

#ifndef SIM_CUSTOM_HMAC_SHA256_H
#define SIM_CUSTOM_HMAC_SHA256_H

#include <stdint.h>
#include <mbedtls/md.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef mbedtls_md_context_t bootutil_hmac_sha256_context;

static inline void bootutil_hmac_sha256_init(bootutil_hmac_sha256_context *ctx)
{
    mbedtls_md_init(ctx);
}

static inline void bootutil_hmac_sha256_drop(bootutil_hmac_sha256_context *ctx)
{
    mbedtls_md_free(ctx);
}

static inline int bootutil_hmac_sha256_set_key(bootutil_hmac_sha256_context *ctx,
                                               const uint8_t *key,
                                               unsigned int key_size)
{
    int rc;

    rc = mbedtls_md_setup(ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (rc != 0) {
        return rc;
    }
    rc = mbedtls_md_hmac_starts(ctx, key, key_size);
    return rc;
}

static inline int bootutil_hmac_sha256_update(bootutil_hmac_sha256_context *ctx,
                                              const void *data,
                                              unsigned int data_length)
{
    return mbedtls_md_hmac_update(ctx, data, data_length);
}

static inline int bootutil_hmac_sha256_finish(bootutil_hmac_sha256_context *ctx,
                                              uint8_t *tag,
                                              unsigned int taglen)
{
    /*
     * Finalize the HMAC computation and output the tag.
     * mbedtls_md_hmac_finish() always outputs the full 32-byte SHA-256 digest
     * and does not support truncation. taglen is ignored; caller must provide
     * a 32-byte buffer.
     */
    (void)taglen;
    return mbedtls_md_hmac_finish(ctx, tag);
}

#ifdef __cplusplus
}
#endif

#endif /* SIM_CUSTOM_HMAC_SHA256_H */
