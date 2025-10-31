/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
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

typedef struct {
    psa_key_id_t key;
} bootutil_aes_ctr_context;

void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx);

void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx);
int bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx, const uint8_t *k);

int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter,
                             const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c);
int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter,
                             const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_H_ */
