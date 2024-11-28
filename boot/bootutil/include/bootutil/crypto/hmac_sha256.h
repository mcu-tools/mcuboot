/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_HMAC_SHA256_H_
#define __BOOTUTIL_CRYPTO_HMAC_SHA256_H_

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <stdint.h>
    #include <stddef.h>
    #include <mbedtls/cmac.h>
    #include <mbedtls/md.h>
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include <tinycrypt/sha256.h>
    #include <tinycrypt/utils.h>
    #include <tinycrypt/constants.h>
    #include <tinycrypt/hmac.h>
#endif /* MCUBOOT_USE_TINYCRYPT */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef struct tc_hmac_state_struct bootutil_hmac_sha256_context;
static inline void bootutil_hmac_sha256_init(bootutil_hmac_sha256_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_hmac_sha256_drop(bootutil_hmac_sha256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_hmac_sha256_set_key(bootutil_hmac_sha256_context *ctx, const uint8_t *key, unsigned int key_size)
{
    int rc;
    rc = tc_hmac_set_key(ctx, key, key_size);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    rc = tc_hmac_init(ctx);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

static inline int bootutil_hmac_sha256_update(bootutil_hmac_sha256_context *ctx, const void *data, unsigned int data_length)
{
    int rc;
    rc = tc_hmac_update(ctx, data, data_length);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

static inline int bootutil_hmac_sha256_finish(bootutil_hmac_sha256_context *ctx, uint8_t *tag, unsigned int taglen)
{
    int rc;
    rc = tc_hmac_final(tag, taglen, ctx);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_MBED_TLS) && !defined(MCUBOOT_USE_PSA_CRYPTO)
/**
 * The generic message-digest context.
 */
typedef mbedtls_md_context_t bootutil_hmac_sha256_context;

static inline void bootutil_hmac_sha256_init(bootutil_hmac_sha256_context *ctx)
{
    mbedtls_md_init(ctx);
}

static inline void bootutil_hmac_sha256_drop(bootutil_hmac_sha256_context *ctx)
{
    mbedtls_md_free(ctx);
}

static inline int bootutil_hmac_sha256_set_key(bootutil_hmac_sha256_context *ctx, const uint8_t *key, unsigned int key_size)
{
    int rc;

    rc = mbedtls_md_setup(ctx, mbedtls_md_info_from_string("SHA256"), 1);
    if (rc != 0) {
        return rc;
    }
    rc = mbedtls_md_hmac_starts(ctx, key, key_size);
    return rc;
}

static inline int bootutil_hmac_sha256_update(bootutil_hmac_sha256_context *ctx, const void *data, unsigned int data_length)
{
    return mbedtls_md_hmac_update(ctx, data, data_length);
}

static inline int bootutil_hmac_sha256_finish(bootutil_hmac_sha256_context *ctx, uint8_t *tag, unsigned int taglen)
{
    (void)taglen;
    /*
     * HMAC the key and check that our received MAC matches the generated tag
     */
    return mbedtls_md_hmac_finish(ctx, tag);
}
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_PSA_CRYPTO)
/**
 * The generic message-digest context.
 */
typedef struct
{
    psa_key_handle_t key_handle;
    psa_mac_operation_t operation;
} bootutil_hmac_sha256_context;

static inline void bootutil_hmac_sha256_init(bootutil_hmac_sha256_context *ctx)
{
    ctx->operation = psa_mac_operation_init();
}

static inline void bootutil_hmac_sha256_drop(bootutil_hmac_sha256_context *ctx)
{
    psa_mac_abort(&ctx->operation);
    psa_destroy_key(ctx->key_handle);
}

static inline int bootutil_hmac_sha256_set_key(bootutil_hmac_sha256_context *ctx, const uint8_t *key, unsigned int key_size)
{
    psa_status_t status;
    psa_key_attributes_t key_attributes = psa_key_attributes_init();

    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&key_attributes, 256u);
    
    status = psa_import_key(&key_attributes, key, key_size, &ctx->key_handle);

    if (status == PSA_SUCCESS) {
        status = psa_mac_sign_setup(&ctx->operation, ctx->key_handle, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    }

    if (status != PSA_SUCCESS) {
        return -1;
    }

    return 0;
}

static inline int bootutil_hmac_sha256_update(bootutil_hmac_sha256_context *ctx, const void *data, unsigned int data_length)
{
    psa_status_t status;

    status = psa_mac_update(&ctx->operation, data, data_length);
    
    if (status != PSA_SUCCESS) {
        return -1;
    }

    return 0;
}

static inline int bootutil_hmac_sha256_finish(bootutil_hmac_sha256_context *ctx, uint8_t *tag, unsigned int taglen)
{
    size_t output_len;
    psa_status_t status;

    status = psa_mac_sign_finish(&ctx->operation, tag, taglen, &output_len);

    if (status != PSA_SUCCESS) {
        return -1;
    }

    return 0;
}
#endif /* MCUBOOT_USE_MBED_TLS */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_HMAC_SHA256_H_ */
