/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 JUUL Labs
 * Copyright (c) 2021-2023 Arm Limited
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SIGN_ED25519
#include "bootutil/sign_key.h"

#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"

#include "bootutil_priv.h"
#include "bootutil/crypto/common.h"

static const uint8_t ed25519_pubkey_oid[] = MBEDTLS_OID_ISO_IDENTIFIED_ORG "\x65\x70";
#define NUM_ED25519_BYTES 32

extern int ED25519_verify(const uint8_t *message, size_t message_len,
                          const uint8_t signature[64],
                          const uint8_t public_key[32]);

/*
 * Parse the public key used for signing.
 */
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

    if (alg.MBEDTLS_CONTEXT_MEMBER(len) != sizeof(ed25519_pubkey_oid) - 1 ||
        memcmp(alg.MBEDTLS_CONTEXT_MEMBER(p), ed25519_pubkey_oid, sizeof(ed25519_pubkey_oid) - 1)) {
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

fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
  uint8_t key_id)
{
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *pubkey;
    uint8_t *end;

    if (hlen != 32 || slen != 64) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    pubkey = (uint8_t *)bootutil_keys[key_id].key;
    end = pubkey + *bootutil_keys[key_id].len;

    rc = bootutil_import_key(&pubkey, end);
    if (rc) {
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    rc = ED25519_verify(hash, 32, sig, pubkey);

    if (rc == 0) {
        /* if verify returns 0, there was an error. */
        FIH_SET(fih_rc, FIH_FAILURE);
        goto out;
    }

    FIH_SET(fih_rc, FIH_SUCCESS);
out:

    FIH_RET(fih_rc);
}

#endif /* MCUBOOT_SIGN_ED25519 */
