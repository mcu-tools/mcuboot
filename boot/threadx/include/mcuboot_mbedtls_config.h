/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef MCUBOOT_MBEDTLS_CONFIG_H_
#define MCUBOOT_MBEDTLS_CONFIG_H_

#include "mcuboot_config/mcuboot_config.h"

/* System support */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

#if !defined(CONFIG_ARM)
#define MBEDTLS_HAVE_ASM
#endif

/* Base modules required for MCUboot verification */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_AES_C

#if defined(MCUBOOT_SIGN_EC256)
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECP_MAX_BITS 256
#define MBEDTLS_MPI_MAX_SIZE 32
#endif

#if defined(MCUBOOT_SIGN_RSA)
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_MPI_MAX_SIZE 384
#endif

#define MBEDTLS_SSL_MAX_CONTENT_LEN 1024

#endif /* MCUBOOT_MBEDTLS_CONFIG_H_ */
