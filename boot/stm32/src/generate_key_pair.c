#include <string.h>
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "stm32wlxx_hal.h"
#include "generate_key_pair/generate_key_pair.h"
#include "key/key.h"
#include "bootutil_log.h"

extern unsigned char enc_priv_key[];
extern unsigned int enc_priv_key_len;


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
        	BOOT_LOG_ERR("RNG reads fails at %lu/%lu bytes", (unsigned long)produced, (unsigned long)len);
            *olen = produced;
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        size_t copy_len = (len - produced >= 4) ? 4 : (len - produced);
        memcpy(output + produced, &val, copy_len);
        produced += copy_len;

        boot_log_info("%08lX",(unsigned long)val,(unsigned long)produced,(unsigned long)len);

    }

    *olen = produced;
    boot_log_info("mbedtls_hardware_poll: total generated = %lu bytes", (unsigned long)*olen);
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

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_hardware_poll, NULL,pers, sizeof(pers)-1);
    if (ret != 0) {
        BOOT_LOG_ERR("SEED FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
    	BOOT_LOG_ERR("PK_SETUP FAIL ret=%d", ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*pk),mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
    	BOOT_LOG_ERR("GEN_KEY FAIL ret=%d", ret);
        goto cleanup;
    }


cleanup:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}



int export_privkey_der(mbedtls_pk_context *pk,
                       unsigned char **der_ptr,
                       size_t *der_len) {
    static unsigned char buf[800];
    int ret;


    int len = mbedtls_pk_write_keypkcs8_der(pk, buf, sizeof(buf));
    if (len < 0) {
    	BOOT_LOG_ERR("fails write pkcs8 der");
        return len;
    }

    *der_ptr = buf + sizeof(buf) - len;
    *der_len = (size_t)len;

    return 0;
}



void dump_p256(const mbedtls_pk_context *pk)
{
    const mbedtls_ecp_keypair *eckey = mbedtls_pk_ec(*pk);
    unsigned char buf[32];
    memset(buf,0, sizeof buf);
    mbedtls_mpi_write_binary(&eckey->private_d, buf, 32);
    BOOT_LOG_DBG("Private key d = ");
    for (int i = 0; i < 32; i++) BOOT_LOG_DBG("%02X", buf[i]);
    BOOT_LOG_DBG("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_X, buf, 32);
    BOOT_LOG_DBG("Public key Q.X = ");
    for (int i = 0; i < 32; i++) BOOT_LOG_DBG("%02X", buf[i]);
    BOOT_LOG_DBG("\n");

    mbedtls_mpi_write_binary(&eckey->private_Q.private_Y, buf, 32);
    BOOT_LOG_DBG("Public key Q.Y = ");
    for (int i = 0; i < 32; i++) BOOT_LOG_DBG("%02X", buf[i]);
    BOOT_LOG_DBG("\n");

}

int export_pub_pem(mbedtls_pk_context *pk) {
    unsigned char buf[800];
    unsigned char buf1[800];
    int ret;

    ret = mbedtls_pk_write_pubkey_pem(pk, buf, sizeof(buf));
    if (ret != 0) {

        return ret;
    }

    ret = mbedtls_pk_write_key_pem(pk, buf1, sizeof(buf1));
    if (ret != 0) {

        return ret;
    }

    char *line = strtok((char *)buf,"\n");
    while(line != NULL){
    	BOOT_LOG_DBG("%s", line);
    	line = strtok(NULL,"\n");
    }

    char *line1 = strtok((char *)buf1,"\n");
    while(line1 != NULL){
    	BOOT_LOG_DBG("%s", line1);
    	line1 = strtok(NULL,"\n");
    }

    return 0;
}
