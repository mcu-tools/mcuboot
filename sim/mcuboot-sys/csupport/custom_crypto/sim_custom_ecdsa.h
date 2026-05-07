/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Simulator ECDSA-P256 backend for MCUBOOT_USE_CUSTOM_CRYPTO.
 * Implements the bootutil ECDSA abstraction using mbedTLS.
 *
 * Setting MCUBOOT_ECDSA_NEED_ASN1_SIG tells image_ecdsa.c to pass the raw
 * DER-encoded signature to bootutil_ecdsa_verify() (same as the mbedTLS path).
 */

#ifndef SIM_CUSTOM_ECDSA_H
#define SIM_CUSTOM_ECDSA_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <mbedtls/ecdsa.h>
#include <mbedtls/asn1.h>
#include <mbedtls/oid.h>

/* Tell image_ecdsa.c to pass the DER signature unchanged to verify(). */
#define MCUBOOT_ECDSA_NEED_ASN1_SIG

#define SIM_NUM_ECC_BYTES (32)

#ifdef __cplusplus
extern "C" {
#endif

/* ---- SubjectPublicKeyInfo parser (same logic as the mainline mbed path) -- */

static const uint8_t sim_ec_pubkey_oid[]    = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t sim_ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

static inline int sim_import_key(uint8_t **cp, uint8_t *end)
{
    size_t len;
    mbedtls_asn1_buf alg, param;

    if (mbedtls_asn1_get_tag(cp, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        return -1;
    }
    end = *cp + len;
    if (mbedtls_asn1_get_alg(cp, end, &alg, &param)) { return -2; }
    if (alg.len != sizeof(sim_ec_pubkey_oid) - 1 ||
        memcmp(alg.p, sim_ec_pubkey_oid, sizeof(sim_ec_pubkey_oid) - 1)) {
        return -3;
    }
    if (param.len != sizeof(sim_ec_secp256r1_oid) - 1 ||
        memcmp(param.p, sim_ec_secp256r1_oid, sizeof(sim_ec_secp256r1_oid) - 1)) {
        return -4;
    }
    if (mbedtls_asn1_get_bitstring_null(cp, end, &len)) { return -5; }
    if (*cp + len != end) { return -6; }
    if (len != 2 * SIM_NUM_ECC_BYTES + 1) { return -7; }
    return 0;
}

/* ---- Context ------------------------------------------------------------- */

typedef mbedtls_ecdsa_context bootutil_ecdsa_context;

static inline void bootutil_ecdsa_init(bootutil_ecdsa_context *ctx)
{
    mbedtls_ecdsa_init(ctx);
}

static inline void bootutil_ecdsa_drop(bootutil_ecdsa_context *ctx)
{
    mbedtls_ecdsa_free(ctx);
}

/* Advance *cp past the SubjectPublicKeyInfo wrapper to the raw EC point. */
static inline int bootutil_ecdsa_parse_public_key(bootutil_ecdsa_context *ctx,
                                                  uint8_t **cp, uint8_t *end)
{
    (void)ctx;
    return sim_import_key(cp, end);
}

/* pk/pk_len: raw 65-byte uncompressed EC point; sig/sig_len: DER signature. */
static inline int bootutil_ecdsa_verify(bootutil_ecdsa_context *ctx,
                                        uint8_t *pk, size_t pk_len,
                                        uint8_t *hash, size_t hash_len,
                                        uint8_t *sig, size_t sig_len)
{
    int rc;

    rc = mbedtls_ecp_group_load(&ctx->MBEDTLS_PRIVATE(grp),
                                MBEDTLS_ECP_DP_SECP256R1);
    if (rc) { return -1; }

    rc = mbedtls_ecp_point_read_binary(&ctx->MBEDTLS_PRIVATE(grp),
                                       &ctx->MBEDTLS_PRIVATE(Q),
                                       pk, pk_len);
    if (rc) { return -1; }

    rc = mbedtls_ecp_check_pubkey(&ctx->MBEDTLS_PRIVATE(grp),
                                  &ctx->MBEDTLS_PRIVATE(Q));
    if (rc) { return -1; }

    rc = mbedtls_ecdsa_read_signature(ctx, hash, hash_len, sig, sig_len);
    return (rc == 0) ? 0 : -1;
}

#ifdef __cplusplus
}
#endif

#endif /* SIM_CUSTOM_ECDSA_H */
