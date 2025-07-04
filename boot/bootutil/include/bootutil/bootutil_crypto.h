/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 */

#ifndef H_BOOTUTIL_CRYPTO
#define H_BOOTUTIL_CRYPTO

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented and provided by system/crypto backends.
 *
 * Function is called by MCUboot after all operations that require
 * cryptographic key access have been completed and no further access
 * to key information should be needed.
 * Implementation of this function is provided by crypto backends
 * and is intended to allow cleanup of key objects and/or application
 * of desired key access policies, which will affect code forward
 * from the point of function invocation, through the entire boot
 * session.
 * This function should not depend on any of MCUboot internal
 * objects and should only affect crypto backend state.
 */
void bootutil_crypto_key_housekeeping(void);

#ifdef __cplusplus
}
#endif

#endif
