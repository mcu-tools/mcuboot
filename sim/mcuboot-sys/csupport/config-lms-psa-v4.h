/*
 * PSA Crypto + LMS configuration for Mbed TLS 4.1.0 (TF-PSA-Crypto 1.1.0).
 *
 * Used with the Cargo features sig-lms + mbedtls-v4. The LMS verifier
 * lives in tf-psa-crypto/extras/lms.c and depends on MBEDTLS_LMS_C plus
 * the PSA-side SHA-256 it uses internally for its hash chains. mcuboot
 * verifies LMS only — MBEDTLS_LMS_PRIVATE (signing) is intentionally
 * left unset; signing is done out-of-tree by imgtool.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MCUBOOT_CONFIG_LMS_PSA_V4_H
#define MCUBOOT_CONFIG_LMS_PSA_V4_H

/* Opt into the 1.x config format. */
#define TF_PSA_CRYPTO_CONFIG_VERSION 0x01000000

/* Build the PSA implementation. LMS reaches into psa_hash_* internally;
 * mcuboot's bootutil_img_hash also goes through PSA when
 * MCUBOOT_USE_PSA_CRYPTO is set. */
#define MBEDTLS_PSA_CRYPTO_C

/* Entropy is supplied externally by the test RNG shim
 * (sim/mcuboot-sys/csupport/psa_rng_stub_v4.c). */
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG

/* SHA-256 — used both by LMS (per RFC 8554) and by bootutil_img_hash
 * via the PSA SHA backend (boot/bootutil/include/bootutil/crypto/sha.h). */
#define PSA_WANT_ALG_SHA_256                    1

/* LMS verification over MBEDTLS_LMS_SHA256_M32_H10 — the only parameter
 * set the 4.x implementation supports. */
#define MBEDTLS_LMS_C

#endif /* MCUBOOT_CONFIG_LMS_PSA_V4_H */
