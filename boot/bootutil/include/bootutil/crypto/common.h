/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2021 Arm Limited
 */

#ifndef __BOOTUTIL_CRYPTO_COMMON_H__
#define __BOOTUTIL_CRYPTO_COMMON_H__

/* The check below can be performed even for those cases
 * where MCUBOOT_USE_MBED_TLS has not been defined
 */
#include "mbedtls/version.h"
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define MBEDTLS_CONTEXT_MEMBER(X) MBEDTLS_PRIVATE(X)
#else
#define MBEDTLS_CONTEXT_MEMBER(X) X
#endif

#endif /* __BOOTUTIL_CRYPTO_COMMON_H__ */
