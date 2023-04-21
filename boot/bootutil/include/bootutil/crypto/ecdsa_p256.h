/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

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
    #define BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE (4 * 8)
    /* Indicate to the caller that the verify function needs the raw ASN.1
     * signature, not a decoded one. */
    #define MCUBOOT_ECDSA_NEED_ASN1_SIG
#endif /* MCUBOOT_USE_MBED_TLS */

/*TODO: remove this after cypress port mbedtls to abstract crypto api */
#if defined(MCUBOOT_USE_CC310) || defined(MCUBOOT_USE_MBED_TLS)
#define NUM_ECC_BYTES (256 / 8)
#endif

#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"
#include "bootutil/sign_key.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaring these like this adds NULL termination.
 */
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

#ifndef MCUBOOT_ECDSA_NEED_ASN1_SIG
/*
 * cp points to ASN1 string containing an integer.
 * Verify the tag, and that the length is 32 bytes.
 */
static int bootutil_read_bigint(uint8_t i[NUM_ECC_BYTES], uint8_t **cp, uint8_t *end)
{
    size_t len;

    if (mbedtls_asn1_get_tag(cp, end, &len, MBEDTLS_ASN1_INTEGER)) {
        return -3;
    }

    if (len >= NUM_ECC_BYTES) {
        memcpy(i, *cp + len - NUM_ECC_BYTES, NUM_ECC_BYTES);
    } else {
        memset(i, 0, NUM_ECC_BYTES - len);
        memcpy(i + NUM_ECC_BYTES - len, *cp, len);
    }
    *cp += len;
    return 0;
}

/*
 * Read in signature. Signature has r and s encoded as integers.
 */
static int bootutil_decode_sig(uint8_t signature[NUM_ECC_BYTES * 2], uint8_t *cp, uint8_t *end)
{
    int rc;
    size_t len;

    rc = mbedtls_asn1_get_tag(&cp, end, &len,
                              MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (rc) {
        return -1;
    }
    if (cp + len > end) {
        return -2;
    }

    rc = bootutil_read_bigint(signature, &cp, end);
    if (rc) {
        return -3;
    }
    rc = bootutil_read_bigint(signature + NUM_ECC_BYTES, &cp, end);
    if (rc) {
        return -4;
    }
    return 0;
}
#endif /* not MCUBOOT_ECDSA_NEED_ASN1_SIG */

static int bootutil_import_key(uint8_t **cp, uint8_t *end)
{
    size_t len;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if (mbedtls_asn1_get_tag(cp, end, &len,
        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        return -1;
    }
    end = *cp + len;

    /* ECParameters (RFC5480) */
    if (mbedtls_asn1_get_alg(cp, end, &alg, &param)) {
        return -2;
    }
    /* id-ecPublicKey (RFC5480) */
    if (alg.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.MBEDTLS_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -3;
    }
    /* namedCurve (RFC5480) */
    if (param.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_secp256r1_oid) - 1 ||
        memcmp(param.MBEDTLS_CONTEXT_MEMBER(p), ec_secp256r1_oid, sizeof(ec_secp256r1_oid) - 1)) {
        return -4;
    }
    /* ECPoint (RFC5480) */
    if (mbedtls_asn1_get_bitstring_null(cp, end, &len)) {
        return -6;
    }
    if (*cp + len != end) {
        return -7;
    }

    if (len != 2 * NUM_ECC_BYTES + 1) {
        return -8;
    }

    return 0;
}

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
                                             uint8_t *pk, size_t pk_len,
                                             uint8_t *hash, size_t hash_len,
                                             uint8_t *sig, size_t sig_len)
{
    int rc;
    (void)ctx;
    (void)pk_len;
    (void)sig_len;
    (void)hash_len;

    uint8_t signature[2 * NUM_ECC_BYTES];
    rc = bootutil_decode_sig(signature, sig, sig + sig_len);
    if (rc) {
        return -1;
    }

    /* Only support uncompressed keys. */
    if (pk[0] != 0x04) {
        return -1;
    }
    pk++;

    rc = uECC_verify(pk, hash, BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE, signature, uECC_secp256r1());
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}

static inline int bootutil_ecdsa_p256_parse_public_key(bootutil_ecdsa_p256_context *ctx,
                                                       uint8_t **cp,uint8_t *end)
{
    (void)ctx;
    return bootutil_import_key(cp, end);
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
                                             uint8_t *hash, size_t hash_len,
                                             uint8_t *sig, size_t sig_len)
{
    (void)ctx;
    (void)pk_len;
    (void)sig_len;
    (void)hash_len;

    /* Only support uncompressed keys. */
    if (pk[0] != 0x04) {
        return -1;
    }
    pk++;

    return cc310_ecdsa_verify_secp256r1(hash, pk, sig, BOOTUTIL_CRYPTO_ECDSA_P256_HASH_SIZE);
}

static inline int bootutil_ecdsa_p256_parse_public_key(bootutil_ecdsa_p256_context *ctx,
                                                       uint8_t **cp,uint8_t *end)
{
    (void)ctx;
    return bootutil_import_key(cp, end);
}
#endif /* MCUBOOT_USE_CC310 */

#if defined(MCUBOOT_USE_MBED_TLS)

typedef mbedtls_ecdsa_context bootutil_ecdsa_p256_context;
static inline void bootutil_ecdsa_p256_init(bootutil_ecdsa_p256_context *ctx)
{
    mbedtls_ecdsa_init(ctx);
}

static inline void bootutil_ecdsa_p256_drop(bootutil_ecdsa_p256_context *ctx)
{
    mbedtls_ecdsa_free(ctx);
}

#ifdef CY_MBEDTLS_HW_ACCELERATION
/*
 * Parse the public key used for signing.
 */
static int bootutil_parse_eckey(bootutil_ecdsa_p256_context *ctx, uint8_t **p, uint8_t *end)
{
    size_t len;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if (mbedtls_asn1_get_tag(p, end, &len,
        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        return -1;
    }
    end = *p + len;

    if (mbedtls_asn1_get_alg(p, end, &alg, &param)) {
        return -2;
    }
    if (alg.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
      memcmp(alg.MBEDTLS_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -3;
    }
    if (param.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_secp256r1_oid) - 1||
      memcmp(param.MBEDTLS_CONTEXT_MEMBER(p), ec_secp256r1_oid, sizeof(ec_secp256r1_oid) - 1)) {
        return -4;
    }

    if (mbedtls_ecp_group_load(&ctx->grp, MBEDTLS_ECP_DP_SECP256R1)) {
        return -5;
    }

    if (mbedtls_asn1_get_bitstring_null(p, end, &len)) {
        return -6;
    }
    if (*p + len != end) {
        return -7;
    }

    if (mbedtls_ecp_point_read_binary(&ctx->grp, &ctx->Q, *p, end - *p)) {
        return -8;
    }

    if (mbedtls_ecp_check_pubkey(&ctx->grp, &ctx->Q)) {
        return -9;
    }
    return 0;
}

static inline int bootutil_ecdsa_p256_verify(bootutil_ecdsa_p256_context *ctx,
                                             uint8_t *pk, size_t pk_len,
                                             uint8_t *hash, size_t hash_len,
                                             uint8_t *sig, size_t sig_len)
{
    (void)pk;
    (void)pk_len;

    /*
     * This is simplified, as the hash length is also 32 bytes.
     */
    while (sig[sig_len - 1] == '\0') {
            sig_len--;
        }
    return mbedtls_ecdsa_read_signature(&ctx, hash, hash_len, sig, sig_len);
}

#else /* CY_MBEDTLS_HW_ACCELERATION */

static inline int bootutil_ecdsa_p256_verify(bootutil_ecdsa_p256_context *ctx,
                                             uint8_t *pk, size_t pk_len,
                                             uint8_t *hash, size_t hash_len,
                                             uint8_t *sig, size_t sig_len)
{
    int rc;

    (void)sig;
    (void)hash;
    (void)hash_len;

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

#endif /* CY_MBEDTLS_HW_ACCELERATION */

static inline int bootutil_ecdsa_p256_parse_public_key(bootutil_ecdsa_p256_context *ctx,
                                                       uint8_t **cp,uint8_t *end)
{
    int rc;
#ifdef CY_MBEDTLS_HW_ACCELERATION
    rc = bootutil_parse_eckey(&ctx, cp, end);
#else
    (void)ctx;
    rc = bootutil_import_key(cp, end);
#endif
    return rc;
}

#endif /* MCUBOOT_USE_MBED_TLS */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_ECDSA_P256_H_ */
