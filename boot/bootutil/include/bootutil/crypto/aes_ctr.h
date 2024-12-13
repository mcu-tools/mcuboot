/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef BOOTUTIL_CRYPTO_AES_CTR_H
#define BOOTUTIL_CRYPTO_AES_CTR_H

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/aes.h>
    #include "bootutil/enc_key_public.h"
    #define BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE BOOT_ENC_KEY_SIZE
    #define BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE (16U)
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #if defined(MCUBOOT_AES_256)
        #error "Cannot use AES-256 for encryption with Tinycrypt library."
    #endif
    #include <string.h>
    #include <tinycrypt/aes.h>
    #include <tinycrypt/ctr_mode.h>
    #include <tinycrypt/constants.h>
    #define BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE TC_AES_KEY_SIZE
    #define BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE TC_AES_BLOCK_SIZE
#endif /* MCUBOOT_USE_TINYCRYPT */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_MBED_TLS) && !defined(MCUBOOT_USE_PSA_CRYPTO)
typedef mbedtls_aes_context bootutil_aes_ctr_context;
static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
   (void)mbedtls_aes_init(ctx);
}

static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx)
{
   /* XXX: config defines MBEDTLS_PLATFORM_NO_STD_FUNCTIONS so no need to free */
   /* (void)mbedtls_aes_free(ctx); */
   (void)ctx;
}

static inline int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *k)
{
   return mbedtls_aes_setkey_enc(ctx, k, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE * 8);
}

static inline int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c)
{
   uint8_t stream_block[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
   (void)memset(&stream_block, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
   return mbedtls_aes_crypt_ctr(ctx, mlen, &blk_off, counter, stream_block, m, c);
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
   uint8_t stream_block[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
   (void)memset(&stream_block, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
   return mbedtls_aes_crypt_ctr(ctx, clen, &blk_off, counter, stream_block, c, m);
}
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_PSA_CRYPTO)
#include "crypto.h"
#include "crypto_values.h"

typedef struct
{
    psa_key_handle_t key_handle;
    psa_cipher_operation_t operation;
} bootutil_aes_ctr_context;

static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
    ctx->operation = psa_cipher_operation_init();
}

static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx)
{
    psa_cipher_abort(&ctx->operation);
}

static inline int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *key)
{
    psa_status_t status;

    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_algorithm(&key_attributes, PSA_ALG_CTR);
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_ENCRYPT);

    status = psa_import_key(&key_attributes, key, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE, &ctx->key_handle);

    if (status != PSA_SUCCESS) {
        return -1;
    }

    return 0;
}

static inline int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c)
{
    (void)blk_off;

    psa_status_t status;
    size_t out_sz;
    size_t f_sz;

    ctx->operation = psa_cipher_operation_init();

    status = psa_cipher_encrypt_setup(&ctx->operation, ctx->key_handle, PSA_ALG_CTR);

    if (status == PSA_SUCCESS) {
        status = psa_cipher_set_iv(&ctx->operation, counter, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE);
    }

    if (status == PSA_SUCCESS) {
        status = psa_cipher_update(&ctx->operation, m, mlen, c, mlen, &out_sz);
    }

    if (status == PSA_SUCCESS) {
        status = psa_cipher_finish(&ctx->operation, c + out_sz, sizeof(mlen) - out_sz, &f_sz);
    }

    if ((status != PSA_SUCCESS) || ((out_sz + f_sz) != mlen)) {
        return -1;
    }

    return 0;
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
    (void)blk_off;

    psa_status_t status;
    size_t out_sz;
    size_t f_sz;

    ctx->operation = psa_cipher_operation_init();

    status = psa_cipher_decrypt_setup(&ctx->operation, ctx->key_handle, PSA_ALG_CTR);

    if (status == PSA_SUCCESS) {
        status = psa_cipher_set_iv(&ctx->operation, counter, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE);
    }

    if (status == PSA_SUCCESS) {
        status = psa_cipher_update(&ctx->operation, c, clen, m, clen, &out_sz);
    }

    if (status == PSA_SUCCESS) {
        status = psa_cipher_finish(&ctx->operation, m + out_sz, sizeof(clen) - out_sz, &f_sz);
    }

    if ((status != PSA_SUCCESS) || ((out_sz + f_sz) != clen)) {
        return -1;
    }

    return 0;
}
#endif /* MCUBOOT_USE_PSA_CRYPTO */

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef struct tc_aes_key_sched_struct bootutil_aes_ctr_context;
static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *k)
{
    int rc;
    rc = tc_aes128_set_encrypt_key(ctx, k);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

static int _bootutil_aes_ctr_crypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *in, uint32_t inlen, uint32_t blk_off, uint8_t *out)
{
    int rc;
    rc = tc_ctr_mode(out, inlen, in, inlen, counter, &blk_off, ctx);
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

static inline int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *m, uint32_t mlen, uint32_t blk_off, uint8_t *c)
{
    return _bootutil_aes_ctr_crypt(ctx, counter, m, mlen, blk_off, c);
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, uint32_t blk_off, uint8_t *m)
{
    return _bootutil_aes_ctr_crypt(ctx, counter, c, clen, blk_off, m);
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#ifdef __cplusplus
}
#endif

#endif /* BOOTUTIL_CRYPTO_AES_CTR_H */
