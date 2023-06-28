/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

/* This file, and the methods within are required when PSA Crypto API is enabled
 * (--features psa-crypto-api), but the selected combination of features does
 * not rely on any PSA Crypto APIs, and will not be adding any of them to the build.
 */

#include <bootutil/bootutil_log.h>

int psa_crypto_init()
{
    BOOT_LOG_SIM("psa_crypto_init() is being stubbed.\n");
    return 0;
}

void mbedtls_test_enable_insecure_external_rng(){
    BOOT_LOG_SIM("mbedtls_test_enable_insecure_external_rng() is being stubbed.\n");
}
