/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2018 Linaro LTD
 * Copyright (c) 2017-2019 JUUL Labs
 * Copyright (c) 2020-2023 Arm Limited
 *
 * Original license:
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SIGN_RSA
#include "bootutil_priv.h"
#include "bootutil/sign_key.h"
#include "bootutil/fault_injection_hardening.h"

#define BOOTUTIL_CRYPTO_RSA_SIGN_ENABLED
#include "bootutil/crypto/rsa.h"

/* PSA Crypto APIs provide an integrated API to perform the verification
 * while for other crypto backends we need to implement each step at this
 * abstraction level
 */
#if !defined(MCUBOOT_USE_PSA_CRYPTO)

#include "bootutil/crypto/sha.h"

/*
 * Constants for this particular constrained implementation of
 * RSA-PSS. In particular, we support RSA 2048, with a SHA256 hash,
 * and a 32-byte salt.  A signature with different parameters will be
 * rejected as invalid.
 */

/* The size, in octets, of the message. */
#define PSS_EMLEN (MCUBOOT_SIGN_RSA_LEN / 8)

/* The size of the hash function.  For SHA256, this is 32 bytes. */
#define PSS_HLEN 32

/* Size of the salt, should be fixed. */
#define PSS_SLEN 32

/* The length of the mask: emLen - hLen - 1. */
#define PSS_MASK_LEN (PSS_EMLEN - PSS_HLEN - 1)

#define PSS_HASH_OFFSET PSS_MASK_LEN

/* For the mask itself, how many bytes should be all zeros. */
#define PSS_MASK_ZERO_COUNT (PSS_MASK_LEN - PSS_SLEN - 1)
#define PSS_MASK_ONE_POS   PSS_MASK_ZERO_COUNT

/* Where the salt starts. */
#define PSS_MASK_SALT_POS   (PSS_MASK_ONE_POS + 1)

static const uint8_t pss_zeros[8] = {0};

/*
 * Compute the RSA-PSS mask-generation function, MGF1.  Assumptions
 * are that the mask length will be less than 256 * PSS_HLEN, and
 * therefore we never need to increment anything other than the low
 * byte of the counter.
 *
 * This is described in PKCS#1, B.2.1.
 */
static void
pss_mgf1(uint8_t *mask, const uint8_t *hash)
{
    bootutil_sha_context ctx;
    uint8_t counter[4] = { 0, 0, 0, 0 };
    uint8_t htmp[PSS_HLEN];
    int count = PSS_MASK_LEN;
    int bytes;

    while (count > 0) {
        bootutil_sha_init(&ctx);
        bootutil_sha_update(&ctx, hash, PSS_HLEN);
        bootutil_sha_update(&ctx, counter, 4);
        bootutil_sha_finish(&ctx, htmp);

        counter[3]++;

        bytes = PSS_HLEN;
        if (bytes > count)
            bytes = count;

        memcpy(mask, htmp, bytes);
        mask += bytes;
        count -= bytes;
    }

    bootutil_sha_drop(&ctx);
}

/*
 * Validate an RSA signature, using RSA-PSS, as described in PKCS #1
 * v2.2, section 9.1.2, with many parameters required to have fixed
 * values. RSASSA-PSS-VERIFY RFC8017 section 8.1.2
 */
static fih_ret
bootutil_cmp_rsasig(bootutil_rsa_context *ctx, uint8_t *hash, uint32_t hlen,
  uint8_t *sig, size_t slen)
{
    bootutil_sha_context shactx;
    uint8_t em[MBEDTLS_MPI_MAX_SIZE];
    uint8_t db_mask[PSS_MASK_LEN];
    uint8_t h2[PSS_HLEN];
    int i;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* The caller has already verified that slen == bootutil_rsa_get_len(ctx) */
    if (slen != PSS_EMLEN ||
        PSS_EMLEN > MBEDTLS_MPI_MAX_SIZE) {
        goto out;
    }

    if (hlen != PSS_HLEN) {
        goto out;
    }

    /* Apply RSAVP1 to produce em = sig^E mod N using the public key */
    if (bootutil_rsa_public(ctx, sig, em)) {
        goto out;
    }

    /*
     * PKCS #1 v2.2, 9.1.2 EMSA-PSS-Verify
     *
     * emBits is 2048
     * emLen = ceil(emBits/8) = 256
     *
     * The salt length is not known at the beginning.
     */

    /* Step 1.  The message is constrained by the address space of a
     * 32-bit processor, which is far less than the 2^61-1 limit of
     * SHA-256.
     */

    /* Step 2.  mHash is passed in as 'hash', with hLen the hlen
     * argument. */

    /* Step 3.  if emLen < hLen + sLen + 2, inconsistent and stop.
     * The salt length is not known at this point.
     */

    /* Step 4.  If the rightmost octet of EM does have the value
     * 0xbc, output inconsistent and stop.
     */
    if (em[PSS_EMLEN - 1] != 0xbc) {
        goto out;
    }

    /* Step 5.  Let maskedDB be the leftmost emLen - hLen - 1 octets
     * of EM, and H be the next hLen octets.
     *
     * maskedDB is then the first 256 - 32 - 1 = 0-222
     * H is 32 bytes 223-254
     */

    /* Step 6.  If the leftmost 8emLen - emBits bits of the leftmost
     * octet in maskedDB are not all equal to zero, output
     * inconsistent and stop.
     *
     * 8emLen - emBits is zero, so there is nothing to test here.
     */

    /* Step 7.  let dbMask = MGF(H, emLen - hLen - 1). */
    pss_mgf1(db_mask, &em[PSS_HASH_OFFSET]);

    /* Step 8.  let DB = maskedDB xor dbMask.
     * To avoid needing an additional buffer, store the 'db' in the
     * same buffer as db_mask.  From now, to the end of this function,
     * db_mask refers to the unmasked 'db'. */
    for (i = 0; i < PSS_MASK_LEN; i++) {
        db_mask[i] ^= em[i];
    }

    /* Step 9.  Set the leftmost 8emLen - emBits bits of the leftmost
     * octet in DB to zero.
     * pycrypto seems to always make the emBits 2047, so we need to
     * clear the top bit. */
    db_mask[0] &= 0x7F;

    /* Step 10.  If the emLen - hLen - sLen - 2 leftmost octets of DB
     * are not zero or if the octet at position emLen - hLen - sLen -
     * 1 (the leftmost position is "position 1") does not have
     * hexadecimal value 0x01, output "inconsistent" and stop. */
    for (i = 0; i < PSS_MASK_ZERO_COUNT; i++) {
        if (db_mask[i] != 0) {
            goto out;
        }
    }

    if (db_mask[PSS_MASK_ONE_POS] != 1) {
        goto out;
    }

    /* Step 11. Let salt be the last sLen octets of DB */

    /* Step 12.  Let M' = 0x00 00 00 00 00 00 00 00 || mHash || salt; */

    /* Step 13.  Let H' = Hash(M') */
    bootutil_sha_init(&shactx);
    bootutil_sha_update(&shactx, pss_zeros, 8);
    bootutil_sha_update(&shactx, hash, PSS_HLEN);
    bootutil_sha_update(&shactx, &db_mask[PSS_MASK_SALT_POS], PSS_SLEN);
    bootutil_sha_finish(&shactx, h2);
    bootutil_sha_drop(&shactx);

    /* Step 14.  If H = H', output "consistent".  Otherwise, output
     * "inconsistent". */
    FIH_CALL(boot_fih_memequal, fih_rc, h2, &em[PSS_HASH_OFFSET], PSS_HLEN);

out:
    FIH_RET(fih_rc);
}

#else /* MCUBOOT_USE_PSA_CRYPTO */

static fih_ret
bootutil_cmp_rsasig(bootutil_rsa_context *ctx, uint8_t *hash, uint32_t hlen,
  uint8_t *sig, size_t slen)
{
    int rc = -1;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* PSA Crypto APIs allow the verification in a single call */
    rc = bootutil_rsassa_pss_verify(ctx, hash, hlen, sig, slen);

    fih_rc = fih_ret_encode_zero_equality(rc);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

    FIH_RET(fih_rc);
}

#endif /* MCUBOOT_USE_PSA_CRYPTO */

fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
  uint8_t key_id)
{
    bootutil_rsa_context ctx;
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *cp;
    uint8_t *end;

    bootutil_rsa_init(&ctx);

    cp = (uint8_t *)bootutil_keys[key_id].key;
    end = cp + *bootutil_keys[key_id].len;

    /* The key used for signature verification is a public RSA key */
    rc = bootutil_rsa_parse_public_key(&ctx, &cp, end);
    if (rc || slen != bootutil_rsa_get_len(&ctx)) {
        goto out;
    }
    FIH_CALL(bootutil_cmp_rsasig, fih_rc, &ctx, hash, hlen, sig, slen);

out:
    bootutil_rsa_drop(&ctx);

    FIH_RET(fih_rc);
}
#endif /* MCUBOOT_SIGN_RSA */
