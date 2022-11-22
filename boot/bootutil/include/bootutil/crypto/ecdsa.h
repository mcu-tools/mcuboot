/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, the choice is: MCUBOOT_USE_PSA_CRYPTO
 * Note that support for MCUBOOT_USE_PSA_CRYPTO is still experimental and it
 * might not support all the crypto abstractions that MCUBOOT_USE_MBED_TLS
 * supports. For this reason, it's allowed to have both of them defined, and
 * for crypto modules that support both abstractions, the
 * MCUBOOT_USE_PSA_CRYPTO will take precedence.
 */

#ifndef __BOOTUTIL_CRYPTO_ECDSA_H_
#define __BOOTUTIL_CRYPTO_ECDSA_H_

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO)
#define MCUBOOT_USE_PSA_OR_MBED_TLS
#endif /* MCUBOOT_USE_PSA_CRYPTO  */

#if (defined(MCUBOOT_USE_PSA_OR_MBED_TLS)) != 1
    #error "One crypto backend must be defined: PSA_CRYPTO"
#endif

#if defined(MCUBOOT_USE_PSA_CRYPTO)
    #include <psa/crypto.h>
    #include <string.h>
#endif /* MCUBOOT_USE_PSA_CRYPTO */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_PSA_CRYPTO)
// OID id-ecPublicKey 1.2.840.10045.2.1.
const uint8_t IdEcPublicKey[] = {0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
// OID secp256r1 1.2.840.10045.3.1.7.
const uint8_t Secp256r1[] = {0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
// OID secp384r1 1.3.132.0.34
const uint8_t Secp384r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22};

typedef struct {
    psa_key_id_t key_id;
    size_t curve_byte_count;
    psa_algorithm_t required_algorithm;
} bootutil_ecdsa_context;

static inline void bootutil_ecdsa_init(bootutil_ecdsa_context *ctx)
{
    ctx->key_id = PSA_KEY_ID_NULL;
    ctx->curve_byte_count = 0;
    ctx->required_algorithm = 0;
}

static inline void bootutil_ecdsa_drop(bootutil_ecdsa_context *ctx)
{
    if (ctx->key_id != PSA_KEY_ID_NULL) {
        (void)psa_destroy_key(ctx->key_id);
    }
}

/* ECDSA public key with format specified in RFC5280 et al. in ASN-1 syntax
 *
 * SEQUENCE {
 *    SEQUENCE {
 *        OBJECT idEcPublicKey
 *        OBJECT namedCurve
 *    }
 *    BIT STRING publicKey
 * }
 *
 * ECDSA signature format specified in RFC3279 et al. in ASN-1 syntax
 *
 * SEQUENCE  {
 *    INTEGER r
 *    INTEGER s
 * }
 *
 */

/* Offset in bytes from the start of the encoding to the length
 * of the innermost SEQUENCE
 */
#define PUB_KEY_LEN_OFF (3)
/* Offset in bytes from the start of the publicKey encoding of the BIT STRING */
#define PUB_KEY_VAL_OFF (3)
/* Computes the pointer to the idEcPublicKey OID from the base of the encoding */
#define PUB_KEY_OID_OFFSET(p) (*p + PUB_KEY_LEN_OFF+1)
/* Computes the pointer to the namedCurve OID from the base of the encoding */
#define CURVE_TYPE_OID_OFFSET(p) PUB_KEY_OID_OFFSET(p) + sizeof(IdEcPublicKey)

/* This helper function gets a pointer to the bitstring associated to the publicKey
 * as encoded per RFC 5280. This function assumes that the public key encoding is not
 * bigger than 127 bytes (i.e. usually up until 384 bit curves)
 *
 * \param[in,out] p    Double pointer to a buffer containing the RFC 5280 of the ECDSA public key.
 *                     On output, the pointer is updated to point to the start of the public key
 *                     in BIT STRING form.
 * \param[out]    size Pointer to a buffer containing the size of the public key extracted
 *
 */
static inline void get_public_key_from_rfc5280_encoding(uint8_t **p, size_t *size)
{
    uint8_t *key_start = (*p) + (PUB_KEY_LEN_OFF + 1 + (*p)[PUB_KEY_LEN_OFF] + PUB_KEY_VAL_OFF);
    *p = key_start;
    *size = key_start[-2]-1; /* -2 from PUB_KEY_VAL_OFF to get the length, -1 to remove the ASN.1 padding byte count */
}

/*
 * Parse a ECDSA public key with format specified in RFC5280 et al.
 *
 * OID for icEcPublicKey is 1.2.840.10045.2.1
 * OIDs for supported curves are as follows:
 *     secp224r1: 1.3.132.0.33
 *     secp256r1 (prime256v1): 1.2.840.10045.3.1.7
 *     secp384r1: 1.3.132.0.34
 */
static int
bootutil_ecdsa_parse_public_key(bootutil_ecdsa_context *ctx, uint8_t **p, uint8_t *end)
{
    psa_key_attributes_t key_attributes = psa_key_attributes_init();
    size_t key_size;
    (void)end;

    /* public key oid is valid */
    if (memcmp(PUB_KEY_OID_OFFSET(p), IdEcPublicKey, sizeof(IdEcPublicKey))) {
        return (int)PSA_ERROR_INVALID_ARGUMENT;
    }

    if (!memcmp(CURVE_TYPE_OID_OFFSET(p), Secp256r1, sizeof(Secp256r1))) {
        ctx->curve_byte_count = 32;
        ctx->required_algorithm = PSA_ALG_SHA_256;
    } else if (!memcmp(CURVE_TYPE_OID_OFFSET(p), Secp384r1, sizeof(Secp384r1))) {
        ctx->curve_byte_count = 48;
        ctx->required_algorithm = PSA_ALG_SHA_384;
    } else {
        return (int)PSA_ERROR_INVALID_ARGUMENT;
    }

    get_public_key_from_rfc5280_encoding(p, &key_size);
    /* Set attributes and import key */
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(ctx->required_algorithm));
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));

    return (int)psa_import_key(&key_attributes, *p, key_size, &ctx->key_id);
}

/* This helper function parses a signature as specified in RFC 5480 into a pair
 * (r,s) of contiguous bytes
 *
 * \param[in]  sig      Pointer to a buffer containing the encoded signature
 * \param[in] num_of_curve_bytes The required number of bytes for r and s
 * \param[out] r_s_pair Buffer containing the (r,s) pair extracted. It's caller
 *                      responsibility to ensure the buffer is big enough to
 *                      hold the parsed (r,s) pair.
 */
static inline void parse_signature_from_rfc5480_encoding(const uint8_t *sig,size_t num_of_curve_bytes,
                                                           uint8_t *r_s_pair)
{
    const uint8_t *sig_ptr = NULL;

    /* Move r in place */
    size_t r_len = sig[3];
    sig_ptr = &sig[4];
    if (r_len >= num_of_curve_bytes) {
        sig_ptr = sig_ptr + r_len - num_of_curve_bytes;
        memcpy(&r_s_pair[0], sig_ptr, num_of_curve_bytes);
        if(r_len % 2) {
            r_len--;
        }
    } else {
        memcpy(&r_s_pair[num_of_curve_bytes - r_len], sig_ptr, r_len);
    }

    /* Move s in place */
    size_t s_len = sig_ptr[r_len+1]; /*  + 1 to skip SEQUENCE */
    sig_ptr  = &sig_ptr[r_len+2];
    if (s_len >= num_of_curve_bytes) {
        sig_ptr = sig_ptr + s_len - num_of_curve_bytes;
        memcpy(&r_s_pair[num_of_curve_bytes], sig_ptr, num_of_curve_bytes);
    } else {
        memcpy(&r_s_pair[2*num_of_curve_bytes - s_len], sig_ptr, s_len);
    }
}

/* PSA Crypto has a dedicated API for ECDSA verification */
static inline int bootutil_ecdsa_verify(const bootutil_ecdsa_context *ctx,
    uint8_t *hash, size_t hlen, uint8_t *sig, size_t slen)
{
    (void)slen;

    uint8_t reformatted_signature[96] = {0}; /* Up to P-384 signature sizes */
    parse_signature_from_rfc5480_encoding(sig, ctx->curve_byte_count,reformatted_signature);

    return (int) psa_verify_hash(ctx->key_id, PSA_ALG_ECDSA(ctx->required_algorithm),
                                 hash, hlen, reformatted_signature, 2*ctx->curve_byte_count);
}
#endif /* MCUBOOT_USE_PSA_CRYPTO */

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_ECDSA_H_ */
