/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2024 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)
#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#if defined(MCUBOOT_ENCRYPT_RSA)
#define BOOTUTIL_CRYPTO_RSA_CRYPT_ENABLED
#include "bootutil/crypto/rsa.h"
#endif

#if defined(MCUBOOT_ENCRYPT_KW)
#include "bootutil/crypto/aes_kw.h"
#endif

#if !defined(MCUBOOT_USE_PSA_CRYPTO)
#if defined(MCUBOOT_ENCRYPT_EC256)
#include "bootutil/crypto/ecdh_p256.h"
#endif

#if defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/ecdh_x25519.h"
#endif

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/sha.h"
#include "bootutil/crypto/hmac_sha256.h"
#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"
#endif
#endif

#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"
#include "bootutil/crypto/common.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#include "bootutil_priv.h"

/* NOUP Fixme:  */
#if !defined(CONFIG_BOOT_ED25519_PSA) && !defined(CONFIG_BOOT_ECDSA_PSA)
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
key_unwrap(const uint8_t *wrapped, uint8_t *enckey, struct bootutil_key *bootutil_enc_key)
{
    bootutil_aes_kw_context aes_kw;
    int rc;

    bootutil_aes_kw_init(&aes_kw);
    rc = bootutil_aes_kw_set_unwrap_key(&aes_kw, bootutil_enc_key->key, *bootutil_enc_key->len);
    if (rc != 0) {
        goto done;
    }
    rc = bootutil_aes_kw_unwrap(&aes_kw, wrapped, BOOT_ENC_TLV_SIZE, enckey, BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        goto done;
    }

done:
    bootutil_aes_kw_drop(&aes_kw);
    return rc;
}
#endif /* MCUBOOT_ENCRYPT_KW */

#if defined(MCUBOOT_ENCRYPT_EC256)
static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

/*
 * Parses the output of `imgtool keygen`, which produces a PKCS#8 elliptic
 * curve keypair. See RFC5208 and RFC5915.
 */
static int
parse_priv_enckey(uint8_t **p, uint8_t *end, uint8_t *private_key)
{
    size_t len;
    int version;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if (mbedtls_asn1_get_tag(p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -1;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 0) {
        return -1;
    }

    if (mbedtls_asn1_get_alg(p, end, &alg, &param) != 0) {
        return -1;
    }

    if (alg.ASN1_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.ASN1_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -1;
    }
    if (param.ASN1_CONTEXT_MEMBER(len) != sizeof(ec_secp256r1_oid) - 1 ||
        memcmp(param.ASN1_CONTEXT_MEMBER(p), ec_secp256r1_oid, sizeof(ec_secp256r1_oid) - 1)) {
        return -1;
    }

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -1;
    }

    /* RFC5915 - ECPrivateKey */

    if (mbedtls_asn1_get_tag(p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) != 0) {
        return -1;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 1) {
        return -1;
    }

    /* privateKey */

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -1;
    }

    if (len != NUM_ECC_BYTES) {
        return -1;
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

static int
parse_priv_enckey(uint8_t **p, uint8_t *end, uint8_t *private_key)
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
        return -1;
    }

    version = 0;
    if (mbedtls_asn1_get_int(p, end, &version) || version != 0) {
        return -1;
    }

    if (mbedtls_asn1_get_alg(p, end, &alg, &param) != 0) {
        return -1;
    }

    if (alg.ASN1_CONTEXT_MEMBER(len) != sizeof(ec_pubkey_oid) - 1 ||
        memcmp(alg.ASN1_CONTEXT_MEMBER(p), ec_pubkey_oid, sizeof(ec_pubkey_oid) - 1)) {
        return -1;
    }

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -1;
    }

    if (mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) != 0) {
        return -1;
    }

    if (len != EC_PRIVK_LEN) {
        return -1;
    }

    memcpy(private_key, *p, EC_PRIVK_LEN);
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
 * @param okm       Output of the KDF computation.
 * @param okm_len   On input the requested length; on output the generated length
 */
static int
hkdf(const uint8_t *ikm, size_t ikm_len, const uint8_t *info, size_t info_len,
        uint8_t *okm, size_t *okm_len)
{
    bootutil_hmac_sha256_context hmac;
    uint8_t salt[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t prk[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t T[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    size_t off;
    size_t len;
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

    memset(salt, 0, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    rc = bootutil_hmac_sha256_set_key(&hmac, salt, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
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

    bootutil_hmac_sha256_drop(&hmac);

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

        bootutil_hmac_sha256_drop(&hmac);

        if (len > BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
            memcpy(&okm[off], T, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
            len -= BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
        } else {
            memcpy(&okm[off], T, len);
            len = 0;
        }
    }

    return 0;

error:
    bootutil_hmac_sha256_drop(&hmac);
    return -1;
}
#endif /* MCUBOOT_ENCRYPT_EC256 || MCUBOOT_ENCRYPT_X25519 */

#if !defined(MCUBOOT_ENC_BUILTIN_KEY)
extern const struct bootutil_key bootutil_enc_key;

/*
 * Default implementation to retrieve the private encryption key which is
 * embedded in the bootloader code (when MCUBOOT_ENC_BUILTIN_KEY is not defined).
 */
int boot_enc_retrieve_private_key(struct bootutil_key **private_key)
{
    *private_key = (struct bootutil_key *)&bootutil_enc_key;

    return 0;
}
#endif /* !MCUBOOT_ENC_BUILTIN_KEY */

#if ( (defined(MCUBOOT_ENCRYPT_RSA) && defined(MCUBOOT_USE_MBED_TLS) && !defined(MCUBOOT_USE_PSA_CRYPTO)) || \
      (defined(MCUBOOT_ENCRYPT_EC256) && defined(MCUBOOT_USE_MBED_TLS)) )
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
#endif /* MBEDTLS_VERSION_NUMBER */
#endif /* (MCUBOOT_ENCRYPT_RSA && MCUBOOT_USE_MBED_TLS && !MCUBOOT_USE_PSA_CRYPTO) ||
          (MCUBOOT_ENCRYPT_EC256 && MCUBOOT_USE_MBED_TLS) */

/*
 * Decrypt an encryption key TLV.
 *
 * @param buf An encryption TLV read from flash (build time fixed length)
 * @param enckey An AES-128 or AES-256 key sized buffer to store to plain key.
 */
int
boot_decrypt_key(const uint8_t *buf, uint8_t *enckey)
{
#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
    bootutil_hmac_sha256_context hmac;
    bootutil_aes_ctr_context aes_ctr;
    uint8_t tag[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t shared[EC_SHARED_LEN];
    uint8_t derived_key[BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t private_key[EC_PRIVK_LEN];
    uint8_t counter[BOOT_ENC_BLOCK_SIZE];
#endif
#if !defined(MCUBOOT_ENCRYPT_KW)
    bootutil_key_exchange_ctx pk_ctx;
    uint8_t *cp;
    uint8_t *cpend;
    size_t len;
#endif
    struct bootutil_key *bootutil_enc_key = NULL;
    int rc = -1;

    BOOT_LOG_DBG("boot_decrypt_key");

    rc = boot_enc_retrieve_private_key(&bootutil_enc_key);
    if (rc) {
        return rc;
    }

    if (bootutil_enc_key == NULL) {
        return rc;
    }

#if !defined(MCUBOOT_ENCRYPT_KW)
    cp = (uint8_t *)bootutil_enc_key->key;
    cpend = cp + *bootutil_enc_key->len;
#endif

#if defined(MCUBOOT_ENCRYPT_RSA)
    bootutil_rsa_init(&pk_ctx);

    /* The enckey is encrypted through RSA so for decryption we need the private key */
    rc = bootutil_rsa_parse_private_key(&pk_ctx, &cp, cpend);
    if (rc) {
        bootutil_rsa_drop(&pk_ctx);
        return rc;
    }

    rc = bootutil_rsa_oaep_decrypt(&pk_ctx, &len, buf, enckey, BOOT_ENC_KEY_SIZE);
    bootutil_rsa_drop(&pk_ctx);
    if (rc) {
        return rc;
    }

#endif /* defined(MCUBOOT_ENCRYPT_RSA) */

#if defined(MCUBOOT_ENCRYPT_KW)

    assert(*bootutil_enc_key->len == BOOT_ENC_KEY_SIZE);
    rc = key_unwrap(buf, enckey, bootutil_enc_key);

#endif /* defined(MCUBOOT_ENCRYPT_KW) */

#if defined(MCUBOOT_ENCRYPT_EC256)
    /*
     * Load the stored EC256 decryption private key
     */

    rc = parse_priv_enckey(&cp, cpend, private_key);
    if (rc) {
        BOOT_LOG_ERR("Failed to parse ASN1 private key");
        return rc;
    }

    /*
     * First "element" in the TLV is the curve point (public key)
     */
    bootutil_ecdh_p256_init(&pk_ctx);

    rc = bootutil_ecdh_p256_shared_secret(&pk_ctx, &buf[EC_PUBK_INDEX], private_key, shared);
    bootutil_ecdh_p256_drop(&pk_ctx);
    if (rc != 0) {
        return -1;
    }

#endif /* defined(MCUBOOT_ENCRYPT_EC256) */

#if defined(MCUBOOT_ENCRYPT_X25519)
    /*
     * Load the stored X25519 decryption private key
     */

    rc = parse_priv_enckey(&cp, cpend, private_key);
    if (rc) {
        BOOT_LOG_ERR("Failed to parse ASN1 private key");
        return rc;
    }

    /*
     * First "element" in the TLV is the curve point (public key)
     */

    bootutil_ecdh_x25519_init(&pk_ctx);

    rc = bootutil_ecdh_x25519_shared_secret(&pk_ctx, &buf[EC_PUBK_INDEX], private_key, shared);
    bootutil_ecdh_x25519_drop(&pk_ctx);
    if (!rc) {
        return -1;
    }

#endif /* defined(MCUBOOT_ENCRYPT_X25519) */

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)

    /*
     * Expand shared secret to create keys for AES-128-CTR + HMAC-SHA256
     */

    len = BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
    rc = hkdf(shared, EC_SHARED_LEN, (uint8_t *)"MCUBoot_ECIES_v1", 16,
            derived_key, &len);
    if (rc != 0 || len != (BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE)) {
        return -1;
    }

    /*
     * HMAC the key and check that our received MAC matches the generated tag
     */

    bootutil_hmac_sha256_init(&hmac);

    /* First BOOT_ENC_KEY_SIZE are used for decryption, remaining 32 bytes are used
     * for MAC tag key
     */
    rc = bootutil_hmac_sha256_set_key(&hmac, &derived_key[BOOT_ENC_KEY_SIZE], 32);
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
    (void)bootutil_hmac_sha256_drop(&hmac);
    if (rc != 0) {
        return -1;
    }

    if (bootutil_constant_time_compare(tag, &buf[EC_TAG_INDEX], EC_TAG_LEN) != 0) {
        return -1;
    }

    /*
     * Finally decrypt the received ciphered key
     */

    bootutil_aes_ctr_init(&aes_ctr);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    rc = bootutil_aes_ctr_set_key(&aes_ctr, derived_key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    memset(counter, 0, BOOT_ENC_BLOCK_SIZE);
    rc = bootutil_aes_ctr_decrypt(&aes_ctr, counter, &buf[EC_CIPHERKEY_INDEX], BOOT_ENC_KEY_SIZE, 0, enckey);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }

    bootutil_aes_ctr_drop(&aes_ctr);

    rc = 0;

#endif /* defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519) */

    return rc;
}
#endif /* CONFIG_BOOT_ED25519_PSA  && CONFIG_BOOT_ECDSA_PSA */

/*
 * Load encryption key.
 */
int
boot_enc_load(struct boot_loader_state *state, int slot,
              const struct image_header *hdr, const struct flash_area *fap,
              struct boot_status *bs)
{
    struct enc_key_data *enc_state = BOOT_CURR_ENC_SLOT(state, slot);
    uint32_t off;
    uint16_t len;
    struct image_tlv_iter it;
#if MCUBOOT_SWAP_SAVE_ENCTLV
    uint8_t *buf;
#else
    uint8_t buf[BOOT_ENC_TLV_SIZE];
#endif
    int rc;

    BOOT_LOG_DBG("boot_enc_load: slot %d", slot);

    /* Already loaded... */
    if (boot_enc_valid(enc_state)) {
        BOOT_LOG_DBG("boot_enc_load: already loaded");
        return 1;
    }

    /* Initialize the AES context */
    boot_enc_init(enc_state);

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    it.start_off = boot_get_state_secondary_offset(state, fap);
#endif

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, BOOT_ENC_TLV, false);
    if (rc) {
        return -1;
    }

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        return rc;
    }

    if (len != BOOT_ENC_TLV_SIZE) {
        return -1;
    }

#if MCUBOOT_SWAP_SAVE_ENCTLV
    buf = bs->enctlv[slot];
    memset(buf, 0xff, BOOT_ENC_TLV_ALIGN_SIZE);
#endif

    rc = flash_area_read(fap, off, buf, BOOT_ENC_TLV_SIZE);
    if (rc) {
        return -1;
    }

    return boot_decrypt_key(buf, bs->enckey[slot]);
}

int
boot_enc_init(struct enc_key_data *enc_state)
{
    bootutil_aes_ctr_init(&enc_state->aes_ctr);
    return 0;
}

int
boot_enc_drop(struct enc_key_data *enc_state)
{
    bootutil_aes_ctr_drop(&enc_state->aes_ctr);
    enc_state->valid = 0;
    return 0;
}

int
boot_enc_set_key(struct enc_key_data *enc_state, const uint8_t *key)
{
    int rc;

    rc = bootutil_aes_ctr_set_key(&enc_state->aes_ctr, key);
    if (rc != 0) {
        boot_enc_drop(enc_state);
        return -1;
    }

    enc_state->valid = 1;

    return 0;
}

bool
boot_enc_valid(const struct enc_key_data *enc_state)
{
    return enc_state->valid;
}

void
boot_enc_encrypt(struct enc_key_data *enc, uint32_t off,
             uint32_t sz, uint32_t blk_off, uint8_t *buf)
{
    uint8_t nonce[16];

    /* Nothing to do with size == 0 */
    if (sz == 0) {
       return;
    }

    memset(nonce, 0, 12);
    off >>= 4;
    nonce[12] = (uint8_t)(off >> 24);
    nonce[13] = (uint8_t)(off >> 16);
    nonce[14] = (uint8_t)(off >> 8);
    nonce[15] = (uint8_t)off;

    assert(enc->valid == 1);
    bootutil_aes_ctr_encrypt(&enc->aes_ctr, nonce, buf, sz, blk_off, buf);
}

void
boot_enc_decrypt(struct enc_key_data *enc, uint32_t off,
             uint32_t sz, uint32_t blk_off, uint8_t *buf)
{
    uint8_t nonce[16];

    /* Nothing to do with size == 0 */
    if (sz == 0) {
       return;
    }

    memset(nonce, 0, 12);
    off >>= 4;
    nonce[12] = (uint8_t)(off >> 24);
    nonce[13] = (uint8_t)(off >> 16);
    nonce[14] = (uint8_t)(off >> 8);
    nonce[15] = (uint8_t)off;

    assert(enc->valid == 1);
    bootutil_aes_ctr_decrypt(&enc->aes_ctr, nonce, buf, sz, blk_off, buf);
}

/**
 * Clears encrypted state after use.
 */
void
boot_enc_zeroize(struct enc_key_data *enc_state)
{
    uint8_t slot;
    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        (void)boot_enc_drop(&enc_state[slot]);
    }
    memset(enc_state, 0, sizeof(struct enc_key_data) * BOOT_NUM_SLOTS);
}

#endif /* MCUBOOT_ENC_IMAGES */
