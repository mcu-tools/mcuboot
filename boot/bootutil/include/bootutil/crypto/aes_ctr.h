/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_H_

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/aes.h>
    #define BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE (16)
    #define BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE (16)
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
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

#if defined(MCUBOOT_USE_MBED_TLS)
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
    int rc;
    rc = mbedtls_aes_crypt_ctr(ctx, mlen, &blk_off, counter, stream_block, m, c);
    memset(stream_block, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    return rc;
}

static inline int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter, const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
    uint8_t stream_block[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    int rc;
    rc = mbedtls_aes_crypt_ctr(ctx, clen, &blk_off, counter, stream_block, c, m);
    memset(stream_block, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    return rc;
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
    uint8_t buf[16];
    uint32_t buflen;
    int rc;
    if (blk_off == 0) {
        rc = tc_ctr_mode(out, inlen, in, inlen, counter, ctx);
        if (rc != TC_CRYPTO_SUCCESS) {
            return -1;
        }
    } else if (blk_off < 16) {
        buflen = ((inlen + blk_off <= 16) ? inlen : (16 - blk_off));
        inlen -= buflen;
        memcpy(&buf[blk_off], &in[0], buflen);
        rc = tc_ctr_mode(buf, 16, buf, 16, counter, ctx);
        if (rc != TC_CRYPTO_SUCCESS) {
            return -1;
        }
        memcpy(&out[0], &buf[blk_off], buflen);
        memset(&buf[0], 0, 16);
        if (inlen > 0) {
            rc = tc_ctr_mode(&out[buflen], inlen, &in[buflen], inlen, counter, ctx);
        }
        if (rc != TC_CRYPTO_SUCCESS) {
            return -1;
        }
    } else {
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
