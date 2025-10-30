/*
 * Copyright (c) 2025 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_GEN_ENC_KEY)
#include <string.h>
#include <stddef.h>
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "bootutil/generate_key_pair.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_hwrng.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);
extern unsigned char enc_priv_key[];
extern unsigned int enc_priv_key_len;

/**
 * @brief Generate random data using the hardware random number generator.
 *
 * @param data Not used.
 * @param output Buffer to fill with random data.
 * @param len Number of random bytes to generate.
 * @param olen Number of random bytes actually generated.
 *
 * @return 0 on success or MBEDTLS_ERR_ENTROPY_SOURCE_FAILED on RNG failure.
 */
#if !defined(CONFIG_MBEDTLS_ENTROPY_POLL_ZEPHYR)
#define NBR_WARM_UP 8
int
mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    (void)data;
    uint32_t val;
    size_t produced = 0;
    // Warm-up
    for (int i = 0; i < NBR_WARM_UP; i++) {
        BOOT_RNG(&val);
    }
    BOOT_LOG_INF("RNG value: %u\r\n", val);

    BOOT_LOG_INF("mbedtls_hardware_poll: ask %lu bytes", (unsigned long)len);

    while (produced < len) {
        if (BOOT_RNG(&val) != 0) {
            BOOT_LOG_ERR("RNG reads fails at %lu/%lu bytes",
                         (unsigned long)produced, (unsigned long)len);
            *olen = produced;
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        size_t copy_len = (len - produced >= 4) ? 4 : (len - produced);
        memcpy(output + produced, &val, copy_len);
        produced += copy_len;
        BOOT_LOG_INF("%08lX", (unsigned long)val, (unsigned long)produced,
                     (unsigned long)len);
    }
    *olen = produced;
    BOOT_LOG_INF("mbedtls_hardware_poll: total generated = %lu bytes",
                 (unsigned long)*olen);
    return 0;
}
#endif

int
mbedtls_hardware_polll_full(void *data, unsigned char *output, size_t len)
{

    size_t dummy;
    return mbedtls_hardware_poll(data, output, len, &dummy);
}
/*
 * Generate an EC-P256 key pair using the mbedTLS library
 *
 * @return 0 on success, or a negative mbedTLS error code on failure.
 */
int
gen_p256_keypair(mbedtls_pk_context *pk)
{
    int ret;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const unsigned char pers[] = "keyge-p256-keygenkeyge-p256-keygen";

    mbedtls_pk_init(pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    /*
     * Seeds the random number generator using a hardware entropy source
     */
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_hardware_polll_full, NULL,
                                pers, sizeof(pers) - 1);
    if (ret != 0) {
        BOOT_LOG_ERR("SEED FAIL ret=%d", ret);
        goto cleanup;
    }

    /*
     * Sets up the public key context for key generation
     */
    ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        BOOT_LOG_ERR("PK_SETUP FAIL ret=%d", ret);
        goto cleanup;
    }

    /*
     *
     */
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

int
export_privkey_der(mbedtls_pk_context *pk)
{
    unsigned char buf[800];
    unsigned char *der_ptr;
    size_t der_len;
    char line[160];
    int pos = 0;

    int len = mbedtls_pk_write_keypkcs8_der(pk, buf, sizeof(buf));
    if (len < 0) {
        BOOT_LOG_ERR("fails write pkcs8 der");
        return len;
    }

    der_ptr = buf + sizeof(buf) - len;
    der_len = (size_t)len;

    BOOT_LOG_INF("Private key DER length = %u\n", (unsigned int)der_len);

    for (size_t i = 0; i < der_len; i++) {
        unsigned int val = (unsigned int)der_ptr[i];
        if (val < 0x10) {
            pos += snprintk(&line[pos], sizeof(line) - pos, "0x0%X", val);
        } else {
            pos += snprintk(&line[pos], sizeof(line) - pos, "0x%X", val);
        }

        if (i < der_len - 1) {
            pos += snprintk(&line[pos], sizeof(line) - pos, ",");
        }

        if (pos > sizeof(line) - 8 || i == der_len - 1) {
            BOOT_LOG_INF("%s", line);
            pos = 0;
        }
    }

    return 0;
}

int
export_pub_pem(mbedtls_pk_context *pk)
{
    unsigned char buf_pub[800];
    unsigned char buf_priv[800];
    int ret;

    ret = mbedtls_pk_write_pubkey_pem(pk, buf_pub, sizeof(buf_pub));
    if (ret != 0) {

        return ret;
    }

    ret = mbedtls_pk_write_key_pem(pk, buf_priv, sizeof(buf_priv));
    if (ret != 0) {

        return ret;
    }

    BOOT_LOG_INF("\n%s", buf_pub);
    BOOT_LOG_INF("\n%s", buf_priv);
    return 0;
}

void
dump_p256(const mbedtls_pk_context *pk)
{
    const mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(*pk);
    unsigned char buf[32];
    memset(buf, 0, sizeof buf);
    mbedtls_mpi_write_binary(&eckey->private_d, buf, 32);
    BOOT_LOG_INF("Private key d = ");
    for (int i = 0; i < 32; i++) {
        BOOT_LOG_INF("%02X", buf[i]);
    }
    BOOT_LOG_INF("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_X, buf, 32);
    BOOT_LOG_INF("Public key Q.X = ");
    for (int i = 0; i < 32; i++) {
        BOOT_LOG_INF("%02X", buf[i]);
    }
    BOOT_LOG_INF("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_Y, buf, 32);
    BOOT_LOG_INF("Public key Q.Y = ");
    for (int i = 0; i < 32; i++) {
        BOOT_LOG_INF("%02X", buf[i]);
    }
    BOOT_LOG_INF("\n");
}

void
generate_enc_key_pair()
{
    mbedtls_pk_context pk;
    int rc = -1;
    BOOT_LOG_INF("Generate enc key pair starting...");
    rc = gen_p256_keypair(&pk);
    rc = export_privkey_der(&pk);
    rc = export_pub_pem(&pk);
    dump_p256(&pk);

    if (rc != 0) {
        BOOT_LOG_ERR("Error during the generation enc key pair\n");
    } else {
        BOOT_LOG_INF("Success key is generated");
    }
}

#endif /* MCUBOOT_GEN_ENC_KEY */
