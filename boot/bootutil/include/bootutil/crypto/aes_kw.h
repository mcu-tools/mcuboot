/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_KW_H_
#define __BOOTUTIL_CRYPTO_AES_KW_H_

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO) || defined(MCUBOOT_USE_MBED_TLS)
#define MCUBOOT_USE_PSA_OR_MBED_TLS
#endif /* MCUBOOT_USE_PSA_CRYPTO || MCUBOOT_USE_MBED_TLS */


#if (defined(MCUBOOT_USE_PSA_OR_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_PSA_OR_MBED_TLS)
#include <psa/crypto.h>
#endif

#if defined(MCUBOOT_USE_TINYCRYPT)
    #if defined(MCUBOOT_AES_256)
        #error "Cannot use AES-256 for encryption with Tinycrypt library."
    #endif
    #include <tinycrypt/aes.h>
    #include <tinycrypt/constants.h>
#endif /* MCUBOOT_USE_TINYCRYPT */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#if defined(MCUBOOT_USE_PSA_OR_MBED_TLS)

#define BOOTUTIL_CRYPTO_AES_KW_KEY_SIZE 32

typedef struct {
    psa_cipher_operation_t op;
    psa_key_id_t key_id;
    psa_key_attributes_t key_attributes;
} bootutil_aes_kw_context;

static inline void bootutil_aes_kw_init(bootutil_aes_kw_context *ctx)
{
    ctx->op = psa_cipher_operation_init();
    ctx->key_id = PSA_KEY_ID_NULL;
    ctx->key_attributes = psa_key_attributes_init();
    return;
}

static inline void bootutil_aes_kw_drop(bootutil_aes_kw_context *ctx)
{
    memset(ctx, 0, sizeof(bootutil_aes_kw_context));
}

static inline int bootutil_aes_kw_set_unwrap_key(bootutil_aes_kw_context *ctx, psa_key_id_t key_id)
{
    ctx->key_id = key_id;
    return 0;
}

static inline int bootutil_aes_kw_unwrap(bootutil_aes_kw_context *ctx, const uint8_t *wrapped_key, uint32_t wrapped_key_len, uint8_t *key, uint32_t key_len)
{
    psa_status_t status = PSA_ERROR_INVALID_ARGUMENT;
    psa_key_id_t output_key_id = PSA_KEY_ID_NULL;
    size_t output_key_len = 0;
    psa_key_attributes_t key_attributes;

    psa_set_key_algorithm(&key_attributes, PSA_ALG_CTR);
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
    psa_set_key_usage_flags(&key_attributes, (PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT));

    status = psa_unwrap_key(&key_attributes, ctx->key_id, PSA_ALG_ECB_NO_PADDING,
                             wrapped_key, wrapped_key_len, &output_key_id);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_export_key(output_key_id, key, key_len, &output_key_len);
    if (status != PSA_SUCCESS) {
        return status;
    }

    return status;
}

#endif /* MCUBOOT_USE_PSA_OR_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef struct tc_aes_key_sched_struct bootutil_aes_kw_context;
static inline void bootutil_aes_kw_init(bootutil_aes_kw_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_aes_kw_drop(bootutil_aes_kw_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_aes_kw_set_unwrap_key(bootutil_aes_kw_context *ctx, const uint8_t *k, uint32_t klen)
{
    int rc;

    if (klen != 16) {
        return -1;
    }

    rc = tc_aes128_set_decrypt_key(ctx, k);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

/*
 * Implements AES key unwrapping following RFC-3394 section 2.2.2, using
 * tinycrypt for AES-128 decryption.
 */
static int bootutil_aes_kw_unwrap(bootutil_aes_kw_context *ctx, const uint8_t *wrapped_key, uint32_t wrapped_key_len, uint8_t *key, uint32_t key_len)
{
    uint8_t A[8];
    uint8_t B[16];
    int8_t i, j, k;

    if (wrapped_key_len != 24 || key_len != 16) {
        return -1;
    }

    for (k = 0; k < 8; k++) {
        A[k]       = wrapped_key[k];
        key[k]     = wrapped_key[8 + k];
        key[8 + k] = wrapped_key[16 + k];
    }

    for (j = 5; j >= 0; j--) {
        for (i = 2; i > 0; i--) {
            for (k = 0; k < 8; k++) {
                B[k] = A[k];
                B[8 + k] = key[((i-1) * 8) + k];
            }
            B[7] ^= 2 * j + i;
            if (tc_aes_decrypt((uint8_t *)&B, (uint8_t *)&B, ctx) != TC_CRYPTO_SUCCESS) {
                return -1;
            }
            for (k = 0; k < 8; k++) {
                A[k] = B[k];
                key[((i-1) * 8) + k] = B[8 + k];
            }
        }
    }

    for (i = 0, k = 0; i < 8; i++) {
        k |= A[i] ^ 0xa6;
    }
    if (k) {
        return -1;
    }

    return 0;
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_AES_KW_H_ */
