/*
 * PSA Crypto configuration for ECDSA signature verification on
 * Mbed TLS 4.1.0 (TF-PSA-Crypto 1.1.0).
 *
 * Used with the `mbedtls-v4` Cargo feature. Supplied to the build
 * via `-DTF_PSA_CRYPTO_CONFIG_FILE=<config-ec-psa-v4.h>`; this file
 * replaces the default `psa/crypto_config.h`.
 *
 * Unlike 3.x, crypto configuration in 4.x is PSA-native: enable
 * mechanisms via PSA_WANT_* macros; the build system's
 * crypto_adjust_config_enable_builtins.h then flips on the
 * corresponding MBEDTLS_*_C legacy internals automatically.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MCUBOOT_CONFIG_EC_PSA_V4_H
#define MCUBOOT_CONFIG_EC_PSA_V4_H

/* Opt into the 1.x config format. */
#define TF_PSA_CRYPTO_CONFIG_VERSION 0x01000000

/* Build the PSA implementation (not just client stubs). */
#define MBEDTLS_PSA_CRYPTO_C

/* Entropy is supplied externally by the test RNG shim
 * (psa_rng_stub_v4.c); do not pull in CTR_DRBG/entropy. */
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG

/* No permanent key storage — keys are always imported per boot. */
/* (MBEDTLS_PSA_CRYPTO_STORAGE_C intentionally left unset.) */

/* ECDSA verification over NIST P-256 with SHA-256. */
#define PSA_WANT_ALG_ECDSA                      1
#define PSA_WANT_ALG_SHA_256                    1
#define PSA_WANT_ECC_SECP_R1_256                1
#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY        1

#if defined(MCUBOOT_SIGN_EC384)
/* P-384 variant, selected via Cargo feature sig-p384. */
#define PSA_WANT_ALG_SHA_384                    1
#define PSA_WANT_ECC_SECP_R1_384                1
#endif

#endif /* MCUBOOT_CONFIG_EC_PSA_V4_H */
