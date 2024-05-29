/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mcuboot_config/mcuboot_config.h"

#include <stddef.h>
#include <inttypes.h>
#include <string.h>

/* We are not really using the MBEDTLS but need the ASN.1 parsing functions */
#define MBEDTLS_ASN1_PARSE_C

#include "bootutil/crypto/sha.h"
#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"

#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"
#include "bootutil/crypto/common.h"

#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot_psa_enc);

#define EXPECTED_ENC_LEN    BOOT_ENC_TLV_SIZE
#define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_X25519
#define EC_PUBK_INDEX       (0)
#define EC_TAG_INDEX        (32)
#define EC_CIPHERKEY_INDEX  (32 + 32)
_Static_assert(EC_CIPHERKEY_INDEX + BOOT_ENC_KEY_SIZE == EXPECTED_ENC_LEN,
        "Please fix ECIES-X25519 component indexes");

#define X25519_OID "\x6e"
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_ISO_IDENTIFIED_ORG \
                                       MBEDTLS_OID_ORG_GOV X25519_OID;

#define SHARED_KEY_LEN 32
#define PRIV_KEY_LEN   32

/* Fixme: This duplicates code from encrypted.c and depends on mbedtls */
static int
parse_x25519_enckey(uint8_t **p, uint8_t *end, uint8_t *private_key)
{
    size_t len;
    int version;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
                                           MBEDTLS_ASN1_SEQUENCE) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 0) {
        return -3;
    }

    if (mbedtls_asn1_get_alg(p, end, &alg, &param) != 0) {
        return -4;
    }

    if (alg.ASN1_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.ASN1_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -5;
    }

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -6;
    }

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -7;
    }

    if (len != PRIV_KEY_LEN) {
        return -8;
    }

    memcpy(private_key, *p, PRIV_KEY_LEN);
    return 0;
}

void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx)
{
    psa_status_t psa_ret = psa_crypto_init();

    (void)ctx;

    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES init PSA crypto init failed %d", psa_ret);
        assert(0);
    }
}

#if defined(MCUBOOT_ENC_IMAGES)
extern const struct bootutil_key bootutil_enc_key;
/*
 * Decrypt an encryption key TLV.
 *
 * @param buf An encryption TLV read from flash (build time fixed length)
 * @param enckey An AES-128 or AES-256 key sized buffer to store to plain key.
 */
int
boot_decrypt_key(const uint8_t *buf, uint8_t *enckey)
{
    uint8_t derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t *cp;
    uint8_t *cpend;
    uint8_t private_key[PRIV_KEY_LEN];
    size_t len;
    psa_status_t psa_ret = PSA_ERROR_BAD_STATE;
    psa_status_t psa_cleanup_ret = PSA_ERROR_BAD_STATE;
    psa_key_id_t kid;
    psa_key_attributes_t kattr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_derivation_operation_t key_do = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_algorithm_t key_do_alg;
    int rc = -1;

    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    /* The psa_cipher_decrypt needs initialization vector of proper length at
     * the beginning of the input buffer.
     */
    uint8_t iv_and_key[PSA_CIPHER_IV_LENGTH(PSA_KEY_TYPE_AES, PSA_ALG_CTR) +
                       BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE];

    psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES crypto init failed %d", psa_ret);
        return -1;
    }

    /*
     * Load the stored X25519 decryption private key
     */
    rc = parse_x25519_enckey(&cp, cpend, private_key);
    if (rc) {
        return rc;
    }

    psa_set_key_type(&kattr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_usage_flags(&kattr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&kattr, PSA_ALG_ECDH);

    psa_ret = psa_import_key(&kattr, private_key, sizeof(private_key), &kid);
    memset(private_key, 0, sizeof(private_key));
    psa_reset_key_attributes(&kattr);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("Built-in key import failed %d", psa_ret);
        return -1;
    }

    key_do_alg = PSA_ALG_KEY_AGREEMENT(PSA_ALG_ECDH, PSA_ALG_HKDF(PSA_ALG_SHA_256));

    psa_ret = psa_key_derivation_setup(&key_do, key_do_alg);
    if (psa_ret != PSA_SUCCESS) {
        psa_cleanup_ret = psa_destroy_key(kid);
        if (psa_cleanup_ret != PSA_SUCCESS) {
            BOOT_LOG_WRN("Built-in key destruction failed %d", psa_cleanup_ret);
        }
        BOOT_LOG_ERR("Key derivation setup failed %d", psa_ret);
        return -1;
    }

    /* Note: PSA 1.1.2 does not have psa_key_agreement that would be useful here
     * as it could just add the derived key to the storage and return key id.
     * Instead, we have to use the code below to generate derived key and put it
     * into storage, to obtain the key id we can then use with psa_mac_* functions.
     */
    psa_ret = psa_key_derivation_key_agreement(&key_do, PSA_KEY_DERIVATION_INPUT_SECRET,
                                               kid, &buf[EC_PUBK_INDEX],
                                               BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    psa_cleanup_ret = psa_destroy_key(kid);
    if (psa_cleanup_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("Built-in key destruction failed %d", psa_cleanup_ret);
    }
    if (psa_ret != PSA_SUCCESS) {
        psa_cleanup_ret = psa_key_derivation_abort(&key_do);
        if (psa_cleanup_ret != PSA_SUCCESS) {
            BOOT_LOG_WRN("Key derivation abort failed %d", psa_ret);
        }

        BOOT_LOG_ERR("Key derivation failed %d", psa_ret);
        return -1;
    }

    /* Only info, no salt */
    psa_ret = psa_key_derivation_input_bytes(&key_do, PSA_KEY_DERIVATION_INPUT_INFO,
                                             "MCUBoot_ECIES_v1", 16);
    if (psa_ret != PSA_SUCCESS) {
        psa_cleanup_ret = psa_key_derivation_abort(&key_do);
        if (psa_cleanup_ret != PSA_SUCCESS) {
            BOOT_LOG_WRN("Key derivation abort failed %d", psa_ret);
        }
        BOOT_LOG_ERR("Key derivation failed %d", psa_ret);
        return -1;
    }

    len = BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
    psa_ret = psa_key_derivation_output_bytes(&key_do, derived_key, len);
    psa_cleanup_ret = psa_key_derivation_abort(&key_do);
    if (psa_cleanup_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("Key derivation cleanup failed %d", psa_ret);
    }
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("Key derivation failed %d", psa_ret);
        return -1;
    }

    /* The derived key consists of BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE bytes
     * followed by BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE bytes. Both parts will
     * be imported at the point where needed and discarded immediately after.
     */
    psa_set_key_type(&kattr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&kattr, PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&kattr, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    /* Import the MAC tag key part of derived key, that is the part that starts
     * after BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE and has length of
     * BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE bytes.
     */
    psa_ret = psa_import_key(&kattr,
                             &derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE],
                             BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE, &kid);
    psa_reset_key_attributes(&kattr);
    if (psa_ret != PSA_SUCCESS) {
        memset(derived_key, 0, sizeof(derived_key));
        BOOT_LOG_ERR("MAC key import failed %d", psa_ret);
        return -1;
    }

    /* Verify the MAC tag of the random encryption key */
    psa_ret = psa_mac_verify(kid, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             &buf[EC_CIPHERKEY_INDEX], BOOT_ENC_KEY_SIZE,
                             &buf[EC_TAG_INDEX],
                             BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    psa_cleanup_ret = psa_destroy_key(kid);
    if (psa_cleanup_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("MAC key destruction failed %d", psa_cleanup_ret);
    }
    if (psa_ret != PSA_SUCCESS) {
        memset(derived_key, 0, sizeof(derived_key));
        BOOT_LOG_ERR("MAC verification failed %d", psa_ret);
        return -1;
    }

    /* The derived key is used in AES decryption of random key */
    psa_set_key_type(&kattr, PSA_KEY_TYPE_AES);
    psa_set_key_usage_flags(&kattr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&kattr, PSA_ALG_CTR);

    /* Import the AES partition of derived key, the first 16 bytes */
    psa_ret = psa_import_key(&kattr, &derived_key[0],
                             BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE, &kid);
    memset(derived_key, 0, sizeof(derived_key));
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES key import failed %d", psa_ret);
        return -1;
    }

    /* Decrypt the random AES encryption key with AES and the key obtained
     * at derivation. */
    memset(&iv_and_key[0], 0, PSA_CIPHER_IV_LENGTH(PSA_KEY_TYPE_AES, PSA_ALG_CTR));
    memcpy(&iv_and_key[PSA_CIPHER_IV_LENGTH(PSA_KEY_TYPE_AES, PSA_ALG_CTR)],
           &buf[EC_CIPHERKEY_INDEX],
	   sizeof(iv_and_key) - PSA_CIPHER_IV_LENGTH(PSA_KEY_TYPE_AES, PSA_ALG_CTR));

    len = 0;
    psa_ret = psa_cipher_decrypt(kid, PSA_ALG_CTR, iv_and_key, sizeof(iv_and_key),
                                 enckey, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE, &len);
    memset(iv_and_key, 0, sizeof(iv_and_key));
    psa_cleanup_ret = psa_destroy_key(kid);
    if (psa_cleanup_ret != PSA_SUCCESS) {
	BOOT_LOG_WRN("AES key destruction failed %d", psa_cleanup_ret);
    }
    if (psa_ret != PSA_SUCCESS || len != BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE) {
        memset(enckey, 0, BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE);
        BOOT_LOG_ERR("Random key decryption failed %d", psa_ret);
        return -1;
    }

    return 0;
}

int bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter,
        const uint8_t *m, uint32_t mlen, size_t blk_off, uint8_t *c)
{
    int ret = 0;
    psa_status_t psa_ret = PSA_ERROR_BAD_STATE;
    psa_key_attributes_t kattr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t kid;
    psa_cipher_operation_t psa_op;
    size_t elen = 0;	/* Decrypted length */

    /* Fixme: calling psa_crypto_init multiple times is not a problem,
     * yet the code here is only present because there is not general
     * crypto init. */
    psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("PSA crypto init failed %d", psa_ret);
        ret = -1;
        goto gone;
    }

    psa_op = psa_cipher_operation_init();

    /* Fixme: Import should happen when key is decrypted, but due to lack
     * of key destruction there is no way to destroy key stored by
     * psa other way than here. */
    psa_set_key_type(&kattr, PSA_KEY_TYPE_AES);
    psa_set_key_usage_flags(&kattr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&kattr, PSA_ALG_CTR);

    psa_ret = psa_import_key(&kattr, ctx->key, BOOT_ENC_KEY_SIZE, &kid);
    psa_reset_key_attributes(&kattr);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES enc import key failed %d", psa_ret);
        ret = -1;
        goto gone;
    }

    /* This could be done with psa_cipher_decrypt one-shot operation, but
     * multi-part operation is used to avoid re-allocating input buffer
     * to account for IV in front of data.
     */
    psa_ret = psa_cipher_encrypt_setup(&psa_op, kid, PSA_ALG_CTR);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES enc setup failed %d", psa_ret);
        ret = -1;
        goto gone_with_key;
    }

    /* Fixme: hardcoded counter  size, but it is hardcoded everywhere */
    psa_ret = psa_cipher_set_iv(&psa_op, counter, 16);
    if (psa_ret != PSA_SUCCESS) {
	BOOT_LOG_ERR("AES enc IV set failed %d", psa_ret);
        ret = -1;
        goto gone_after_setup;
    }

    psa_ret = psa_cipher_update(&psa_op, m, mlen, c, mlen, &elen);
    if (psa_ret != PSA_SUCCESS) {
	BOOT_LOG_ERR("AES enc encryption failed %d", psa_ret);
        ret = -1;
        goto gone_after_setup;
    }

gone_after_setup:
    psa_ret = psa_cipher_abort(&psa_op);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("AES enc cipher abort failed %d", psa_ret);
        /* Intentionally not changing the ret */
    }
gone_with_key:
    /* Fixme: Should be removed once key is shared by id */
    psa_ret = psa_destroy_key(kid);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("AES enc destroy key failed %d", psa_ret);
        /* Intentionally not changing the ret */
    }
gone:
    return ret;
}

int bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx, uint8_t *counter,
        const uint8_t *c, uint32_t clen, size_t blk_off, uint8_t *m)
{
    int ret = 0;
    psa_status_t psa_ret = PSA_ERROR_BAD_STATE;
    psa_key_attributes_t kattr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t kid;
    psa_cipher_operation_t psa_op;
    size_t dlen = 0;	/* Decrypted length */

    /* Fixme: the init should already happen before calling the function, but
     * somehow it does not, for example when recovering in swap.
     */
    psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("PSA crypto init failed %d", psa_ret);
        ret = -1;
        goto gone;
    }

    psa_op = psa_cipher_operation_init();

    /* Fixme: Import should happen when key is decrypted, but due to lack
     * of key destruction there is no way to destroy key stored by
     * psa other way than here. */
    psa_set_key_type(&kattr, PSA_KEY_TYPE_AES);
    psa_set_key_usage_flags(&kattr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&kattr, PSA_ALG_CTR);

    psa_ret = psa_import_key(&kattr, ctx->key, BOOT_ENC_KEY_SIZE, &kid);
    psa_reset_key_attributes(&kattr);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES dec import key failed %d", psa_ret);
        ret = -1;
        goto gone;
    }

    /* This could be done with psa_cipher_decrypt one-shot operation, but
     * multi-part operation is used to avoid re-allocating input buffer
     * to account for IV in front of data.
     */
    psa_ret = psa_cipher_decrypt_setup(&psa_op, kid, PSA_ALG_CTR);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_ERR("AES dec setup failed %d", psa_ret);
        ret = -1;
        goto gone_with_key;
    }

    /* Fixme: hardcoded counter  size, but it is hardcoded everywhere */
    psa_ret = psa_cipher_set_iv(&psa_op, counter, 16);
    if (psa_ret != PSA_SUCCESS) {
	BOOT_LOG_ERR("AES dec IV set failed %d", psa_ret);
        ret = -1;
        goto gone_after_setup;
    }

    psa_ret = psa_cipher_update(&psa_op, c, clen, m, clen, &dlen);
    if (psa_ret != PSA_SUCCESS) {
	BOOT_LOG_ERR("AES dec decryption failed %d", psa_ret);
        ret = -1;
        goto gone_after_setup;
    }

gone_after_setup:
    psa_ret = psa_cipher_abort(&psa_op);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("PSA dec abort failed %d", psa_ret);
        /* Intentionally not changing the ret */
    }
gone_with_key:
    psa_ret = psa_destroy_key(kid);
    if (psa_ret != PSA_SUCCESS) {
        BOOT_LOG_WRN("PSA dec key failed %d", psa_ret);
        /* Intentionally not changing the ret */
    }
gone:
    return ret;
}
#endif /* defined(MCUBOOT_ENC_IMAGES) */
