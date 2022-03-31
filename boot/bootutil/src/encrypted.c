/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)
#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#if defined(MCUBOOT_ENCRYPT_RSA)
#include "mbedtls/rsa.h"
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#include "rsa_alt_helpers.h"
#else
#include "mbedtls/rsa_internal.h"
#endif
#include "mbedtls/asn1.h"
#endif

#if defined(MCUBOOT_ENCRYPT_KW)
#include "bootutil/crypto/aes_kw.h"
#endif

#if defined(MCUBOOT_ENCRYPT_EC256)
#include "bootutil/crypto/ecdh_p256.h"
#endif

#if defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/ecdh_x25519.h"
#endif

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/sha256.h"
#include "bootutil/crypto/hmac_sha256.h"
#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"
#endif

#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"
#include "bootutil/crypto/common.h"

#include "bootutil_priv.h"

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
#if defined(_compare)
static inline int bootutil_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t size)
{
    return _compare(a, b, size);
}
#else
static int bootutil_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t size)
{
    const uint8_t *tempa = a;
    const uint8_t *tempb = b;
    uint8_t result = 0;
    unsigned int i;

    for (i = 0; i < size; i++) {
        result |= tempa[i] ^ tempb[i];
    }
    return result;
}
#endif
#endif

#if defined(MCUBOOT_ENCRYPT_KW)
static int
key_unwrap(const uint8_t *wrapped, uint8_t *enckey)
{
    bootutil_aes_kw_context aes_kw;
    int rc;

    bootutil_aes_kw_init(&aes_kw);
    rc = bootutil_aes_kw_set_unwrap_key(&aes_kw, bootutil_enc_key.key, *bootutil_enc_key.len);

    if (rc == 0) {
        rc = bootutil_aes_kw_unwrap(&aes_kw, wrapped, TLV_ENC_KW_SZ, enckey, BOOT_ENC_KEY_SIZE);
    }

    bootutil_aes_kw_drop(&aes_kw);
    return rc;
}
#endif /* MCUBOOT_ENCRYPT_KW */

#if defined(MCUBOOT_ENCRYPT_RSA)
static int
parse_rsa_enckey(mbedtls_rsa_context *ctx, uint8_t **p, uint8_t *end)
{
    size_t len;

    if (mbedtls_asn1_get_tag(p, end, &len,
                MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    /* Non-optional fields. */
    if ( /* version */
        mbedtls_asn1_get_int(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(ver)) != 0 ||
         /* public modulus */
        mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(N)) != 0 ||
         /* public exponent */
        mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(E)) != 0 ||
         /* private exponent */
        mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(D)) != 0 ||
         /* primes */
        mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(P)) != 0 ||
        mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(Q)) != 0) {

        return -3;
    }

#if !defined(MBEDTLS_RSA_NO_CRT)
    /*
     * DP/DQ/QP are only used inside mbedTLS if it was built with the
     * Chinese Remainder Theorem enabled (default). In case it is disabled
     * we parse, or if not available, we calculate those values.
     */
    if (*p < end) {
        if ( /* d mod (p-1) and d mod (q-1) */
            mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(DP)) != 0 ||
            mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(DQ)) != 0 ||
             /* q ^ (-1) mod p */
            mbedtls_asn1_get_mpi(p, end, &ctx->MBEDTLS_CONTEXT_MEMBER(QP)) != 0) {

            return -4;
        }
    } else {
        if (mbedtls_rsa_deduce_crt(&ctx->MBEDTLS_CONTEXT_MEMBER(P),
                                   &ctx->MBEDTLS_CONTEXT_MEMBER(Q),
                                   &ctx->MBEDTLS_CONTEXT_MEMBER(D),
                                   &ctx->MBEDTLS_CONTEXT_MEMBER(DP),
                                   &ctx->MBEDTLS_CONTEXT_MEMBER(DQ),
                                   &ctx->MBEDTLS_CONTEXT_MEMBER(QP)) != 0) {
            return -5;
        }
    }
#endif

    ctx->MBEDTLS_CONTEXT_MEMBER(len) = mbedtls_mpi_size(&ctx->MBEDTLS_CONTEXT_MEMBER(N));

    if (mbedtls_rsa_check_privkey(ctx) != 0) {
        return -6;
    }

    return 0;
}
#endif

#if defined(MCUBOOT_ENCRYPT_EC256)
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

#define SHARED_KEY_LEN NUM_ECC_BYTES
#define PRIV_KEY_LEN   NUM_ECC_BYTES

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
#endif /* defined(MCUBOOT_ENCRYPT_EC256) */

#if defined(MCUBOOT_ENCRYPT_X25519)
#define X25519_OID "\x6e"
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_ISO_IDENTIFIED_ORG \
                                       MBEDTLS_OID_ORG_GOV X25519_OID;

#define SHARED_KEY_LEN 32
#define PRIV_KEY_LEN   32

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

    if (alg.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.MBEDTLS_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
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
#endif /* defined(MCUBOOT_ENCRYPT_X25519) */

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
/*
 * HKDF as described by RFC5869.
 *
 * @param ikm       The input data to be derived.
 * @param ikm_len   Length of the input data.
 * @param info      An information tag.
 * @param info_len  Length of the information tag.
 * @param salt      An salt tag.
 * @param salt_len  Length of the salt tag.
 * @param okm       Output of the KDF computation.
 * @param okm_len   On input the requested length; on output the generated length
 */
static int
hkdf(const uint8_t *ikm, uint16_t ikm_len, const uint8_t *info, uint16_t info_len,
        const uint8_t *salt, uint16_t salt_len, uint8_t *okm, uint16_t *okm_len)
{
    bootutil_hmac_sha256_context hmac;
    uint8_t prk[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t T[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint16_t off;
    uint16_t len;
    uint8_t counter;
    bool first;
    int rc;

    /*
     * Extract
     */

    if (ikm == NULL || okm == NULL || ikm_len == 0) {
        return -1;
    }

    bootutil_hmac_sha256_init(&hmac);

    rc = bootutil_hmac_sha256_set_key(&hmac, salt, salt_len);
    if (rc != 0) {
        goto error;
    }

    rc = bootutil_hmac_sha256_update(&hmac, ikm, ikm_len);
    if (rc != 0) {
        goto error;
    }

    rc = bootutil_hmac_sha256_finish(&hmac, prk, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        goto error;
    }

    /*
     * Expand
     */

    len = *okm_len;
    counter = 1;
    first = true;
    for (off = 0; len > 0; off += BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE, ++counter) {
        bootutil_hmac_sha256_init(&hmac);

        rc = bootutil_hmac_sha256_set_key(&hmac, prk, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
        if (rc != 0) {
            goto error;
        }

        if (first) {
            first = false;
        } else {
            rc = bootutil_hmac_sha256_update(&hmac, T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            if (rc != 0) {
                goto error;
            }
        }

        rc = bootutil_hmac_sha256_update(&hmac, info, info_len);
        if (rc != 0) {
            goto error;
        }

        rc = bootutil_hmac_sha256_update(&hmac, &counter, 1);
        if (rc != 0) {
            goto error;
        }

        rc = bootutil_hmac_sha256_finish(&hmac, T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
        if (rc != 0) {
            goto error;
        }

        if (len > BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
            memcpy(&okm[off], T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            len -= BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
        } else {
            memcpy(&okm[off], T, len);
            len = 0;
        }
    }

    bootutil_hmac_sha256_drop(&hmac);
    return 0;

error:
    bootutil_hmac_sha256_drop(&hmac);
    return -1;
}
#endif

int
boot_enc_init(struct enc_key_data *enc_state, uint8_t slot)
{
    bootutil_aes_ctr_init(&enc_state[slot].aes_ctr);
    return 0;
}

int
boot_enc_drop(struct enc_key_data *enc_state, uint8_t slot)
{
    bootutil_aes_ctr_drop(&enc_state[slot].aes_ctr);
    return 0;
}

int
boot_enc_set_key(struct enc_key_data *enc_state, uint8_t slot,
        const struct boot_status *bs)
{
    int rc;

    rc = bootutil_aes_ctr_set_key(&enc_state[slot].aes_ctr, bs->enckey[slot]);
    if (rc != 0) {
        boot_enc_drop(enc_state, slot);
        enc_state[slot].valid = 0;
        return -1;
    }

    enc_state[slot].valid = 1;

    return 0;
}

#define EXPECTED_ENC_LEN            BOOT_ENC_TLV_SIZE

#if defined(MCUBOOT_ENCRYPT_RSA)
#    define EXPECTED_ENC_TLV        IMAGE_TLV_ENC_RSA2048
#elif defined(MCUBOOT_ENCRYPT_KW)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_KW
#elif defined(MCUBOOT_ENCRYPT_EC256)
#    define EXPECTED_ENC_TLV     IMAGE_TLV_ENC_EC256
#    define EXPECTED_ENC_EXT_LEN (EXPECTED_ENC_LEN + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE)
#    define EC_PUBK_INDEX       (0)
#    define EC_TAG_INDEX        (65)
#    define EC_CIPHERKEY_INDEX  (65 + 32)
_Static_assert(EC_CIPHERKEY_INDEX + BOOT_ENC_KEY_SIZE == EXPECTED_ENC_LEN,
        "Please fix ECIES-P256 component indexes");
#elif defined(MCUBOOT_ENCRYPT_X25519)
#    define EXPECTED_ENC_TLV        IMAGE_TLV_ENC_X25519
#    define EC_PUBK_INDEX       (0)
#    define EC_TAG_INDEX        (32)
#    define EC_CIPHERKEY_INDEX  (32 + 32)
_Static_assert(EC_CIPHERKEY_INDEX + BOOT_ENC_KEY_SIZE == EXPECTED_ENC_LEN,
        "Please fix ECIES-X25519 component indexes");
#endif

#ifndef EXPECTED_ENC_EXT_LEN
#define EXPECTED_ENC_EXT_LEN        EXPECTED_ENC_LEN
#endif

#if defined(MCUBOOT_ENCRYPT_RSA) || \
    (defined(MCUBOOT_ENCRYPT_EC256) && defined(MCUBOOT_USE_MBED_TLS))
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
static int fake_rng(void *p_rng, unsigned char *output, size_t len)
{
    size_t i;

    (void)p_rng;
    for (i = 0; i < len; i++) {
        output[i] = (char)i;
    }

    return 0;
}
#endif
#endif /* defined(MCUBOOT_ENCRYPT_RSA) ||
          defined(MCUBOOT_ENCRYPT_EC256) && defined(MCUBOOT_USE_MBED_TLS) */

/*
 * Decrypt an encryption key TLV.
 *
 * @param buf An encryption TLV read from flash (build time fixed length)
 * @param sz An encryption TLV buffer data size
 * @param enckey An AES-128 or AES-256 key sized buffer to store to plain key.
 */
int
boot_enc_decrypt(const uint8_t *buf, uint8_t *enckey, uint32_t sz, uint8_t *enciv)
{
#if defined(MCUBOOT_ENCRYPT_RSA)
    mbedtls_rsa_context rsa;
    uint8_t *cp;
    uint8_t *cpend;
    size_t olen;
#endif
#if defined(MCUBOOT_ENCRYPT_EC256)
    bootutil_ecdh_p256_context ecdh_p256;
#endif
#if defined(MCUBOOT_ENCRYPT_X25519)
    bootutil_ecdh_x25519_context ecdh_x25519;
#endif
#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
    bootutil_hmac_sha256_context hmac;
    bootutil_aes_ctr_context aes_ctr;
    uint8_t salt[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t tag[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t shared[SHARED_KEY_LEN];
    uint8_t derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE + BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE * 2];
    uint8_t *cp;
    uint8_t *cpend;
    uint8_t private_key[PRIV_KEY_LEN];
    uint8_t counter[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    uint16_t len;
    uint16_t out_len;
    const uint8_t *my_salt = salt;
    uint8_t *my_key_iv = counter;
    uint8_t *my_counter = counter;
#else
    (void)sz;
    (void)enciv;
#endif
    int rc = -1;

#if defined(MCUBOOT_ENCRYPT_RSA)

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    mbedtls_rsa_init(&rsa);
    mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
#else
    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
#endif
    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    rc = parse_rsa_enckey(&rsa, &cp, cpend);
    if (rc) {
        mbedtls_rsa_free(&rsa);
        return rc;
    }
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    rc = mbedtls_rsa_rsaes_oaep_decrypt(&rsa, fake_rng, NULL,
            NULL, 0, &olen, buf, enckey, BOOT_ENC_KEY_SIZE);
#else
    rc = mbedtls_rsa_rsaes_oaep_decrypt(&rsa, NULL, NULL, MBEDTLS_RSA_PRIVATE,
            NULL, 0, &olen, buf, enckey, BOOT_ENC_KEY_SIZE);
#endif
    mbedtls_rsa_free(&rsa);

#endif /* defined(MCUBOOT_ENCRYPT_RSA) */

#if defined(MCUBOOT_ENCRYPT_KW)

    assert(*bootutil_enc_key.len == BOOT_ENC_KEY_SIZE);
    rc = key_unwrap(buf, enckey);

#endif /* defined(MCUBOOT_ENCRYPT_KW) */

#if defined(MCUBOOT_ENCRYPT_EC256)

    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    /*
     * Load the stored EC256 decryption private key
     */

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
    if (rc != 0) {
        return -1;
    }

#endif /* defined(MCUBOOT_ENCRYPT_EC256) */

#if defined(MCUBOOT_ENCRYPT_X25519)

    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    /*
     * Load the stored X25519 decryption private key
     */

    rc = parse_x25519_enckey(&cp, cpend, private_key);
    if (rc) {
        return rc;
    }

    /*
     * First "element" in the TLV is the curve point (public key)
     */

    bootutil_ecdh_x25519_init(&ecdh_x25519);

    rc = bootutil_ecdh_x25519_shared_secret(&ecdh_x25519, &buf[EC_PUBK_INDEX], private_key, shared);
    bootutil_ecdh_x25519_drop(&ecdh_x25519);
    if (!rc) {
        return -1;
    }

#endif /* defined(MCUBOOT_ENCRYPT_X25519) */

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)

    /*
     * Expand shared secret to create keys for AES-128-CTR + HMAC-SHA256
     */

    /* Prepare for default encryption scheme with zero salt and AES IVs */
    (void)memset(counter, 0, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);
    (void)memset(salt, 0, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);

    len = BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;

    if (sz > EXPECTED_ENC_LEN) {
        /* Use new enhanced encryption scheme with randomly generated salt and AES IVs */
        len += BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE * 2;

        my_salt = &buf[EXPECTED_ENC_LEN];

        my_key_iv = &derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE +
                                 BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];

        my_counter = &derived_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE +
                                  BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE +
                                  BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE];
    }

    out_len = len;
    rc = hkdf(shared, SHARED_KEY_LEN, (uint8_t *)"MCUBoot_ECIES_v1", BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE,
              my_salt, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE, derived_key, &out_len);

    if (rc != 0 || len != out_len) {
        return -1;
    }

    /*
     * HMAC the key and check that our received MAC matches the generated tag
     */

    bootutil_hmac_sha256_init(&hmac);

    rc = bootutil_hmac_sha256_set_key(&hmac, &derived_key[BOOT_ENC_KEY_SIZE], BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    rc = bootutil_hmac_sha256_update(&hmac, &buf[EC_CIPHERKEY_INDEX], BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    /* Assumes the tag buffer is at least sizeof(hmac_tag_size(state)) bytes */
    rc = bootutil_hmac_sha256_finish(&hmac, tag, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    if (bootutil_constant_time_compare(tag, &buf[EC_TAG_INDEX], BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }

    bootutil_hmac_sha256_drop(&hmac);

    (void)memcpy(enciv, my_counter, BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE);

    /*
     * Finally decrypt the received ciphered key
     */

    bootutil_aes_ctr_init(&aes_ctr);

    rc = bootutil_aes_ctr_set_key(&aes_ctr, derived_key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    rc = bootutil_aes_ctr_decrypt(&aes_ctr, my_key_iv, &buf[EC_CIPHERKEY_INDEX], BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE, 0, enckey);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    bootutil_aes_ctr_drop(&aes_ctr);

    rc = 0;

#endif /* defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519) */

    return rc;
}

/*
 * Load encryption key.
 */
int
boot_enc_load(struct enc_key_data *enc_state, int image_index,
        const struct image_header *hdr, const struct flash_area *fap,
        struct boot_status *bs)
{
    uint32_t off;
    uint16_t len;
    struct image_tlv_iter it;
#if MCUBOOT_SWAP_SAVE_ENCTLV
    uint8_t *buf;
#else
    uint8_t buf[EXPECTED_ENC_EXT_LEN];
#endif
    uint8_t slot;
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, flash_area_get_id(fap));
    if (rc < 0) {
        return rc;
    }
    slot = rc;

#ifndef MCUBOOT_ENC_IMAGES_XIP
    /* Already loaded... */
    if (enc_state[slot].valid) {
        return 1;
    }
#endif

    /* Initialize the AES context */
    boot_enc_init(enc_state, slot);

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, EXPECTED_ENC_TLV, false);
    if (rc) {
        return -1;
    }

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        return rc;
    }

    if ((len < EXPECTED_ENC_LEN) || (len > EXPECTED_ENC_EXT_LEN)) {
        return -1;
    }

#if MCUBOOT_SWAP_SAVE_ENCTLV
    buf = bs->enctlv[slot];
    (void)memset(buf, BOOT_UNINITIALIZED_TLV_FILL, BOOT_ENC_TLV_ALIGN_SIZE);
#endif

    rc = flash_area_read(fap, off, buf, len);
    if (rc != 0) {
        return -1;
    }

    return (boot_enc_decrypt(buf, bs->enckey[slot], len, enc_state[slot].aes_iv));
}

bool
boot_enc_valid(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap)
{
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, flash_area_get_id(fap));
    if (rc < 0) {
        /* can't get proper slot number - skip encryption, */
        /* postpone the error for a upper layer */
        return false;
    }

    return enc_state[rc].valid;
}

int
boot_encrypt(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap, uint32_t off, uint32_t sz,
        uint32_t blk_off, uint8_t *buf)
{
    struct enc_key_data *enc;
    uint8_t *nonce;
    int rc;

    /* boot_copy_region will call boot_encrypt with sz = 0 when skipping over
       the TLVs. */
    if (sz == 0) {
        return 0;
    }

#if MCUBOOT_SWAP_USING_SCRATCH
/* in this case scratch area contains encrypted source block that was copied
from secondary slot */
    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        rc = 1;
    }
    else
#endif
    {
        rc = flash_area_id_to_multi_image_slot(image_index, fap->fa_id);
        if (rc < 0) {
            assert(0);
            return rc;
        }
    }

    enc = &enc_state[rc];
    assert(enc->valid == 1);

    nonce = enc->aes_iv;

    off >>= 4;
    nonce[12] = (uint8_t)(off >> 24);
    nonce[13] = (uint8_t)(off >> 16);
    nonce[14] = (uint8_t)(off >> 8);
    nonce[15] = (uint8_t)off;

    return bootutil_aes_ctr_encrypt(&enc->aes_ctr, nonce, buf, sz, blk_off, buf);
}

/**
 * Clears encrypted state after use.
 */
void
boot_enc_zeroize(struct enc_key_data *enc_state)
{
    uint8_t slot;
    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        (void)boot_enc_drop(enc_state, slot);
    }
    (void)memset(enc_state, 0, sizeof(struct enc_key_data) * BOOT_NUM_SLOTS);
}

#endif /* MCUBOOT_ENC_IMAGES */
