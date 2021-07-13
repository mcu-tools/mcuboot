/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_ECDSA_P256_H_
#define __BOOTUTIL_CRYPTO_ECDSA_P256_H_

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_TINYCRYPT) + \
     defined(MCUBOOT_USE_CC310) + \
     defined(MCUBOOT_USE_MBED_TLS)) != 1
    #error "One crypto backend must be defined: either CC310, TINYCRYPT, or MBED_TLS"
#endif

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include <tinycrypt/ecc_dsa.h>
    #include <tinycrypt/constants.h>
    #define BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE (4 * 8)
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
    #include <cc310_glue.h>
    #define BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE (4 * 8)
#endif /* MCUBOOT_USE_CC310 */

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/ecdsa.h>
    #include "bootutil/crypto/common.h"
    #define BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE (4 * 8)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef uintptr_t bootutil_ecdsa_p256_context;
static inline void bootutil_ecdsa_p256_init(bootutil_ecdsa_p256_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_ecdsa_p256_drop(bootutil_ecdsa_p256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_ecdsa_p256_verify(bootutil_ecdsa_p256_context *ctx,
                                             const uint8_t *pk, size_t pk_len,
                                             const uint8_t *hash,
                                             const uint8_t *sig, size_t sig_len)
{
    int rc;
    (void)ctx;
    (void)pk_len;
    (void)sig_len;

    /* Only support uncompressed keys. */
    if (pk[0] != 0x04) {
        return -1;
    }
    pk++;

    rc = uECC_verify(pk, hash, BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE, sig, uECC_secp256r1());
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_CC310)
typedef uintptr_t bootutil_ecdsa_p256_context;
static inline void bootutil_ecdsa_p256_init(bootutil_ecdsa_p256_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_ecdsa_p256_drop(bootutil_ecdsa_p256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_ecdsa_p256_verify(bootutil_ecdsa_p256_context *ctx,
                                             uint8_t *pk, size_t pk_len,
                                             uint8_t *hash,
                                             uint8_t *sig, size_t sig_len)
{
    (void)ctx;
    (void)pk_len;
    (void)sig_len;

    /* Only support uncompressed keys. */
    if (pk[0] != 0x04) {
        return -1;
    }
    pk++;

    return cc310_ecdsa_verify_secp256r1(hash, pk, sig, BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE);
}
#endif /* MCUBOOT_USE_CC310 */

#if defined(MCUBOOT_USE_MBED_TLS)

/* Indicate to the caller that the verify function needs the raw ASN.1
 * signature, not a decoded one. */
#define MCUBOOT_ECDSA_NEED_ASN1_SIG

typedef mbedtls_ecdsa_context bootutil_ecdsa_p256_context;
static inline void bootutil_ecdsa_p256_init(bootutil_ecdsa_p256_context *ctx)
{
    mbedtls_ecdsa_init(ctx);
}

static inline void bootutil_ecdsa_p256_drop(bootutil_ecdsa_p256_context *ctx)
{
    mbedtls_ecdsa_free(ctx);
}

static inline int bootutil_ecdsa_p256_verify(bootutil_ecdsa_p256_context *ctx,
                                             uint8_t *pk, size_t pk_len,
                                             uint8_t *hash,
                                             uint8_t *sig, size_t sig_len)
{
    int rc;

    (void)sig;
    (void)hash;

    rc = mbedtls_ecp_group_load(&ctx->MBEDTLS_CONTEXT_MEMBER(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (rc) {
        return -1;
    }

    rc = mbedtls_ecp_point_read_binary(&ctx->MBEDTLS_CONTEXT_MEMBER(grp), &ctx->MBEDTLS_CONTEXT_MEMBER(Q), pk, pk_len);
    if (rc) {
        return -1;
    }

    rc = mbedtls_ecp_check_pubkey(&ctx->MBEDTLS_CONTEXT_MEMBER(grp), &ctx->MBEDTLS_CONTEXT_MEMBER(Q));
    if (rc) {
        return -1;
    }

    rc = mbedtls_ecdsa_read_signature(ctx, hash, BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE,
                                      sig, sig_len);
    if (rc) {
        return -1;
    }

    return 0;
}
#endif /* MCUBOOT_USE_MBED_TLS */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_ECDSA_P256_H_ */
