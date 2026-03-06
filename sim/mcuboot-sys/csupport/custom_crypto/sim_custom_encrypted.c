/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Sim-only encryption support for the MCUBOOT_USE_CUSTOM_CRYPTO path.
 *
 * encrypted.c suppresses its entire body when MCUBOOT_USE_CUSTOM_CRYPTO is
 * defined because embedded targets (e.g. Infineon SE RT) provide their own
 * boot_enc_* symbols.  This file is the sim's custom implementation of those
 * symbols for ECIES-P256 encryption, using the mbedTLS-backed custom crypto
 * stubs.
 *
 * Provides: boot_enc_retrieve_private_key, boot_decrypt_key, boot_enc_init,
 *           boot_enc_drop, boot_enc_set_key, boot_enc_load, boot_enc_valid,
 *           boot_enc_encrypt, boot_enc_decrypt, boot_enc_zeroize.
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES) && defined(MCUBOOT_USE_CUSTOM_CRYPTO)

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bootutil/crypto/ecdh_p256.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/crypto/hmac_sha256.h"
#include "bootutil/crypto/aes_ctr.h"
#include "bootutil/crypto/common.h"
#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"
#include "bootutil/bootutil_log.h"

#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#include "bootutil_priv.h"

/* ------------------------------------------------------------------ */
/*  Constant-time compare                                             */
/* ------------------------------------------------------------------ */
static int
sim_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t size)
{
    uint8_t result = 0;
    size_t i;

    for (i = 0; i < size; i++) {
        result |= a[i] ^ b[i];
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  parse_priv_enckey — PKCS#8 EC private key parser for EC256        */
/* ------------------------------------------------------------------ */
#if defined(MCUBOOT_ENCRYPT_EC256)

static const uint8_t ec_pubkey_oid[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
static const uint8_t ec_secp256r1_oid[] = MBEDTLS_OID_EC_GRP_SECP256R1;

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

    if (len != SIM_NUM_ECC_BYTES) {
        return -1;
    }

    memcpy(private_key, *p, len);
    return 0;
}

#endif /* MCUBOOT_ENCRYPT_EC256 */

/* ------------------------------------------------------------------ */
/*  HKDF (RFC 5869)                                                   */
/* ------------------------------------------------------------------ */
#if defined(MCUBOOT_ENCRYPT_EC256)

static int
hkdf(const uint8_t *ikm, size_t ikm_len,
     const uint8_t *info, size_t info_len,
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

    if (ikm == NULL || okm == NULL || ikm_len == 0) {
        return -1;
    }

    /* Extract */
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

    /* Expand */
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

#endif /* MCUBOOT_ENCRYPT_EC256 */

/* ------------------------------------------------------------------ */
/*  boot_enc_retrieve_private_key                                     */
/* ------------------------------------------------------------------ */
#if !defined(MCUBOOT_ENC_BUILTIN_KEY)
extern const struct bootutil_key bootutil_enc_key;

int
boot_enc_retrieve_private_key(struct bootutil_key **private_key)
{
    *private_key = (struct bootutil_key *)&bootutil_enc_key;
    return 0;
}
#endif /* !MCUBOOT_ENC_BUILTIN_KEY */

/* ------------------------------------------------------------------ */
/*  boot_decrypt_key                                                  */
/* ------------------------------------------------------------------ */
int
boot_decrypt_key(const uint8_t *buf, uint8_t *enckey)
{
#if defined(MCUBOOT_ENCRYPT_EC256)
    bootutil_hmac_sha256_context hmac;
    bootutil_aes_ctr_context aes_ctr;
    uint8_t tag[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t shared[EC_SHARED_LEN];
    uint8_t derived_key[BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t private_key[EC_PRIVK_LEN];
    uint8_t counter[BOOT_ENC_BLOCK_SIZE];
    bootutil_key_exchange_ctx pk_ctx;
    uint8_t *cp;
    uint8_t *cpend;
    size_t len;
    struct bootutil_key *bootutil_enc_key_ptr = NULL;
    int rc = -1;

    BOOT_LOG_DBG("boot_decrypt_key");

    rc = boot_enc_retrieve_private_key(&bootutil_enc_key_ptr);
    if (rc) {
        return rc;
    }
    if (bootutil_enc_key_ptr == NULL) {
        return -1;
    }

    cp = (uint8_t *)bootutil_enc_key_ptr->key;
    cpend = cp + *bootutil_enc_key_ptr->len;

    /* Parse the PKCS#8 EC private key */
    rc = parse_priv_enckey(&cp, cpend, private_key);
    if (rc) {
        BOOT_LOG_ERR("Failed to parse ASN1 private key");
        return rc;
    }

    /* ECDH shared secret */
    bootutil_ecdh_p256_init(&pk_ctx);
    rc = bootutil_ecdh_p256_shared_secret(&pk_ctx, &buf[EC_PUBK_INDEX],
                                          private_key, shared);
    bootutil_ecdh_p256_drop(&pk_ctx);
    if (rc != 0) {
        return -1;
    }

    /* HKDF: derive AES key + HMAC key */
    len = BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE;
    rc = hkdf(shared, EC_SHARED_LEN,
              (const uint8_t *)"MCUBoot_ECIES_v1", 16,
              derived_key, &len);
    if (rc != 0 || len != (BOOT_ENC_KEY_SIZE + BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE)) {
        return -1;
    }

    /* HMAC-SHA256 verify the ciphertext tag */
    bootutil_hmac_sha256_init(&hmac);
    rc = bootutil_hmac_sha256_set_key(&hmac, &derived_key[BOOT_ENC_KEY_SIZE], 32);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }
    rc = bootutil_hmac_sha256_update(&hmac, &buf[EC_CIPHERKEY_INDEX],
                                     BOOT_ENC_KEY_SIZE);
    if (rc != 0) {
        (void)bootutil_hmac_sha256_drop(&hmac);
        return -1;
    }
    rc = bootutil_hmac_sha256_finish(&hmac, tag, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    (void)bootutil_hmac_sha256_drop(&hmac);
    if (rc != 0) {
        return -1;
    }
    if (sim_constant_time_compare(tag, &buf[EC_TAG_INDEX], EC_TAG_LEN) != 0) {
        return -1;
    }

    /* AES-CTR decrypt the wrapped key */
    bootutil_aes_ctr_init(&aes_ctr);
    rc = bootutil_aes_ctr_set_key(&aes_ctr, derived_key);
    if (rc != 0) {
        bootutil_aes_ctr_drop(&aes_ctr);
        return -1;
    }
    memset(counter, 0, BOOT_ENC_BLOCK_SIZE);
    rc = bootutil_aes_ctr_decrypt(&aes_ctr, counter,
                                  &buf[EC_CIPHERKEY_INDEX],
                                  BOOT_ENC_KEY_SIZE, 0, enckey);
    bootutil_aes_ctr_drop(&aes_ctr);
    if (rc != 0) {
        return -1;
    }

    return 0;
#else
    (void)buf;
    (void)enckey;
    return -1;
#endif /* MCUBOOT_ENCRYPT_EC256 */
}

/* ------------------------------------------------------------------ */
/*  boot_enc_load                                                     */
/* ------------------------------------------------------------------ */
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

    if (boot_enc_valid(enc_state)) {
        BOOT_LOG_DBG("boot_enc_load: already loaded");
        return 1;
    }

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

/* ------------------------------------------------------------------ */
/*  boot_enc_{init,drop,set_key,valid}                                */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  boot_enc_{encrypt,decrypt}                                        */
/* ------------------------------------------------------------------ */
void
boot_enc_encrypt(struct enc_key_data *enc, uint32_t off,
                 uint32_t sz, uint32_t blk_off, uint8_t *buf)
{
    uint8_t nonce[16];

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

/* ------------------------------------------------------------------ */
/*  boot_enc_zeroize                                                  */
/* ------------------------------------------------------------------ */
void
boot_enc_zeroize(struct enc_key_data *enc_state)
{
    uint8_t slot;

    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        (void)boot_enc_drop(&enc_state[slot]);
    }
    memset(enc_state, 0, sizeof(struct enc_key_data) * BOOT_NUM_SLOTS);
}

#endif /* MCUBOOT_ENC_IMAGES && MCUBOOT_USE_CUSTOM_CRYPTO */
