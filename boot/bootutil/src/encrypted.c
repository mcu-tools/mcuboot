/*
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

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)
#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "hal/hal_flash.h"

#if defined(MCUBOOT_ENCRYPT_RSA)
#include "mbedtls/rsa.h"
#include "mbedtls/asn1.h"
#endif

#if defined(MCUBOOT_ENCRYPT_KW)
#  if defined(MCUBOOT_USE_MBED_TLS)
#    include "mbedtls/nist_kw.h"
#    include "mbedtls/aes.h"
#  else
#    include "tinycrypt/aes.h"
#  endif
#endif

#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"

#include "bootutil_priv.h"

#define TLV_ENC_RSA_SZ  256
#define TLV_ENC_KW_SZ   24

#if defined(MCUBOOT_ENCRYPT_KW)
#if defined(MCUBOOT_USE_MBED_TLS)
static int
key_unwrap(uint8_t *wrapped, uint8_t *enckey)
{
    mbedtls_nist_kw_context kw;
    int rc;
    size_t olen;

    mbedtls_nist_kw_init(&kw);

    rc = mbedtls_nist_kw_setkey(&kw, MBEDTLS_CIPHER_ID_AES,
            bootutil_enc_key.key, *bootutil_enc_key.len * 8, 0);
    if (rc) {
        goto done;
    }

    rc = mbedtls_nist_kw_unwrap(&kw, MBEDTLS_KW_MODE_KW, wrapped, TLV_ENC_KW_SZ,
            enckey, &olen, BOOT_ENC_KEY_SIZE);

done:
    mbedtls_nist_kw_free(&kw);
    return rc;
}
#else /* !MCUBOOT_USE_MBED_TLS */
/*
 * Implements AES key unwrapping following RFC-3394 section 2.2.2, using
 * tinycrypt for AES-128 decryption.
 */
static int
key_unwrap(uint8_t *wrapped, uint8_t *enckey)
{
    struct tc_aes_key_sched_struct aes;
    uint8_t A[8];
    uint8_t B[16];
    int8_t i, j, k;

    if (tc_aes128_set_decrypt_key(&aes, bootutil_enc_key.key) == 0) {
        return -1;
    }

    for (k = 0; k < 8; k++) {
        A[k]          = wrapped[k];
        enckey[k]     = wrapped[8 + k];
        enckey[8 + k] = wrapped[16 + k];
    }

    for (j = 5; j >= 0; j--) {
        for (i = 2; i > 0; i--) {
            for (k = 0; k < 8; k++) {
                B[k] = A[k];
                B[8 + k] = enckey[((i-1) * 8) + k];
            }
            B[7] ^= 2 * j + i;
            if (tc_aes_decrypt((uint8_t *)&B, (uint8_t *)&B, &aes) == 0) {
                return -1;
            }
            for (k = 0; k < 8; k++) {
                A[k] = B[k];
                enckey[((i-1) * 8) + k] = B[8 + k];
            }
        }
    }

    for (i = 0, k = 0; i < 8; i++) {
        k |= A[i] ^ 0xa6;
    }
    if (k) {
        return -1;
    }
    return 0;
}
#endif /* MCUBOOT_USE_MBED_TLS */
#endif /* MCUBOOT_ENCRYPT_KW */

#if defined(MCUBOOT_ENCRYPT_RSA)
static int
parse_enckey(mbedtls_rsa_context *ctx, uint8_t **p, uint8_t *end)
{
    int rc;
    size_t len;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    if ( /* version */
        mbedtls_asn1_get_int(p, end, &ctx->ver) != 0 ||
         /* public modulus */
        mbedtls_asn1_get_mpi(p, end, &ctx->N) != 0 ||
         /* public exponent */
        mbedtls_asn1_get_mpi(p, end, &ctx->E) != 0 ||
         /* private exponent */
        mbedtls_asn1_get_mpi(p, end, &ctx->D) != 0 ||
         /* primes */
        mbedtls_asn1_get_mpi(p, end, &ctx->P) != 0 ||
        mbedtls_asn1_get_mpi(p, end, &ctx->Q) != 0 ||
         /* d mod (p-1) and d mod (q-1) */
        mbedtls_asn1_get_mpi(p, end, &ctx->DP) != 0 ||
        mbedtls_asn1_get_mpi(p, end, &ctx->DQ) != 0 ||
         /* q ^ (-1) mod p */
        mbedtls_asn1_get_mpi(p, end, &ctx->QP) != 0) {
        return -3;
    }

    ctx->len = mbedtls_mpi_size(&ctx->N);

    if (*p != end) {
        return -4;
    }

    if (mbedtls_rsa_check_pubkey(ctx) != 0 ||
        mbedtls_rsa_check_privkey(ctx) != 0) {
        return -5;
    }

    return 0;
}
#endif

int
boot_enc_set_key(struct enc_key_data *enc_state, uint8_t slot, uint8_t *enckey)
{
    int rc;

#if defined(MCUBOOT_USE_MBED_TLS)
    mbedtls_aes_init(&enc_state[slot].aes);
    rc = mbedtls_aes_setkey_enc(&enc_state[slot].aes, enckey, BOOT_ENC_KEY_SIZE_BITS);
    if (rc) {
        mbedtls_aes_free(&enc_state[slot].aes);
        return -1;
    }
#else
    (void)rc;

    /* set_encrypt and set_decrypt do the same thing in tinycrypt */
    tc_aes128_set_encrypt_key(&enc_state[slot].aes, enckey);
#endif

    enc_state[slot].valid = 1;

    return 0;
}

#if defined(MCUBOOT_ENCRYPT_RSA)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_RSA2048
#    define EXPECTED_ENC_LEN    TLV_ENC_RSA_SZ
#elif defined(MCUBOOT_ENCRYPT_KW)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_KW128
#    define EXPECTED_ENC_LEN    TLV_ENC_KW_SZ
#endif

/*
 * Load encryption key.
 */
int
boot_enc_load(struct enc_key_data *enc_state, int image_index,
        const struct image_header *hdr, const struct flash_area *fap,
        uint8_t *enckey)
{
#if defined(MCUBOOT_ENCRYPT_RSA)
    mbedtls_rsa_context rsa;
    uint8_t *cp;
    uint8_t *cpend;
    size_t olen;
#endif
    uint32_t off;
    uint16_t len;
    struct image_tlv_iter it;
    uint8_t buf[TLV_ENC_RSA_SZ];
    uint8_t slot;
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, fap->fa_id);
    if (rc < 0) {
        return rc;
    }
    slot = rc;

    /* Already loaded... */
    if (enc_state[slot].valid) {
        return 1;
    }

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, EXPECTED_ENC_TLV, false);
    if (rc) {
        return -1;
    }

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        return rc;
    }

    if (len != EXPECTED_ENC_LEN) {
        return -1;
    }

    rc = flash_area_read(fap, off, buf, EXPECTED_ENC_LEN);
    if (rc) {
        return -1;
    }

#if defined(MCUBOOT_ENCRYPT_RSA)
    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    cp = (uint8_t *)bootutil_enc_key.key;
    cpend = cp + *bootutil_enc_key.len;

    rc = parse_enckey(&rsa, &cp, cpend);
    if (rc) {
        mbedtls_rsa_free(&rsa);
        return rc;
    }

    rc = mbedtls_rsa_rsaes_oaep_decrypt(&rsa, NULL, NULL, MBEDTLS_RSA_PRIVATE,
            NULL, 0, &olen, buf, enckey, BOOT_ENC_KEY_SIZE);
    mbedtls_rsa_free(&rsa);

#elif defined(MCUBOOT_ENCRYPT_KW)
    assert(*bootutil_enc_key.len == 16);
    rc = key_unwrap(buf, enckey);
#endif

    return rc;
}

bool
boot_enc_valid(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap)
{
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, fap->fa_id);
    if (rc < 0) {
        /* can't get proper slot number - skip encryption, */
        /* postpone the error for a upper layer */
        return false;
    }

    return enc_state[rc].valid;
}

void
boot_encrypt(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap, uint32_t off, uint32_t sz,
        uint32_t blk_off, uint8_t *buf)
{
    struct enc_key_data *enc;
    uint32_t i, j;
    uint8_t u8;
    uint8_t nonce[16];
    uint8_t blk[16];
    int rc;

    memset(nonce, 0, 12);
    off >>= 4;
    nonce[12] = (uint8_t)(off >> 24);
    nonce[13] = (uint8_t)(off >> 16);
    nonce[14] = (uint8_t)(off >> 8);
    nonce[15] = (uint8_t)off;

    rc = flash_area_id_to_multi_image_slot(image_index, fap->fa_id);
    if (rc < 0) {
        assert(0);
        return;
    }

    enc = &enc_state[rc];
    assert(enc->valid == 1);
    for (i = 0; i < sz; i++) {
        if (i == 0 || blk_off == 0) {
#if defined(MCUBOOT_USE_MBED_TLS)
            mbedtls_aes_crypt_ecb(&enc->aes, MBEDTLS_AES_ENCRYPT, nonce, blk);
#else
            tc_aes_encrypt(blk, nonce, &enc->aes);
#endif

            for (j = 16; j > 0; --j) {
                if (++nonce[j - 1] != 0) {
                    break;
                }
            }
        }

        u8 = *buf;
        *buf++ = u8 ^ blk[blk_off];
        blk_off = (blk_off + 1) & 0x0f;
    }
}

/**
 * Clears encrypted state after use.
 */
void
boot_enc_zeroize(struct enc_key_data *enc_state)
{
    memset(enc_state, 0, sizeof(struct enc_key_data) * BOOT_NUM_SLOTS);
}

#endif /* MCUBOOT_ENC_IMAGES */
