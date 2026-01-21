/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_PSA_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_PSA_H_

#include <string.h>
#include <stdint.h>
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/enc_key_public.h"
#include <psa/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif
#define BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE BOOT_ENC_KEY_SIZE
#define BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE (16)
typedef struct {
    psa_cipher_operation_t op;
    psa_key_id_t key_id;
    uint8_t key[BOOT_ENC_KEY_SIZE];
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

static inline int import_key_for_ctr(bootutil_aes_ctr_context *ctx)
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

static inline int set_iv_for_ctr(bootutil_aes_ctr_context *ctx, uint8_t *counter)
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

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_H_ */
