/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Arm Limited
 */

#include "bootutil/fault_injection_hardening.h"

#ifdef FIH_ENABLE_DELAY

#include "bootutil/fault_injection_hardening_delay_rng.h"
#include "mcuboot-mbedtls-cfg.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

/* Mbedtls implementation of the delay RNG. Can be replaced by any other RNG
 * implementation that is backed by an entropy source by altering these
 * functions. This is not provided as a header API and a C file implementation
 * due to issues with inlining.
 */

#ifdef MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#error "FIH_ENABLE_DELAY requires an entropy source"
#endif /* MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES */

mbedtls_entropy_context fih_entropy_ctx;
mbedtls_ctr_drbg_context fih_drbg_ctx;

int fih_delay_init(void)
{
    mbedtls_entropy_init(&fih_entropy_ctx);
    mbedtls_ctr_drbg_init(&fih_drbg_ctx);
    mbedtls_ctr_drbg_seed(&fih_drbg_ctx , mbedtls_entropy_func,
                          &fih_entropy_ctx, NULL, 0);

    return 1;
}

unsigned char fih_delay_random_uchar(void)
{
    unsigned char delay;

    mbedtls_ctr_drbg_random(&fih_drbg_ctx,(unsigned char*) &delay,
                            sizeof(delay));

    return delay;
}

#endif /* FIH_ENABLE_DELAY */
