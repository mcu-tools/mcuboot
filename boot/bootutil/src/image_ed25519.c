/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 JUUL Labs
 * Copyright (c) 2021-2023 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SIGN_ED25519
#include "bootutil/sign_key.h"

#if !defined(MCUBOOT_KEY_IMPORT_BYPASS_ASN)
/* We are not really using the MBEDTLS but need the ASN.1 parsing functions */
#define MBEDTLS_ASN1_PARSE_C
#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"
#include "bootutil/crypto/common.h"
#endif

#include "bootutil_priv.h"
#include "bootutil/crypto/sha.h"

#define EDDSA_SIGNATURE_LENGTH 64
#define NUM_ED25519_BYTES 32

extern int ED25519_verify(const uint8_t *message, size_t message_len,
                          const uint8_t signature[EDDSA_SIGNATURE_LENGTH],
                          const uint8_t public_key[NUM_ED25519_BYTES]);

#if !defined(MCUBOOT_KEY_IMPORT_BYPASS_ASN)
/*
 * Parse the public key used for signing.
 */
static const uint8_t ed25519_pubkey_oid[] = MBEDTLS_OID_ISO_IDENTIFIED_ORG "\x65\x70";

static int
bootutil_import_key(uint8_t **cp, uint8_t *end)
{
    size_t len;
    mbedtls_asn1_buf alg;
    mbedtls_asn1_buf param;

    if (mbedtls_asn1_get_tag(cp, end, &len,
        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        return -1;
    }
    end = *cp + len;

    if (mbedtls_asn1_get_alg(cp, end, &alg, &param)) {
        return -2;
    }

    if (alg.ASN1_CONTEXT_MEMBER(len) != sizeof(ed25519_pubkey_oid) - 1 ||
        memcmp(alg.ASN1_CONTEXT_MEMBER(p), ed25519_pubkey_oid, sizeof(ed25519_pubkey_oid) - 1)) {
        return -3;
    }

    if (mbedtls_asn1_get_bitstring_null(cp, end, &len)) {
        return -4;
    }
    if (*cp + len != end) {
        return -5;
    }

    if (len != NUM_ED25519_BYTES) {
        return -6;
    }

    return 0;
}
#endif /* !defined(MCUBOOT_KEY_IMPORT_BYPASS_ASN) */

/* Signature verification base function.
 * The function takes buffer of specified length and tries to verify
 * it against provided signature.
 * The function does key import and checks whether signature is
 * of expected length.
 */
static fih_ret
bootutil_verify(uint8_t *buf, uint32_t blen,
                uint8_t *sig, size_t slen,
                uint8_t key_id)
{
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *pubkey;
    uint8_t *end;

    if (blen != IMAGE_HASH_SIZE || slen != EDDSA_SIGNATURE_LENGTH) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    pubkey = (uint8_t *)bootutil_keys[key_id].key;
    end = pubkey + *bootutil_keys[key_id].len;

#if !defined(MCUBOOT_KEY_IMPORT_BYPASS_ASN)
    rc = bootutil_import_key(&pubkey, end);
    if (rc) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }
#else
    /* Directly use the key contents from the ASN stream,
     * these are the last NUM_ED25519_BYTES.
     * There is no check whether this is the correct key,
     * here, by the algorithm selected.
     */
    if (*bootutil_keys[key_id].len < NUM_ED25519_BYTES) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    pubkey = end - NUM_ED25519_BYTES;
#endif

    rc = ED25519_verify(buf, blen, sig, pubkey);

    if (rc == 0) {
        /* if verify returns 0, there was an error. */
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    FIH_SET(fih_rc, FIH_SUCCESS);
out:

    FIH_RET(fih_rc);
}

/* Hash signature verification function.
 * Verifies hash against provided signature.
 * The function verifies that hash is of expected size and then
 * calls bootutil_verify to do the signature verification.
 */
fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen,
                    uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    if (hlen != IMAGE_HASH_SIZE) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    FIH_CALL(bootutil_verify, fih_rc, hash, IMAGE_HASH_SIZE, sig,
             slen, key_id);

out:
    FIH_RET(fih_rc);
}

/* Image verification function.
 * The function directly calls bootutil_verify to verify signature
 * of image.
 */
fih_ret
bootutil_verify_img(uint8_t *img, uint32_t size,
                    uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    FIH_CALL(bootutil_verify, fih_rc, img, size, sig,
             slen, key_id);

    FIH_RET(fih_rc);
}

#endif /* MCUBOOT_SIGN_ED25519 */
