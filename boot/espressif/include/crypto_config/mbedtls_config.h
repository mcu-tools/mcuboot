/*
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal Mbed TLS configuration for the MCUboot Espressif port.
 * RSA keeps mbedtls_custom_config.h. This file covers EC256, Ed25519
 * hashing, and unsigned image hashing.
 */

#ifndef ESPRESSIF_MBEDTLS_CONFIG_H
#define ESPRESSIF_MBEDTLS_CONFIG_H

#include <rom/ets_sys.h>

#if defined(CONFIG_ESP_MCUBOOT_SERIAL)
/* Serial recovery encodes packets with Mbed TLS base64. */
#define MBEDTLS_BASE64_C
#endif

#if defined(CONFIG_ESP_SIGN_EC256)

/* System support. EC256 verification allocates through the port heap. */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_EXIT_ALT
#define MBEDTLS_PLATFORM_PRINTF_MACRO        ets_printf
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

#define MBEDTLS_HAVE_ASM

#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_SHA256_C
/* Bootloader IRAM is tight; prefer the smaller hash implementation. */
#define MBEDTLS_SHA256_SMALLER

/* P-256 only. MPI_MAX_SIZE is in bytes. */
#define MBEDTLS_ECP_MAX_BITS             256
#define MBEDTLS_MPI_MAX_SIZE             32

#elif defined(CONFIG_ESP_SIGN_ED25519)

/*
 * Ed25519 verify stays in Fiat. Mbed TLS provides the image hash (SHA-256)
 * and the SHA-512 used inside the Fiat verifier. Mbed TLS 3.6 has no
 * Ed25519 API.
 */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_SHA512_C
#define MBEDTLS_SHA512_SMALLER

#else

/* Unsigned images: SHA-256 image hash only. */
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA256_SMALLER

#endif

#endif /* ESPRESSIF_MBEDTLS_CONFIG_H */
