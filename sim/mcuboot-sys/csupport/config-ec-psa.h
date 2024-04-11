/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

#ifndef MCUBOOT_PSA_CRYPTO_CONFIG_ECDSA
#define MCUBOOT_PSA_CRYPTO_CONFIG_ECDSA

#if defined(MCUBOOT_USE_PSA_CRYPTO)
#include "config-add-psa-crypto.h"
#endif

#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C

/* mbed TLS modules */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_AES_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#if defined(MCUBOOT_SIGN_EC384)
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#else
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#endif /* MCUBOOT_SIGN_EC384 */

#endif /* MCUBOOT_PSA_CRYPTO_CONFIG_ECDSA */
