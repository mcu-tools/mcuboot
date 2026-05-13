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
 * (fake_external_rng_for_test.c); do not pull in CTR_DRBG/entropy. */
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

#if defined(MCUBOOT_ENCRYPT_EC256)
/*
 * ECIES-P256 image encryption via boot/bootutil/src/encrypted_psa.c:
 * ECDH key agreement feeds HKDF, whose output keys AES-CTR (data) and
 * HMAC-SHA-256 (TLV authenticator). Enabled by the combined
 * sig-ecdsa-psa + enc-ec256 + mbedtls-v4 Cargo features.
 */
#define PSA_WANT_ALG_ECDH                       1
#define PSA_WANT_ALG_HKDF                       1
#define PSA_WANT_ALG_CTR                        1
#define PSA_WANT_ALG_HMAC                       1
#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_DERIVE   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_IMPORT   1
#define PSA_WANT_KEY_TYPE_HMAC                  1
#endif

#endif /* MCUBOOT_CONFIG_EC_PSA_V4_H */
