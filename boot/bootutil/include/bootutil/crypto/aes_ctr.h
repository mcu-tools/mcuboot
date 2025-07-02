/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, the choices are: MCUBOOT_USE_MBED_TLS, MCUBOOT_USE_TINYCRYPT
 * or MCUBOOT_USE_PSA_CRYPTO. Since MCUBOOT_USE_PSA_CRYPTO does not yet
 * support all the same abstraction as MCUBOOT_USE_MBED_TLS, the support for
 * PSA Crypto is built on top of mbed TLS, i.e. they must be both defined
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_H_

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO) || defined(MCUBOOT_USE_MBED_TLS)
#define MCUBOOT_USE_PSA_OR_MBED_TLS
#endif /* MCUBOOT_USE_PSA_CRYPTO || MCUBOOT_USE_MBED_TLS */

#if (defined(MCUBOOT_USE_PSA_OR_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT) ) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT or PSA"
#endif


#include "bootutil/enc_key_public.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO)
    #include <psa/crypto.h>
    #include "bootutil/enc_key_public.h"
    #define BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE BOOT_ENC_KEY_SIZE
    #define BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE (16)
#elif defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/aes.h>
    #define BOOT_ENC_BLOCK_SIZE (16)
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include <string.h>
    #include <tinycrypt/aes.h>
    #include <tinycrypt/ctr_mode.h>
    #include <tinycrypt/constants.h>
    #if defined(MCUBOOT_AES_256) || (BOOT_ENC_KEY_SIZE != TC_AES_KEY_SIZE)
        #error "Cannot use AES-256 for encryption with Tinycrypt library."
    #endif
    #define BOOT_ENC_BLOCK_SIZE TC_AES_BLOCK_SIZE
#endif /* MCUBOOT_USE_TINYCRYPT */


#include <stdint.h>
#include "bootutil/bootutil_log.h"


#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_PSA_CRYPTO)
typedef struct {
    psa_cipher_operation_t op;
    psa_key_id_t key_id;
    uint8_t key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE];
} bootutil_aes_ctr_context;

static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
    ctx->op = psa_cipher_operation_init();
    ctx->key_id = PSA_KEY_ID_NULL;
    memset(ctx->key, 0, sizeof(ctx->key));
}

static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx)
{
    (void)psa_cipher_abort(&(ctx->op));
    memset(ctx->key, 0, sizeof(ctx->key));
    (void)psa_destroy_key(ctx->key_id);
    ctx->key_id = PSA_KEY_ID_NULL;
}

static inline int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *k)
{
    memcpy(ctx->key, k, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE);
    return 0;
}

static int import_key_for_ctr(bootutil_aes_ctr_context *ctx)
{
    psa_status_t status = PSA_ERROR_INVALID_ARGUMENT;
    psa_key_attributes_t key_attributes = psa_key_attributes_init();
    psa_key_usage_t usage = (PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);

    /* Setup the key policy */
    psa_set_key_usage_flags(&key_attributes, usage);
    psa_set_key_algorithm(&key_attributes, PSA_ALG_CTR);
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);

    /* Import a key */
    status = psa_import_key(&key_attributes, ctx->key, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE,
                            &(ctx->key_id));
    return (int)status;
}

static int set_iv_for_ctr(bootutil_aes_ctr_context *ctx, uint8_t *counter)
{
    psa_status_t status = PSA_ERROR_INVALID_ARGUMENT;
    ctx->op = psa_cipher_operation_init();

    /* Setup the object always as encryption for CTR*/
    status = psa_cipher_encrypt_setup(&(ctx->op), ctx->key_id, PSA_ALG_CTR);
    if (status != PSA_SUCCESS) {
        return (int)status;
    }

    status = psa_cipher_set_iv(&(ctx->op), counter, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    return (int)status;
}

static inline int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c)
{
    psa_status_t status = PSA_ERROR_INVALID_ARGUMENT;
    (void)counter; (void)blk_off; /* These are handled by the API */
    size_t output_length = 0;
    size_t clen = mlen;

    status = import_key_for_ctr(ctx);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = set_iv_for_ctr(ctx, counter);
    if (status != PSA_SUCCESS) {
        return status;
    }

    if (!(mlen % BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE)) {
        status = psa_cipher_update(&(ctx->op), m, mlen, c, clen, &output_length);
    } else {
        /* Partial blocks and overlapping input/outputs might lead to unexpected
         * failures in mbed TLS, so treat them separately. Details available at
         * https://github.com/Mbed-TLS/mbedtls/issues/3266
         */
        size_t len_aligned = (mlen/BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE)*BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE;
        status = psa_cipher_update(&(ctx->op), m, len_aligned, c, clen, &output_length);
        if (status != PSA_SUCCESS) {
            goto ret_val;
        }
        size_t remaining_items = mlen % 16;
        uint8_t last_output[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
        size_t last_output_length = 0;
        status = psa_cipher_update(&(ctx->op), &m[len_aligned], remaining_items,
                                   last_output, sizeof(last_output), &last_output_length);
        if (status != PSA_SUCCESS) {
            goto ret_val;
        }
        memcpy(&c[len_aligned], last_output, remaining_items);
        output_length += last_output_length;
    }

ret_val:
    /* Note that counter is not updated in this API while it's updated in the
     * mbed TLS API. This means that the caller can't rely on the value of it
     * if compatibility with different API implementations wants to be kept
     */
    psa_cipher_abort(&(ctx->op));
    psa_destroy_key(ctx->key_id);
    ctx->key_id = PSA_KEY_ID_NULL;

    return (int)status;
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
    return bootutil_aes_ctr_encrypt(ctx, counter, c, clen, blk_off, m);
}

#elif defined(MCUBOOT_USE_MBED_TLS)
typedef mbedtls_aes_context bootutil_aes_ctr_context;
static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
    (void)mbedtls_aes_init(ctx);
}

static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx)
{
    mbedtls_aes_free(ctx);
}

static inline int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *k)
{
    return mbedtls_aes_setkey_enc(ctx, k, BOOT_ENC_KEY_SIZE * 8);
}

static inline int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c)
{
    uint8_t stream_block[BOOT_ENC_BLOCK_SIZE];
    return mbedtls_aes_crypt_ctr(ctx, mlen, &blk_off, counter, stream_block, m, c);
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
    uint8_t stream_block[BOOT_ENC_BLOCK_SIZE];
    return mbedtls_aes_crypt_ctr(ctx, clen, &blk_off, counter, stream_block, c, m);
}
#endif /* MCUBOOT_USE_MBED_TLS */

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

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_H_ */
