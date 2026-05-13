/*
 * Copyright (c) 2026 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_GEN_ENC_KEY)
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "bootutil/generate_key_pair.h"
#include "bootutil/boot_store_enc_keys.h"
#include "pkcs8secp256write.h"
#include "bootutil/bootutil_log.h"
BOOT_LOG_MODULE_DECLARE(mcuboot);

#define MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES    (30 + 2 * MBEDTLS_ECP_MAX_BYTES)
#define PK_ECP_SIZE_BASE64 ((MBEDTLS_PK_ECP_PUB_DER_MAX_BYTES*4+3-1)/3)
#define PEM_BEGIN_PUBLIC_KEY_SIZE 25
#define PEM_END_PUBLIC_KEY_SIZE 27
#define NEW_LINE_CHARS 3
#define PEM_PUBLIC_KEY_SIZE (PEM_BEGIN_PUBLIC_KEY_SIZE + PK_ECP_SIZE_BASE64 + PEM_END_PUBLIC_KEY_SIZE + NEW_LINE_CHARS)

/**
 * @brief Generate public and private key and contain in mbedtls_pk_context.
 *
 * @param pk Initialize mbedtls_pk_context and contains the generate key pair.
 *
 * @return 0 for Success.
 * @return Not equal to zero, therefore failure.
 */
static int
gen_p256_keypair(mbedtls_pk_context *pk)
{
    int ret;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const unsigned char pers[] = "keyge-p256-keygenkeyge-p256-keygen";

    mbedtls_pk_init(pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                pers, sizeof(pers) - 1);
    if (ret != 0) {
        BOOT_LOG_ERR("SEED FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        BOOT_LOG_ERR("PK_SETUP FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*pk),
                              mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        BOOT_LOG_ERR("GEN_KEY FAIL ret=%d", ret);
        goto cleanup;
    }

cleanup:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

/**
 * @brief Export private key in PKCS8 format.
 *
 * @param pk Initialize mbedtls_pk_context and contains the generate key pair.
 *
 * @return 0 for Success.
 * @return Not equal to zero, therefore failure.
 */
static int
save_privkey_der(const mbedtls_pk_context *pk)
{
    unsigned char buf[MCUBOOT_PRIV_ENC_KEY_LEN];

    int len = mbedtls_pk_write_keypkcs8_der(pk, buf, sizeof(buf));
    if (len != MCUBOOT_PRIV_ENC_KEY_LEN) {
        BOOT_LOG_ERR("fails write pkcs8 der: len=%d", len);
        return len;
    }

    int ret = store_priv_enc_key(buf, len);
    if (ret != 0)
    {
        BOOT_LOG_ERR("key store fail store ret=%d",ret);
        return ret;
    }

    return 0;
}

/**
 * @brief Export private and public key in PEM format.
 *
 * @param pk Initialize mbedtls_pk_context and contains the generate key pair.
 *
 * @return 0 for Success.
 * @return Not equal to zero, therefore failure.
 */
static int
export_pub_pem(const mbedtls_pk_context *pk)
{
    unsigned char buf_pub[PEM_PUBLIC_KEY_SIZE];

    int ret = mbedtls_pk_write_pubkey_pem(pk, buf_pub, sizeof(buf_pub));
    if (ret != 0) 
    {
        BOOT_LOG_ERR("PUBKEY_PEM FAIL ret=%d", ret);
        return ret;
    }

    BOOT_LOG_INF("\n%s", buf_pub);

    return 0;
}

void
generate_enc_key_pair(void)
{
    mbedtls_pk_context pk;
    BOOT_LOG_INF("Generate enc key pair starting...");

    int ret = gen_p256_keypair(&pk);
    if(ret != 0){
        BOOT_LOG_ERR("Error during the generation enc key pair");
        return;
    }

    ret = save_privkey_der(&pk);
    if(ret != 0){
        BOOT_LOG_ERR("Error during safe of private key der");
        return;
    }

    BOOT_LOG_INF("Saved private enc key");

    ret = export_pub_pem(&pk);
    if(ret != 0){
        BOOT_LOG_ERR("Error during the exportation of public key pem");
        return;
    }
}

#endif /* MCUBOOT_GEN_ENC_KEY */
