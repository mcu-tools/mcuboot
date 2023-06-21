/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2017 Linaro LTD
 * Copyright (C) 2021 Arm Limited
 *
 * Original license:
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SIGN_EC256
/*TODO: remove this after cypress port mbedtls to abstract crypto api */
#if defined(MCUBOOT_USE_CC310) || defined(MCUBOOT_USE_MBED_TLS)
#define NUM_ECC_BYTES (256 / 8)
#endif
#if defined(MCUBOOT_USE_TINYCRYPT) || defined(MCUBOOT_USE_CC310) || \
    defined(MCUBOOT_USE_MBED_TLS)
#include "bootutil/sign_key.h"

#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"
#include "bootutil/crypto/ecdsa_p256.h"
#include "bootutil/crypto/common.h"
#include "bootutil_priv.h"

/*
 * Declaring these like this adds NULL termination.
 */
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

/*
 * Parse the public key used for signing.
 */
#ifdef CY_MBEDTLS_HW_ACCELERATION
static int
bootutil_parse_eckey(mbedtls_ecdsa_context *ctx, uint8_t **p, uint8_t *end)
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
    if (param.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_secp256r1_oid) - 1 ||
      memcmp(param.MBEDTLS_CONTEXT_MEMBER(p), ec_secp256r1_oid, sizeof(ec_secp256r1_oid) - 1)) {
        return -4;
    }

    if (mbedtls_ecp_group_load(&ctx->MBEDTLS_CONTEXT_MEMBER(grp), MBEDTLS_ECP_DP_SECP256R1)) {
        return -5;
    }

    if (mbedtls_asn1_get_bitstring_null(p, end, &len)) {
        return -6;
    }
    if (*p + len != end) {
        return -7;
    }

    if (mbedtls_ecp_point_read_binary(&ctx->MBEDTLS_CONTEXT_MEMBER(grp),
                                      &ctx->MBEDTLS_CONTEXT_MEMBER(Q),
                                      *p, end - *p)) {
        return -8;
    }

    if (mbedtls_ecp_check_pubkey(&ctx->MBEDTLS_CONTEXT_MEMBER(grp),
                                 &ctx->MBEDTLS_CONTEXT_MEMBER(Q))) {
        return -9;
    }
    return 0;
}
#else /* !CY_MBEDTLS_HW_ACCELERATION */
static int
bootutil_import_key(uint8_t **cp, uint8_t *end)
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
#endif /* CY_MBEDTLS_HW_ACCELERATION */

#ifndef MCUBOOT_ECDSA_NEED_ASN1_SIG
/*
 * cp points to ASN1 string containing an integer.
 * Verify the tag, and that the length is 32 bytes.
 */
static int
bootutil_read_bigint(uint8_t i[NUM_ECC_BYTES], uint8_t **cp, uint8_t *end)
{
    size_t len;

    if (mbedtls_asn1_get_tag(cp, end, &len, MBEDTLS_ASN1_INTEGER)) {
        return -3;
    }

    if (len >= NUM_ECC_BYTES) {
        (void)memcpy(i, *cp + len - NUM_ECC_BYTES, NUM_ECC_BYTES);
    } else {
        (void)memset(i, 0, NUM_ECC_BYTES - len);
        (void)memcpy(i + NUM_ECC_BYTES - len, *cp, len);
    }
    *cp += len;
    return 0;
}

/*
 * Read in signature. Signature has r and s encoded as integers.
 */
static int
bootutil_decode_sig(uint8_t signature[NUM_ECC_BYTES * 2], uint8_t *cp, uint8_t *end)
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

fih_int
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
  uint8_t key_id)
{
    int rc;
    bootutil_ecdsa_p256_context ctx;
    uint8_t *pubkey;
    uint8_t *end;

#ifndef MCUBOOT_ECDSA_NEED_ASN1_SIG
    uint8_t signature[2 * NUM_ECC_BYTES];
#endif

    pubkey = (uint8_t *)bootutil_keys[key_id].key;
    end = pubkey + *bootutil_keys[key_id].len;

#ifdef CY_MBEDTLS_HW_ACCELERATION
    mbedtls_ecdsa_init(&ctx);
    rc = bootutil_parse_eckey(&ctx, &pubkey, end);
#else
    rc = bootutil_import_key(&pubkey, end);
#endif
    if (rc != 0) {
        return FIH_FAILURE;
    }

#ifndef MCUBOOT_ECDSA_NEED_ASN1_SIG
    rc = bootutil_decode_sig(signature, sig, sig + slen);
    if (rc != 0) {
        return FIH_FAILURE;
    }
#endif

    /*
     * This is simplified, as the hash length is also 32 bytes.
     */
#ifdef CY_MBEDTLS_HW_ACCELERATION

    rc = mbedtls_ecdsa_read_signature(&ctx, hash, hlen, sig, slen);

#else /* CY_MBEDTLS_HW_ACCELERATION */
    if (hlen != NUM_ECC_BYTES) {
        return FIH_FAILURE;
    }

    bootutil_ecdsa_p256_init(&ctx);
#ifdef MCUBOOT_ECDSA_NEED_ASN1_SIG
    rc = bootutil_ecdsa_p256_verify(&ctx, pubkey, end - pubkey, hash, sig, slen);
#else
    rc = bootutil_ecdsa_p256_verify(&ctx, pubkey, end - pubkey, hash, signature,
                                    2 * NUM_ECC_BYTES);
#endif
#endif /* CY_MBEDTLS_HW_ACCELERATION */

    bootutil_ecdsa_p256_drop(&ctx);

    FIH_RET(fih_int_encode_zero_equality(rc));
}

#endif /* MCUBOOT_USE_TINYCRYPT || defined MCUBOOT_USE_CC310 */
#endif /* MCUBOOT_SIGN_EC256 */
