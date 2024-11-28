/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are two choices: MCUBOOT_USE_MBED_TLS, or
 * MCUBOOT_USE_TINYCRYPT.  It is a compile error there is not exactly
 * one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_ECDH_P256_H_
#define __BOOTUTIL_CRYPTO_ECDH_P256_H_

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include <mbedtls/ecp.h>
    #include <mbedtls/ecdh.h>
    #define EC256_PUBK_LEN (65)
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include <tinycrypt/ecc_dh.h>
    #include <tinycrypt/constants.h>
    #define BOOTUTIL_CRYPTO_ECDH_P256_HASH_SIZE (4 * 8)
#endif /* MCUBOOT_USE_TINYCRYPT */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MCUBOOT_USE_TINYCRYPT)
typedef uintptr_t bootutil_ecdh_p256_context;
static inline void bootutil_ecdh_p256_init(bootutil_ecdh_p256_context *ctx)
{
    (void)ctx;
}

static inline void bootutil_ecdh_p256_drop(bootutil_ecdh_p256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_ecdh_p256_shared_secret(bootutil_ecdh_p256_context *ctx, const uint8_t *pk, const uint8_t *sk, uint8_t *z)
{
    int rc;
    (void)ctx;

    if (pk[0] != 0x04) {
        return -1;
    }

    rc = uECC_valid_public_key(&pk[1], uECC_secp256r1());
    if (rc != 0) {
        return -1;
    }

    rc = uECC_shared_secret(&pk[1], sk, z, uECC_secp256r1());
    if (rc != TC_CRYPTO_SUCCESS) {
        return -1;
    }
    return 0;
}
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_MBED_TLS) && !defined(MCUBOOT_USE_PSA_CRYPTO)
#define NUM_ECC_BYTES 32

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
static int fake_rng(void *p_rng, unsigned char *output, size_t len);
#endif

typedef struct bootutil_ecdh_p256_context {
    mbedtls_ecp_group grp;
    mbedtls_ecp_point P;
    mbedtls_mpi z;
    mbedtls_mpi d;
} bootutil_ecdh_p256_context;

static inline void bootutil_ecdh_p256_init(bootutil_ecdh_p256_context *ctx)
{
    mbedtls_mpi_init(&ctx->z);
    mbedtls_mpi_init(&ctx->d);

    mbedtls_ecp_group_init(&ctx->grp);
    mbedtls_ecp_point_init(&ctx->P);

    if (mbedtls_ecp_group_load(&ctx->grp, MBEDTLS_ECP_DP_SECP256R1) != 0) {
        mbedtls_ecp_group_free(&ctx->grp);
        mbedtls_ecp_point_free(&ctx->P);
    }
}

static inline void bootutil_ecdh_p256_drop(bootutil_ecdh_p256_context *ctx)
{
    mbedtls_mpi_free(&ctx->d);
    mbedtls_mpi_free(&ctx->z);
    mbedtls_ecp_group_free(&ctx->grp);
    mbedtls_ecp_point_free(&ctx->P);
}

static inline int bootutil_ecdh_p256_shared_secret(bootutil_ecdh_p256_context *ctx, const uint8_t *pk, const uint8_t *sk, uint8_t *z)
{
    int rc;

    rc = mbedtls_ecp_point_read_binary(&ctx->grp,
                                       &ctx->P,
                                       pk,
                                       EC256_PUBK_LEN);
    if (rc != 0) {
        mbedtls_ecp_group_free(&ctx->grp);
        mbedtls_ecp_point_free(&ctx->P);
        return -1;
    }

    rc = mbedtls_ecp_check_pubkey(&ctx->grp, &ctx->P);
    if (rc != 0) {
        mbedtls_ecp_group_free(&ctx->grp);
        mbedtls_ecp_point_free(&ctx->P);
        return -1;
    }

    (void)mbedtls_mpi_read_binary(&ctx->d, sk, NUM_ECC_BYTES);

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    rc = mbedtls_ecdh_compute_shared(&ctx->grp,
                                     &ctx->z,
                                     &ctx->P,
                                     &ctx->d,
                                     fake_rng,
                                     NULL);
#else
    rc = mbedtls_ecdh_compute_shared(&ctx->grp,
                                     &ctx->z,
                                     &ctx->P,
                                     &ctx->d,
                                     NULL,
                                     NULL);
#endif
    (void)mbedtls_mpi_write_binary(&ctx->z, z, NUM_ECC_BYTES);

    return rc;
}
#endif /* MCUBOOT_USE_MBED_TLS */


#if defined(MCUBOOT_USE_PSA_CRYPTO)

#include "crypto.h"
#include "crypto_values.h"

#define NUM_ECC_BYTES 32

typedef void* bootutil_ecdh_p256_context;

static inline void bootutil_ecdh_p256_init(bootutil_ecdh_p256_context *ctx)
{
    (void) ctx;
}

static inline void bootutil_ecdh_p256_drop(bootutil_ecdh_p256_context *ctx)
{
    (void) ctx;
}

static inline int bootutil_ecdh_p256_shared_secret(bootutil_ecdh_p256_context *ctx, const uint8_t *pub_key, const uint8_t *priv_key, uint8_t *shared)
{
    (void) ctx;

    psa_status_t status;
    psa_key_handle_t private_key_handle;

    psa_key_attributes_t private_key_attributes = psa_key_attributes_init();

    psa_set_key_usage_flags(&private_key_attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&private_key_attributes, PSA_ALG_ECDH);
    psa_set_key_type(&private_key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&private_key_attributes, 256u);

    status = psa_import_key(&private_key_attributes, priv_key, 32, &private_key_handle);

    if (status == PSA_SUCCESS)
    {
        size_t res_len;

        status = psa_raw_key_agreement(PSA_ALG_ECDH,
                                    private_key_handle,
                                    pub_key,
                                    65,
                                    shared,
                                    32,
                                    &res_len);
    }

    psa_destroy_key(private_key_handle);

    if (status != PSA_SUCCESS)
    {
        return -1;
    }
    
    return 0;
}
#endif /* MCUBOOT_USE_PSA_CRYPTO */


#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_CRYPTO_ECDH_P256_H_ */
