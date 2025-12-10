/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2017-2019 JUUL Labs
 * Copyright (c) 2021-2023 Arm Limited
 * Copyright (c) 2025 Siemens AG
 */

/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, the choices are: MCUBOOT_USE_MBED_TLS, MCUBOOT_USE_TINYCRYPT,
 * MCUBOOT_USE_PSA_CRYPTO, MCUBOOT_USE_CC310. Note that support for MCUBOOT_USE_PSA_CRYPTO
 * is still experimental and it might not support all the crypto abstractions
 * that MCUBOOT_USE_MBED_TLS supports. For this reason, it's allowed to have
 * both of them defined, and for crypto modules that support both abstractions,
 * the MCUBOOT_USE_PSA_CRYPTO will take precedence.
 */

#ifndef __BOOTUTIL_CRYPTO_SHA_H_
#define __BOOTUTIL_CRYPTO_SHA_H_

#include "mcuboot_config/mcuboot_config.h"
#include "mcuboot_config/mcuboot_logging.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO) || defined(MCUBOOT_USE_MBED_TLS)
#define MCUBOOT_USE_PSA_OR_MBED_TLS
#endif /* MCUBOOT_USE_PSA_CRYPTO || MCUBOOT_USE_MBED_TLS */

#if (defined(MCUBOOT_USE_PSA_OR_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT) + \
     defined(MCUBOOT_USE_CC310)) != 1
    #error "One crypto backend must be defined: either CC310/MBED_TLS/TINYCRYPT/PSA_CRYPTO"
#endif

#if defined(MCUBOOT_SHA512)
    #define IMAGE_HASH_SIZE (64)
    #define EXPECTED_HASH_TLV IMAGE_TLV_SHA512
#elif defined(MCUBOOT_SIGN_EC384)
    #define IMAGE_HASH_SIZE (48)
    #define EXPECTED_HASH_TLV IMAGE_TLV_SHA384
#else
    #define IMAGE_HASH_SIZE (32)
    #define EXPECTED_HASH_TLV IMAGE_TLV_SHA256
#endif /* MCUBOOT_SIGN */

/* Universal defines for SHA-256 */
#define BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE  (64)
#define BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE (32)

#if defined(MCUBOOT_USE_PSA_CRYPTO)

#include <psa/crypto.h>

#elif defined(MCUBOOT_USE_MBED_TLS)

#ifdef MCUBOOT_SHA512
#include <mbedtls/sha512.h>
#else
#include <mbedtls/sha256.h>
#endif

#include <mbedtls/version.h>
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#include <mbedtls/compat-2.x.h>
#endif

#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
#if defined(MCUBOOT_SHA512)
    #include <tinycrypt/sha512.h>
#else
    #include <tinycrypt/sha256.h>
#endif
    #include <tinycrypt/constants.h>
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
    #include <cc310_glue.h>
#endif /* MCUBOOT_USE_CC310 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_PSA_CRYPTO)

typedef struct {
    psa_hash_operation_t operation;
    psa_status_t status;
} bootutil_sha_context;

static inline void bootutil_sha_init(bootutil_sha_context *ctx)
{
    ctx->operation = psa_hash_operation_init();
#if defined(MCUBOOT_SHA512)
    ctx->status = psa_hash_setup(&ctx->operation, PSA_ALG_SHA_512);
#elif defined(MCUBOOT_SIGN_EC384)
    ctx->status = psa_hash_setup(&ctx->operation, PSA_ALG_SHA_384);
#else
    ctx->status = psa_hash_setup(&ctx->operation, PSA_ALG_SHA_256);
#endif
}

static inline void bootutil_sha_drop(bootutil_sha_context *ctx)
{
    psa_hash_abort(&ctx->operation);
}

static inline void bootutil_sha_update(bootutil_sha_context *ctx,
                                      const void *data,
                                      uint32_t data_len)
{
    if (ctx->status) {
        return;
    }
    ctx->status = psa_hash_update(&ctx->operation, data, data_len);
}

static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output)
{
    size_t hash_length;

    if (ctx->status) {
        return (int)ctx->status;
    }
    return (int)psa_hash_finish(&ctx->operation, output, IMAGE_HASH_SIZE, &hash_length);
}

#elif defined(MCUBOOT_USE_MBED_TLS)

typedef struct {
#ifdef MCUBOOT_SHA512
    mbedtls_sha512_context mbedtls_ctx;
#else
    mbedtls_sha256_context mbedtls_ctx;
#endif
    int error_code;
} bootutil_sha_context;

static inline void bootutil_sha_init(bootutil_sha_context *ctx)
{
#ifdef MCUBOOT_SHA512
    mbedtls_sha512_init(&ctx->mbedtls_ctx);
    ctx->error_code = mbedtls_sha512_starts_ret(&ctx->mbedtls_ctx, 0);
#else
    mbedtls_sha256_init(&ctx->mbedtls_ctx);
    ctx->error_code = mbedtls_sha256_starts_ret(&ctx->mbedtls_ctx, 0);
#endif
}

static inline void bootutil_sha_drop(bootutil_sha_context *ctx)
{
#ifdef MCUBOOT_SHA512
    mbedtls_sha512_free(&ctx->mbedtls_ctx);
#else
    mbedtls_sha256_free(&ctx->mbedtls_ctx);
#endif
}

static inline void bootutil_sha_update(bootutil_sha_context *ctx,
                                      const void *data,
                                      uint32_t data_len)
{
    if (ctx->error_code) {
        return;
    }
#ifdef MCUBOOT_SHA512
    ctx->error_code = mbedtls_sha512_update_ret(&ctx->mbedtls_ctx, data, data_len);
#else
    ctx->error_code = mbedtls_sha256_update_ret(&ctx->mbedtls_ctx, data, data_len);
#endif
}

static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output)
{
    if (ctx->error_code) {
        return ctx->error_code;
    }
#ifdef MCUBOOT_SHA512
    return mbedtls_sha512_finish_ret(&ctx->mbedtls_ctx, output);
#else
    return mbedtls_sha256_finish_ret(&ctx->mbedtls_ctx, output);
#endif
}

#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)

typedef struct {
#if defined(MCUBOOT_SHA512)
    struct tc_sha512_state_struct state_struct;
#else
    struct tc_sha256_state_struct state_struct;
#endif
    int status;
} bootutil_sha_context;

static inline void bootutil_sha_init(bootutil_sha_context *ctx)
{
#if defined(MCUBOOT_SHA512)
    ctx->status = tc_sha512_init(&ctx->state_struct);
#else
    ctx->status = tc_sha256_init(&ctx->state_struct);
#endif
}

static inline void bootutil_sha_drop(bootutil_sha_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_sha_update(bootutil_sha_context *ctx,
                                       const void *data,
                                       uint32_t data_len)
{
    if (ctx->status == TC_CRYPTO_FAIL) {
        return;
    }
#if defined(MCUBOOT_SHA512)
    ctx->status = tc_sha512_update(&ctx->state_struct, data, data_len);
#else
    ctx->status = tc_sha256_update(&ctx->state_struct, data, data_len);
#endif
}

static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output)
{
    if (ctx->status == TC_CRYPTO_FAIL) {
        return 1;
    }
#if defined(MCUBOOT_SHA512)
    return TC_CRYPTO_FAIL == tc_sha512_final(output, &ctx->state_struct);
#else
    return TC_CRYPTO_FAIL == tc_sha256_final(output, &ctx->state_struct);
#endif
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
static inline void bootutil_sha_init(bootutil_sha_context *ctx)
{
    cc310_sha256_init(ctx);
}

static inline void bootutil_sha_drop(bootutil_sha_context *ctx)
{
    (void)ctx;
    nrf_cc310_disable();
}

static inline void bootutil_sha_update(bootutil_sha_context *ctx,
                                       const void *data,
                                       uint32_t data_len)
{
    cc310_sha256_update(ctx, data, data_len);
}

static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output)
{
    cc310_sha256_finalize(ctx, output);
    return 0;
}
#endif /* MCUBOOT_USE_CC310 */

/**
 * Does bootutil_sha_init, bootutil_sha_update, and bootutil_sha_finish at once.
 *
 * @param data     Pointer to the data to hash.
 * @param data_len Length of @c data in bytes.
 * @param digest   Pointer to where the resulting digest shall be stored.
 *
 * @return         @c 0 on success and nonzero otherwise.
 */
int bootutil_sha(const uint8_t *data, size_t data_len,
                 uint8_t digest[static IMAGE_HASH_SIZE]);

/**
 * Computes an HMAC as per RFC 2104.
 *
 * @param key      The key to authenticate with.
 * @param key_len  Length of @c key in bytes.
 * @param data     The data to authenticate.
 * @param data_len Length of @c data in bytes.
 * @param hmac     Pointer to where the resulting HMAC shall be stored.
 *
 * @return         @c 0 on success and nonzero otherwise.
 */
int bootutil_sha_hmac(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len,
                      uint8_t hmac[static IMAGE_HASH_SIZE]);

/**
 * Extracts a key as per RFC 5869.
 *
 * @param salt     Optional salt value.
 * @param salt_len Length of @c salt in bytes.
 * @param ikm      Input keying material.
 * @param ikm_len  Length of @c ikm in bytes.
 * @param prk      Pointer to where the extracted key shall be stored.
 *
 * @return         @c 0 on success and nonzero otherwise.
 */
int bootutil_sha_hkdf_extract(const uint8_t *salt, size_t salt_len,
                              const uint8_t *ikm, size_t ikm_len,
                              uint8_t prk[static IMAGE_HASH_SIZE]);

/**
 * Expands a key as per RFC 5869.
 *
 * @param prk      A pseudorandom key of at least @c IMAGE_HASH_SIZE bytes.
 * @param prk_len  Length of @c prk in bytes.
 * @param info     Optional context and application specific information.
 * @param info_len Length of @c info in bytes.
 * @param okm      Output keying material.
 * @param okm_len  Length of @c okm in bytes (<= 255 * @c IMAGE_HASH_SIZE).
 *
 * @return         @c 0 on success and nonzero otherwise.
 */
int bootutil_sha_hkdf_expand(const uint8_t *prk, size_t prk_len,
                             const uint8_t *info, size_t info_len,
                             uint8_t *okm, uint_fast16_t okm_len);

/**
 * Performs both extraction and expansion as per RFC 5869.
 *
 * @param salt     Optional salt value.
 * @param salt_len Length of @c salt in bytes.
 * @param ikm      Input keying material.
 * @param ikm_len  Length of @c ikm in bytes.
 * @param info     Optional context and application specific information.
 * @param info_len Length of @c info in bytes.
 * @param okm      Output keying material.
 * @param okm_len  Length of @c okm in bytes (<= 255 * @c IMAGE_HASH_SIZE).
 *
 * @return         @c 0 on success and nonzero otherwise.
 */
int bootutil_sha_hkdf(const uint8_t *salt, size_t salt_len,
                      const uint8_t *ikm, size_t ikm_len,
                      const uint8_t *info, size_t info_len,
                      uint8_t *okm, uint_fast16_t okm_len);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_SHA_H_ */
