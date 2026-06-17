/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Simulator SHA-256 backend for MCUBOOT_USE_CUSTOM_CRYPTO.
 * Implements the bootutil SHA abstraction using mbedTLS.
 */

#ifndef SIM_CUSTOM_SHA_H
#define SIM_CUSTOM_SHA_H

#include <stdint.h>
#include "bootutil/crypto/common.h"
#if MCUBOOT_MBEDTLS_CRYPTO_IN_PRIVATE_SUBDIR
#include <mbedtls/private/sha256.h>
#else
#include <mbedtls/sha256.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef mbedtls_sha256_context bootutil_sha_context;

static inline int bootutil_sha_init(bootutil_sha_context *ctx)
{
    mbedtls_sha256_init(ctx);
    return mbedtls_sha256_starts(ctx, 0 /* is224=0 → SHA-256 */);
}

static inline int bootutil_sha_drop(bootutil_sha_context *ctx)
{
    mbedtls_sha256_free(ctx);
    return 0;
}

static inline int bootutil_sha_update(bootutil_sha_context *ctx,
                                      const void *data,
                                      uint32_t data_len)
{
    return mbedtls_sha256_update(ctx, (const unsigned char *)data, data_len);
}

static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output)
{
    return mbedtls_sha256_finish(ctx, output);
}

#ifdef __cplusplus
}
#endif

#endif /* SIM_CUSTOM_SHA_H */
