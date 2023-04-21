/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2021 Arm Limited
 */

#ifndef __BOOTUTIL_CRYPTO_COMMON_H__
#define __BOOTUTIL_CRYPTO_COMMON_H__

/* TODO May need to update this in a future 3.x version of Mbed TLS.
 * Extract a member of the mbedtls context structure.
 */
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define MBEDTLS_CONTEXT_MEMBER(X) MBEDTLS_PRIVATE(X)
#else
#define MBEDTLS_CONTEXT_MEMBER(X) X
#endif

#endif /* __BOOTUTIL_CRYPTO_COMMON_H__ */
