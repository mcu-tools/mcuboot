/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

#ifndef MCUBOOT_MBEDTLS_CONFIG_ADD_PSA_CRYPTO_H
#define MCUBOOT_MBEDTLS_CONFIG_ADD_PSA_CRYPTO_H

#include "mbedtls/build_info.h"

/* Enable PSA Crypto Core without support for the permanent storage
 * Don't define MBEDTLS_PSA_CRYPTO_STORAGE_C to make sure that support
 * for permanent keys is not enabled, as it is not usually required during boot
 */
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG

#if defined(MCUBOOT_ENCRYPT_RSA) || defined(MCUBOOT_SIGN_RSA)
    #define MBEDTLS_PK_C
    #define MBEDTLS_CTR_DRBG_C
    #define MBEDTLS_CIPHER_C
    #define MBEDTLS_ENTROPY_C
    #define MBEDTLS_PK_PARSE_C
    #define MBEDTLS_PK_WRITE_C
#endif /* MCUBOOT_ENCRYPT_RSA || MCUBOOT_SIGN_RSA */

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
    #define MBEDTLS_PLATFORM_FREE_MACRO free
    #define MBEDTLS_PLATFORM_CALLOC_MACRO calloc
#endif /* MCUBOOT_ENCRYPT_EC256 || MCUBOOT_ENCRYPT_X25519 */

#if !defined(MCUBOOT_ENCRYPT_X25519)
    #define MBEDTLS_PSA_BUILTIN_CIPHER 1
#endif /* MCUBOOT_ENCRYPT_X25519 */

#if defined(MCUBOOT_ENCRYPT_KW)
    #define MBEDTLS_PSA_CRYPTO_CONFIG
    #define MBEDTLS_POLY1305_C
#endif /* MCUBOOT_ENCRYPT_KW */

#if MBEDTLS_VERSION_NUMBER == 0x03000000
/* This PSA define is available only with more recent versions of 3.x */
#define PSA_KEY_ID_NULL                         ((psa_key_id_t)0)   // not overly happy with this being here
#endif /* MBEDTLS_VERSION_NUMBER == 0x03000000 */

#endif /* MCUBOOT_MBEDTLS_CONFIG_ADD_PSA_CRYPTO_H */
