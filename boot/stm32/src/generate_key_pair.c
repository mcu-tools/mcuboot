#include <string.h>
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "stm32wlxx_hal.h"
#include "generate_key_pair/generate_key_pair.h"
#include "key/key.h"

extern unsigned char enc_priv_key[];
extern unsigned int enc_priv_key_len;
//RNG_HandleTypeDef hrng;

//void MX_RNG_Init(void)
//{
//
//    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
//    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RNG;
//    PeriphClkInit.RngClockSelection    = RCC_RNGCLKSOURCE_MSI;
//    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
//
//    __HAL_RCC_RNG_CLK_ENABLE();
//
//    hrng.Instance = RNG;
//    if (HAL_RNG_Init(&hrng) != HAL_OK) {
//        Error_Handler();
//    }
//
//
//    uint32_t dummy;
//    for (int i = 0; i < 4; i++) {
//        HAL_RNG_GenerateRandomNumber(&hrng, &dummy);
//    }
//}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{

    (void)data;
    uint32_t val;
    size_t produced = 0;
    // Warm-up
    for (int i = 0; i < 8; i++) {
        HAL_RNG_GenerateRandomNumber(&hrng, &val);
    }

    boot_log_info("mbedtls_hardware_poll: ask %lu bytes", (unsigned long)len);


    while (produced < len) {
        if (HAL_RNG_GenerateRandomNumber(&hrng, &val) != HAL_OK) {
            boot_log_info("RNG lecture fails at %lu/%lu bytes", (unsigned long)produced, (unsigned long)len);
            *olen = produced;
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        size_t copy_len = (len - produced >= 4) ? 4 : (len - produced);
        memcpy(output + produced, &val, copy_len);
        produced += copy_len;

        boot_log_info("%08lX",
                      (unsigned long)val,
                      (unsigned long)produced,
                      (unsigned long)len);

    }

    *olen = produced;
    boot_log_info("mbedtls_hardware_poll: total generate = %lu bytes", (unsigned long)*olen);
    return 0;
}

int gen_p256_keypair(mbedtls_pk_context *pk)
{
    int ret;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const unsigned char pers[] = "stm32-p256-keygenstm32-p256-keygen";

    mbedtls_pk_init(pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);


    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_hardware_poll, NULL,
                                pers, sizeof(pers)-1);
    if (ret != 0) {
        boot_log_info("SEED FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        boot_log_info("PK_SETUP FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                              mbedtls_pk_ec(*pk),
                              mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        boot_log_info("GEN_KEY FAIL ret=%d", ret);
        goto cleanup;
    }


        ret = extract_private_key_to_enc_buffer(pk);
        if (ret != 0) {
            boot_log_info("PRIV_KEY_EXTRACT FAIL ret=%d", ret);
            goto cleanup;
        }

cleanup:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

int extract_private_key_to_enc_buffer(const mbedtls_pk_context *pk)
{
    mbedtls_ecp_keypair *ec_key;
    unsigned char priv_key_raw[32];
    int ret;


    ec_key = mbedtls_pk_ec(*pk);
    if (ec_key == NULL) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }


    ret = mbedtls_mpi_write_binary(&(ec_key->private_d), priv_key_raw, 32);
    if (ret != 0) {
        boot_log_info("MPI write binary failed ret=%d", ret);
        return ret;
    }


    if (enc_priv_key_len < 32) {
        boot_log_info("enc_priv_key buffer too small: %d < 32", enc_priv_key_len);
        return MBEDTLS_ERR_PK_BUFFER_TOO_SMALL;
    }


    memset(enc_priv_key, 0, enc_priv_key_len);
    memcpy(enc_priv_key, priv_key_raw, 32);

    boot_log_info("Private key stored in enc_priv_key (32 bytes)");


    mbedtls_platform_zeroize(priv_key_raw, sizeof(priv_key_raw));

    return 0;
}


void dump_p256(const mbedtls_pk_context *pk)
{
    const mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(*pk);
    unsigned char buf[32];
    memset(buf,0, sizeof buf);
    mbedtls_mpi_write_binary(&eckey->private_d, buf, 32);
    boot_log_info("Private key d = ");
    for (int i = 0; i < 32; i++) boot_log_info("%02X", buf[i]);
    boot_log_info("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_X, buf, 32);
    boot_log_info("Public key Q.X = ");
    for (int i = 0; i < 32; i++) boot_log_info("%02X", buf[i]);
    boot_log_info("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_Y, buf, 32);
    boot_log_info("Public key Q.Y = ");
    for (int i = 0; i < 32; i++) boot_log_info("%02X", buf[i]);
    boot_log_info("\n");

}
