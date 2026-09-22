/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Schneider Electric
 */

/* PSA Crypto implementation of the FIH delay RNG.
 *
 * Replaces fault_injection_hardening_delay_rng_mbedtls.c when the bootloader
 * uses PSA Crypto as its primary crypto stack (CONFIG_BOOT_USE_PSA_CRYPTO on
 * Zephyr: BOOT_ECDSA_PSA, BOOT_ED25519_PSA, or BOOT_RSA_PSA).
 *
 * tf-psa-crypto 1.0 (mbedTLS 4.x) removed the public mbedTLS entropy and
 * CTR-DRBG APIs that the mbedTLS implementation relied on.  psa_generate_random()
 * is the stable, backend-independent replacement.
 */

#include "fault_injection_hardening.h"

#ifdef FIH_ENABLE_DELAY

#include "psa/crypto.h"

int fih_delay_init(void)
{
    psa_status_t status = psa_crypto_init();

    return (status == PSA_SUCCESS) ? 1 : 0;
}

unsigned char fih_delay_random_uchar(void)
{
    unsigned char delay = 1U; /* safe non-zero fallback */

    (void)psa_generate_random(&delay, sizeof(delay));

    return delay;
}

#endif /* FIH_ENABLE_DELAY */
