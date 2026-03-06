/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Simulator ECDH-P256 backend for MCUBOOT_USE_CUSTOM_CRYPTO.
 * Implements the bootutil ECDH-P256 abstraction using mbedTLS.
 * The private key is received as raw bytes and used transiently.
 */

#ifndef SIM_CUSTOM_ECDH_P256_H
#define SIM_CUSTOM_ECDH_P256_H

#include <stdint.h>
#include <stddef.h>

#include <mbedtls/build_info.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/bignum.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_ECP256_PRIVKEY_BYTES (32u)
#define SIM_ECP256_PUBKEY_BYTES  (65u)
#define SIM_ECP256_SHARED_BYTES  (32u)

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
/* mbedTLS 3.x requires a non-NULL f_rng for ecp_mul (point blinding).
 * This stub satisfies the requirement for the simulator context where
 * side-channel resistance is not a concern. */
static inline int sim_fake_rng(void *p_rng, unsigned char *output, size_t len)
{
    size_t i;
    (void)p_rng;
    for (i = 0; i < len; i++) {
        output[i] = (unsigned char)i;
    }
    return 0;
}
#endif /* MBEDTLS_VERSION_NUMBER >= 0x03000000 */

typedef struct { int _unused; } bootutil_ecdh_p256_context;
typedef bootutil_ecdh_p256_context bootutil_key_exchange_ctx;

static inline void bootutil_ecdh_p256_init(bootutil_ecdh_p256_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_ecdh_p256_drop(bootutil_ecdh_p256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_ecdh_p256_shared_secret(
        bootutil_ecdh_p256_context *ctx,
        const uint8_t *pk,
        const uint8_t *sk,
        uint8_t *z)
{
    mbedtls_ecp_group grp;
    mbedtls_ecp_point peer;
    mbedtls_mpi       d, shared;
    int rc = -1;

    (void)ctx;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&peer);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&shared);

    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) { goto out; }
    if (mbedtls_ecp_point_read_binary(&grp, &peer, pk, SIM_ECP256_PUBKEY_BYTES) != 0) { goto out; }
    if (mbedtls_ecp_check_pubkey(&grp, &peer) != 0) { goto out; }
    if (mbedtls_mpi_read_binary(&d, sk, SIM_ECP256_PRIVKEY_BYTES) != 0) { goto out; }
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    if (mbedtls_ecdh_compute_shared(&grp, &shared, &peer, &d, sim_fake_rng, NULL) != 0) { goto out; }
#else
    if (mbedtls_ecdh_compute_shared(&grp, &shared, &peer, &d, NULL, NULL) != 0) { goto out; }
#endif
    if (mbedtls_mpi_write_binary(&shared, z, SIM_ECP256_SHARED_BYTES) != 0) { goto out; }
    rc = 0;

out:
    mbedtls_mpi_free(&shared);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&peer);
    mbedtls_ecp_group_free(&grp);
    return rc;
}

#ifdef __cplusplus
}
#endif

#endif /* SIM_CUSTOM_ECDH_P256_H */
