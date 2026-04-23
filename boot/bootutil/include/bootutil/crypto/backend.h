/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#ifndef __BOOTUTIL_CRYPTO_BACKEND_H__
#define __BOOTUTIL_CRYPTO_BACKEND_H__

#if defined(MCUBOOT_CRYPTO_BACKEND_HAS_INIT)
/* Backend specific implementation for crypto subsystem
 * initialization.
 * MCUboot is supposed to call this function before
 * any crypto operation, including sha functions,
 * is executed.
 * Returns true on success. The function can also just fault
 * and topple entire boot process.
 */
bool bootutil_crypto_backend_init(void);

/* Backend specific implementation for crypto subsystem
 * deinitialization.
 * No more cryptographic operations are expected to be
 * invoked by MCUboot after this founction is called.
 * Returns true on success.
 */
bool bootutil_crypto_backend_deinit(void);
#else
#define bootutil_crypto_backend_init() (true)
#define bootutil_crypto_backend_deinit() (true)
#endif

#endif /* __BOOTUTIL_CRYPTO_BACKEND_H__ */
