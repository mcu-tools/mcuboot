/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES) && defined(MCUBOOT_ENCRYPT_EC256)

#include "encrypted_priv.h"

static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

/*
 * Parses the output of `imgtool keygen`, which produces a PKCS#8 elliptic
 * curve keypair. See RFC5208 and RFC5915.
 */
static int
parse_ec256_enckey(uint8_t **p, uint8_t *end, uint8_t *private_key)
{
    int rc;
    size_t len;
    int version;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 0) {
        return -3;
    }

    if ((rc = mbedtls_asn1_get_alg(p, end, &alg, &param)) != 0) {
        return -5;
    }

    if (alg.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.MBEDTLS_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -6;
    }
    if (param.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_secp256r1_oid) - 1 ||
        memcmp(param.MBEDTLS_CONTEXT_MEMBER(p), ec_secp256r1_oid, sizeof(ec_secp256r1_oid) - 1)) {
        return -7;
    }

    if ((rc = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return -8;
    }

    /* RFC5915 - ECPrivateKey */

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -9;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 1) {
        return -10;
    }

    /* privateKey */

    if ((rc = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return -11;
    }

    if (len != NUM_ECC_BYTES) {
        return -12;
    }

    memcpy(private_key, *p, len);

    /* publicKey usually follows but is not parsed here */

    return 0;
}

/*
 * Decrypt an encryption key TLV.
 *
 * @param buf An encryption TLV read from flash (build time fixed length)
 * @param enckey An AES-128 or AES-256 key sized buffer to store to plain key.
 */
int
boot_enc_decrypt(const uint8_t *buf, uint8_t *enckey)
{
    bootutil_ecdh_p256_context ecdh_p256;
    uint8_t shared[SHARED_KEY_LEN];
    uint8_t *cp;
    uint8_t *cpend;
    uint8_t derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t private_key[PRIV_KEY_LEN];
    uint8_t counter[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    int rc = -1;

    /*
    * Load the stored EC256 decryption private key
    */

    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    rc = parse_ec256_enckey(&cp, cpend, private_key);
    if (rc) {
        return rc;
    }

    /*
    * First "element" in the TLV is the curve point (public key)
    */

    bootutil_ecdh_p256_init(&ecdh_p256);

    rc = bootutil_ecdh_p256_shared_secret(&ecdh_p256, &buf[EC_PUBK_INDEX], private_key, shared);
    bootutil_ecdh_p256_drop(&ecdh_p256);
    if (rc) {
        return -1;
    }
    
    rc = expand_secret(derived_key, shared);
    if (rc) {
        return -1;
    }

    rc = HMAC_key(buf, derived_key);
    if (rc) {
        return -1;
    }
    
    return decrypt_ciphered_key(buf, counter, derived_key, enckey);

}

#endif /* MCUBOOT_ENC_IMAGES && MCUBOOT_ENCRYPT_EC256 */
