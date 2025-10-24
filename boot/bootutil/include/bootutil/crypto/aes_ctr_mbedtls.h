/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_MBEDTLS_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_MBEDTLS_H_

#include <string.h>
#include <stdint.h>
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/enc_key_public.h"
#include <mbedtls/aes.h>

#define BOOT_ENC_BLOCK_SIZE (16)

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_MBEDTLS_H_ */
