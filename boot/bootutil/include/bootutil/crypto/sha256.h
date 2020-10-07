/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2017-2019 JUUL Labs
 */

/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_SHA256_H_
#define __BOOTUTIL_CRYPTO_SHA256_H_

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT) + \
     defined(MCUBOOT_USE_CC310)) != 1
    #error "One crypto backend must be defined: either CC310, MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/sha256.h>
    #define BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE (64)
    #define BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE (32)
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include <tinycrypt/sha256.h>
    #include <tinycrypt/constants.h>
    #define BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE TC_SHA256_BLOCK_SIZE
    #define BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE TC_SHA256_DIGEST_SIZE
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
    #include <cc310_glue.h>
    #define BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE (64)
    #define BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE (32)
#endif /* MCUBOOT_USE_CC310 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
typedef mbedtls_sha256_context bootutil_sha256_context;

static inline void bootutil_sha256_init(bootutil_sha256_context *ctx)
{
    mbedtls_sha256_init(ctx);
    (void)mbedtls_sha256_starts_ret(ctx, 0);
}

static inline void bootutil_sha256_drop(bootutil_sha256_context *ctx)
{
    /* XXX: config defines MBEDTLS_PLATFORM_NO_STD_FUNCTIONS so no need to free */
    /* (void)mbedtls_sha256_free(ctx); */
    (void)ctx;
}

static inline int bootutil_sha256_update(bootutil_sha256_context *ctx,
                                         const void *data,
                                         uint32_t data_len)
{
    return mbedtls_sha256_update_ret(ctx, data, data_len);
}

static inline int bootutil_sha256_finish(bootutil_sha256_context *ctx,
                                          uint8_t *output)
{
    return mbedtls_sha256_finish_ret(ctx, output);
}
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef struct tc_sha256_state_struct bootutil_sha256_context;
static inline void bootutil_sha256_init(bootutil_sha256_context *ctx)
{
    tc_sha256_init(ctx);
}

static inline void bootutil_sha256_drop(bootutil_sha256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_sha256_update(bootutil_sha256_context *ctx,
                                         const void *data,
                                         uint32_t data_len)
{
    return tc_sha256_update(ctx, data, data_len);
}

static inline int bootutil_sha256_finish(bootutil_sha256_context *ctx,
                                          uint8_t *output)
{
    return tc_sha256_final(output, ctx);
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
static inline void bootutil_sha256_init(bootutil_sha256_context *ctx)
{
    cc310_sha256_init(ctx);
}

static inline void bootutil_sha256_drop(bootutil_sha256_context *ctx)
{
    (void)ctx;
    nrf_cc310_disable();
}

static inline int bootutil_sha256_update(bootutil_sha256_context *ctx,
                                          const void *data,
                                          uint32_t data_len)
{
    cc310_sha256_update(ctx, data, data_len);
    return 0;
}

static inline int bootutil_sha256_finish(bootutil_sha256_context *ctx,
                                          uint8_t *output)
{
    cc310_sha256_finalize(ctx, output);
    return 0;
}
#endif /* MCUBOOT_USE_CC310 */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_SHA256_H_ */
