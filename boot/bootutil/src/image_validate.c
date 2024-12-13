/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2021 Infineon Technologies AG
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

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"
#include "bootutil/crypto/sha256.h"
#include "bootutil/sign_key.h"
#include "bootutil/security_cnt.h"
#include "bootutil/fault_injection_hardening.h"

#include "bootutil/bootutil_log.h"

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif
#if defined(MCUBOOT_SIGN_RSA)
#include "mbedtls/rsa.h"
#endif
#if defined(MCUBOOT_SIGN_EC) || defined(MCUBOOT_SIGN_EC256)
#include "mbedtls/ecdsa.h"
#endif
#if defined(MCUBOOT_ENC_IMAGES) || defined(MCUBOOT_SIGN_RSA) || \
    defined(MCUBOOT_SIGN_EC) || defined(MCUBOOT_SIGN_EC256)
#include "mbedtls/asn1.h"
#endif

#include "bootutil_priv.h"

#ifdef CYW20829
#include "cy_security_cnt_platform.h"
#endif

/*
 * Compute SHA256 over the image.
 */
static int
bootutil_img_hash(struct enc_key_data *enc_state, int image_index,
                  struct image_header *hdr, const struct flash_area *fap,
                  uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result,
                  uint8_t *seed, int seed_len)
{
    bootutil_sha256_context sha256_ctx;
    uint32_t blk_sz;
    uint32_t size;
    uint16_t hdr_size;
    uint32_t off;
    int rc;
    uint32_t blk_off;
    uint32_t tlv_off;

#if (BOOT_IMAGE_NUMBER == 1) || !defined(MCUBOOT_ENC_IMAGES) || \
    defined(MCUBOOT_RAM_LOAD)
    (void)enc_state;
    (void)image_index;
    (void)hdr_size;
    (void)blk_off;
    (void)tlv_off;
#ifdef MCUBOOT_RAM_LOAD
    (void)blk_sz;
    (void)off;
    (void)rc;
    (void)fap;
    (void)tmp_buf;
    (void)tmp_buf_sz;
#endif
#endif

#ifdef MCUBOOT_ENC_IMAGES
    /* Encrypted images only exist in the secondary slot */
    if (MUST_DECRYPT(fap, image_index, hdr) &&
            !boot_enc_valid(enc_state, image_index, fap)) {
        return -1;
    }
#endif

    bootutil_sha256_init(&sha256_ctx);

    /* in some cases (split image) the hash is seeded with data from
     * the loader image */
    if (seed && (seed_len > 0)) {
        bootutil_sha256_update(&sha256_ctx, seed, seed_len);
    }

    /* Hash is computed over image header and image itself. */
    size = hdr_size = hdr->ih_hdr_size;
    size += hdr->ih_img_size;
    tlv_off = size;

    /* If protected TLVs are present they are also hashed. */
    size += hdr->ih_protect_tlv_size;

    do
    {
#if defined(MCUBOOT_RAM_LOAD)
#if defined(MCUBOOT_MULTI_MEMORY_LOAD)
        if (IS_RAM_BOOTABLE(hdr) && IS_RAM_BOOT_STAGE())
#endif /* MCUBOOT_MULTI_MEMORY_LOAD */
        {
            bootutil_sha256_update(
                &sha256_ctx, (void *)(IMAGE_RAM_BASE + hdr->ih_load_addr), size);
                break;
        }
#endif /* MCUBOOT_RAM_LOAD */
#if !(defined(MCUBOOT_RAM_LOAD) && !defined(MCUBOOT_MULTI_MEMORY_LOAD))
        {
            for (off = 0; off < size; off += blk_sz) {
                blk_sz = size - off;
                if (blk_sz > tmp_buf_sz) {
                    blk_sz = tmp_buf_sz;
                }
#ifdef MCUBOOT_ENC_IMAGES
                /* The only data that is encrypted in an image is the payload;
                * both header and TLVs (when protected) are not.
                */
                if ((off < hdr_size) && ((off + blk_sz) > hdr_size)) {
                    /* read only the header */
                    blk_sz = hdr_size - off;
                }
                if ((off < tlv_off) && ((off + blk_sz) > tlv_off)) {
                    /* read only up to the end of the image payload */
                    blk_sz = tlv_off - off;
                }
#endif /* MCUBOOT_ENC_IMAGES */
                rc = flash_area_read(fap, off, tmp_buf, blk_sz);
                if (rc) {
                    bootutil_sha256_drop(&sha256_ctx);
                    return rc;
                }
#ifdef MCUBOOT_ENC_IMAGES
                if (MUST_DECRYPT(fap, image_index, hdr)) {
                    /* Only payload is encrypted (area between header and TLVs) */
                    if (off >= hdr_size && off < tlv_off) {
                        blk_off = (off - hdr_size) & 0xf;
#ifdef MCUBOOT_ENC_IMAGES_XIP
                        rc = bootutil_img_encrypt(enc_state, image_index, hdr, fap, off,
                                blk_sz, blk_off, tmp_buf);
#else
                        rc = boot_encrypt(enc_state, image_index, fap, off - hdr_size,
                                blk_sz, blk_off, tmp_buf);
#endif /* MCUBOOT_ENC_IMAGES_XIP */
                        if (rc) {
                            return rc;
                        }
                    }
                }
#endif /* MCUBOOT_ENC_IMAGES */
                bootutil_sha256_update(&sha256_ctx, tmp_buf, blk_sz);
            }
        }
#endif /* !MCUBOOT_RAM_LOAD || MCUBOOT_MULTI_MEMORY_LOAD */
    } while (false);
    bootutil_sha256_finish(&sha256_ctx, hash_result);
    bootutil_sha256_drop(&sha256_ctx);

    return 0;
}

/*
 * Currently, we only support being able to verify one type of
 * signature, because there is a single verification function that we
 * call.  List the type of TLV we are expecting.  If we aren't
 * configured for any signature, don't define this macro.
 */
#if (defined(MCUBOOT_SIGN_RSA)      + \
     defined(MCUBOOT_SIGN_EC)       + \
     defined(MCUBOOT_SIGN_EC256)    + \
     defined(MCUBOOT_SIGN_ED25519)) > 1
#error "Only a single signature type is supported!"
#endif

#if defined(MCUBOOT_SIGN_RSA)
#    if MCUBOOT_SIGN_RSA_LEN == 2048
#        define EXPECTED_SIG_TLV IMAGE_TLV_RSA2048_PSS
#    elif MCUBOOT_SIGN_RSA_LEN == 3072
#        define EXPECTED_SIG_TLV IMAGE_TLV_RSA3072_PSS
#    else
#        error "Unsupported RSA signature length"
#    endif
#    define SIG_BUF_SIZE (MCUBOOT_SIGN_RSA_LEN / 8)
#    define EXPECTED_SIG_LEN(x) ((x) == SIG_BUF_SIZE) /* 2048 bits */
#elif defined(MCUBOOT_SIGN_EC)
#    define EXPECTED_SIG_TLV IMAGE_TLV_ECDSA224
#    define SIG_BUF_SIZE 128
#    define EXPECTED_SIG_LEN(x)  (1) /* always true, ASN.1 will validate */
#elif defined(MCUBOOT_SIGN_EC256)
#    define EXPECTED_SIG_TLV IMAGE_TLV_ECDSA256
#    define SIG_BUF_SIZE 128
#    define EXPECTED_SIG_LEN(x)  (1) /* always true, ASN.1 will validate */
#elif defined(MCUBOOT_SIGN_ED25519)
#    define EXPECTED_SIG_TLV IMAGE_TLV_ED25519
#    define SIG_BUF_SIZE 64
#    define EXPECTED_SIG_LEN(x) ((x) == SIG_BUF_SIZE)
#else
#    define SIG_BUF_SIZE 32 /* no signing, sha256 digest only */
#endif

/* Complex result masks for bootutil_img_validate() */
#define SET_MASK_IMAGE_TLV_SHA256       ((signed)0x0000002E)
fih_int FIH_MASK_IMAGE_TLV_SHA256 = FIH_INT_INIT_GLOBAL(
        SET_MASK_IMAGE_TLV_SHA256);

#define SET_MASK_SIG_TLV_EXPECTED       ((signed)0x00007400)
fih_int FIH_MASK_SIG_TLV_EXPECTED = FIH_INT_INIT_GLOBAL(
        SET_MASK_SIG_TLV_EXPECTED);

#define SET_MASK_IMAGE_TLV_SEC_CNT      ((signed)0x005C0000)
fih_int FIH_MASK_IMAGE_TLV_SEC_CNT = FIH_INT_INIT_GLOBAL(
        SET_MASK_IMAGE_TLV_SEC_CNT);

#define CHK_MASK_IMAGE_TLV_SHA256       SET_MASK_IMAGE_TLV_SHA256

#if defined MCUBOOT_SKIP_VALIDATE_SECONDARY_SLOT
    #if defined MCUBOOT_VALIDATE_PRIMARY_SLOT
        #error Boot slot validation cannot be enabled if upgrade slot validation is disabled
    #endif
#endif

#if defined(MCUBOOT_SIGN_RSA)   || \
    defined(MCUBOOT_SIGN_EC)    || \
    defined(MCUBOOT_SIGN_EC256) || \
    defined(MCUBOOT_SIGN_ED25519)
    
    #if defined MCUBOOT_SKIP_VALIDATE_SECONDARY_SLOT
        #define CHK_MASK_SIG_TLV_EXPECTED       ((signed)0)
    #else
        #define CHK_MASK_SIG_TLV_EXPECTED       SET_MASK_SIG_TLV_EXPECTED
    #endif /* MCUBOOT_SKIP_VALIDATE_SECONDARY_SLOT */
#else
    #define CHK_MASK_SIG_TLV_EXPECTED       ((signed)0)
#endif /* defined(MCUBOOT_SIGN_RSA)   ||
          defined(MCUBOOT_SIGN_EC)    ||
          defined(MCUBOOT_SIGN_EC256) ||
          defined(MCUBOOT_SIGN_ED25519) */

#ifdef MCUBOOT_HW_ROLLBACK_PROT
    #define CHK_MASK_IMAGE_TLV_SEC_CNT      SET_MASK_IMAGE_TLV_SEC_CNT
#else
    #define CHK_MASK_IMAGE_TLV_SEC_CNT      ((signed)0)
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

fih_int FIH_IMG_VALIDATE_COMPLEX_OK = FIH_INT_INIT_GLOBAL( \
    CHK_MASK_IMAGE_TLV_SHA256  |                      \
    CHK_MASK_SIG_TLV_EXPECTED  |                      \
    CHK_MASK_IMAGE_TLV_SEC_CNT);

#undef SET_MASK_IMAGE_TLV_SHA256
#undef SET_MASK_SIG_TLV_EXPECTED
#undef SET_MASK_IMAGE_TLV_SEC_CNT

#undef CHK_MASK_IMAGE_TLV_SHA256
#undef CHK_MASK_SIG_TLV_EXPECTED
#undef CHK_MASK_IMAGE_TLV_SEC_CNT

#ifdef EXPECTED_SIG_TLV
#if !defined(MCUBOOT_HW_KEY)
static int
bootutil_find_key(uint8_t *keyhash, uint8_t keyhash_len)
{
    bootutil_sha256_context sha256_ctx;
    int i;
    const struct bootutil_key *key;
    uint8_t hash[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];

    if (keyhash_len != BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
        return -1;
    }

    for (i = 0; i < bootutil_key_cnt; i++) {
        key = &bootutil_keys[i];
        bootutil_sha256_init(&sha256_ctx);
        bootutil_sha256_update(&sha256_ctx, key->key, *key->len);
        bootutil_sha256_finish(&sha256_ctx, hash);
        if (!memcmp(hash, keyhash, keyhash_len)) {
            bootutil_sha256_drop(&sha256_ctx);
            return i;
        }
    }
    bootutil_sha256_drop(&sha256_ctx);
    return -1;
}
#else
extern unsigned int pub_key_len;
static int
bootutil_find_key(uint8_t image_index, uint8_t *key, uint16_t key_len)
{
    bootutil_sha256_context sha256_ctx;
    uint8_t hash[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t key_hash[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    size_t key_hash_size = sizeof(key_hash);
    int rc;
    fih_int fih_rc;

    bootutil_sha256_init(&sha256_ctx);
    bootutil_sha256_update(&sha256_ctx, key, key_len);
    bootutil_sha256_finish(&sha256_ctx, hash);
    bootutil_sha256_drop(&sha256_ctx);

    rc = boot_retrieve_public_key_hash(image_index, key_hash, &key_hash_size);
    if (rc) {
        return rc;
    }

    /* Adding hardening to avoid this potential attack:
     *  - Image is signed with an arbitrary key and the corresponding public
     *    key is added as a TLV field.
     * - During public key validation (comparing against key-hash read from
     *   HW) a fault is injected to accept the public key as valid one.
     */
    FIH_CALL(boot_fih_memequal, fih_rc, hash, key_hash, key_hash_size);
    if (fih_eq(fih_rc, FIH_SUCCESS)) {
        bootutil_keys[0].key = key;
        pub_key_len = key_len;
        return 0;
    }

    return -1;
}
#endif /* !MCUBOOT_HW_KEY */
#endif

#ifdef MCUBOOT_HW_ROLLBACK_PROT
/**
 * Reads the value of an image's security counter.
 *
 * @param hdr           Pointer to the image header structure.
 * @param fap           Pointer to a description structure of the image's
 *                      flash area.
 * @param security_cnt  Pointer to store the security counter value.
 *
 * @return              FIH_SUCCESS on success; FIH_FAILURE on failure.
 */
fih_int
bootutil_get_img_security_cnt(struct image_header *hdr,
                              const struct flash_area *fap,
                              fih_uint *img_security_cnt)
{
    uint32_t img_sec_cnt = 0u;
    uint32_t img_chk_cnt = 0u;
    struct image_tlv_iter it = {0};
    uint32_t off = 0u;
    uint16_t len = 0u;
    int32_t rc = -1;
    fih_int fih_rc = FIH_FAILURE;

    if ((NULL == hdr) ||
        (NULL == fap) ||
        (NULL == img_security_cnt)) {
        /* Invalid parameter. */
        goto out;
    }

    /* The security counter TLV is in the protected part of the TLV area. */
    if (0u == hdr->ih_protect_tlv_size) {
        goto out;
    }

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_SEC_CNT, true);
    if (rc != 0) {
        goto out;
    }

    /* Traverse through the protected TLV area to find
     * the security counter TLV.
     */

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        /* Security counter TLV has not been found. */
        goto out;
    }

    if (len != sizeof(img_sec_cnt)) {
        /* Security counter is not valid. */
        goto out;
    }

    rc = LOAD_IMAGE_DATA(hdr, fap, off, &img_sec_cnt, len);
    if (rc != 0) {
        goto out;
    }

    *img_security_cnt = fih_uint_encode(img_sec_cnt);

    rc = LOAD_IMAGE_DATA(hdr, fap, off, &img_chk_cnt, len);
    if (rc != 0) {
        goto out;
    }

    if (fih_uint_eq(fih_uint_encode(img_chk_cnt), *img_security_cnt)) {

        if (img_sec_cnt == img_chk_cnt) {
            fih_rc = FIH_SUCCESS;
        }
    }

out:
    FIH_RET(fih_rc);
}
#ifdef CYW20829
/**
 * Reads the content of an image's reprovisioning packet.
 *
 * @param hdr            Pointer to the image header structure.
 * @param fap            Pointer to a description structure of the image's
 *                       flash area.
 * @param reprov_packet  Pointer to store the reprovisioning packet.
 *
 * @return               0 on success; nonzero on failure.
 */
int32_t
bootutil_get_img_reprov_packet(struct image_header *hdr,
                              const struct flash_area *fap,
                              uint8_t *reprov_packet)
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    int32_t rc;

    if ((hdr == NULL) ||
        (fap == NULL) ||
        (reprov_packet == NULL)) {
        /* Invalid parameter. */
        return BOOT_EBADARGS;
    }

    /* The reprovisioning packet TLV is in the protected part of the TLV area. */
    if (hdr->ih_protect_tlv_size == 0) {
        return BOOT_EBADIMAGE;
    }

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_PROV_PACK, true);
    if (rc) {
        return rc;
    }

    /* Traverse through the protected TLV area to find
     * the reprovisioning apcket TLV.
     */

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        /* Reprovisioning packet TLV has not been found. */
        return -1;
    }

    if (len != REPROV_PACK_SIZE) {
        /* Reprovisioning packet is not valid. */
        return BOOT_EBADIMAGE;
    }

    rc = LOAD_IMAGE_DATA(hdr, fap, off, reprov_packet, len);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}
#endif /* CYW20289 */
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

/*
 * Verify the integrity of the image.
 * Return non-zero if image could not be validated/does not validate.
 */
fih_int
bootutil_img_validate(struct enc_key_data *enc_state, int image_index,
                      struct image_header *hdr, const struct flash_area *fap,
                      uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *seed,
                      int seed_len, uint8_t *out_hash)
{
    uint32_t off;
    uint16_t len;
    uint16_t type;

#ifdef EXPECTED_SIG_TLV
    fih_int valid_signature = FIH_FAILURE;
    int key_id = -1;
#ifdef MCUBOOT_HW_KEY
    /* Few extra bytes for encoding and for public exponent. */
    uint8_t key_buf[SIG_BUF_SIZE + 24];
#endif
#endif /* EXPECTED_SIG_TLV */

    struct image_tlv_iter it;
    uint8_t buf[SIG_BUF_SIZE];
    uint8_t hash[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    int rc = 0;
    fih_int fih_rc = FIH_FAILURE;
    /* fih_complex_result stores patterns of successful execution
     * of required checks
     */
    fih_int fih_complex_result = FIH_INT_ZERO;

#ifdef MCUBOOT_HW_ROLLBACK_PROT
    fih_uint security_cnt = FIH_UINT_MAX;
    uint32_t img_security_cnt = 0;
#ifdef CYW20829
    uint8_t reprov_packet[REPROV_PACK_SIZE];
    fih_int security_counter_valid = FIH_FAILURE;
    fih_uint extracted_img_cnt = FIH_UINT_MAX;
#endif /* CYW20829 */
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

    rc = bootutil_img_hash(enc_state, image_index, hdr, fap, tmp_buf,
            tmp_buf_sz, hash, seed, seed_len);
    if (rc) {
        goto out;
    }

    if (out_hash) {
        (void)memcpy(out_hash, hash, BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE);
    }

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_ANY, false);
    if (rc) {
        goto out;
    }

    /*
     * Traverse through all of the TLVs, performing any checks we know
     * and are able to do.
     */
    while (true) {
        rc = bootutil_tlv_iter_next(&it, &off, &len, &type);
        if (rc < 0) {
            goto out;
        } else if (rc > 0) {
            break;
        }

        if (type == IMAGE_TLV_SHA256) {
            /*
             * Verify the SHA256 image hash.  This must always be
             * present.
             */
            if (len != sizeof(hash)) {
                rc = -1;
                goto out;
            }
            rc = LOAD_IMAGE_DATA(hdr, fap, off, buf, sizeof(hash));
            if (rc) {
                goto out;
            }

            FIH_CALL(boot_fih_memequal, fih_rc, hash, buf, sizeof(hash));

            if (fih_eq(fih_rc, FIH_SUCCESS)) {
                /* Encode succesful completion pattern to complex_result */
                fih_complex_result = fih_or(fih_complex_result,
                                                 FIH_MASK_IMAGE_TLV_SHA256);
            }
            else {
                BOOT_LOG_DBG("IMAGE_TLV_SHA256 is invalid");
                rc = -1;
                goto out;
            }

#ifdef EXPECTED_SIG_TLV
#ifndef MCUBOOT_HW_KEY
        } else if (type == IMAGE_TLV_KEYHASH) {
            /*
             * Determine which key we should be checking.
             */
            if (len != BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
                rc = -1;
                goto out;
            }
            rc = LOAD_IMAGE_DATA(hdr, fap, off, buf, len);
            if (rc) {
                goto out;
            }
            key_id = bootutil_find_key(buf, len);
            /*
             * The key may not be found, which is acceptable.  There
             * can be multiple signatures, each preceded by a key.
             */
#else
        } else if (type == IMAGE_TLV_PUBKEY) {
            /*
             * Determine which key we should be checking.
             */
            if (len > sizeof(key_buf)) {
                rc = -1;
                goto out;
            }
            rc = LOAD_IMAGE_DATA(hdr, fap, off, key_buf, len);
            if (rc) {
                goto out;
            }
            key_id = bootutil_find_key(image_index, key_buf, len);
            /*
             * The key may not be found, which is acceptable.  There
             * can be multiple signatures, each preceded by a key.
             */
#endif /* !MCUBOOT_HW_KEY */
        } else if (type == EXPECTED_SIG_TLV) {
            /* Ignore this signature if it is out of bounds. */
            if (key_id < 0 || key_id >= bootutil_key_cnt) {
                key_id = -1;
                continue;
            }
            if (!EXPECTED_SIG_LEN(len) || len > sizeof(buf)) {
                rc = -1;
                goto out;
            }
            rc = LOAD_IMAGE_DATA(hdr, fap, off, buf, len);
            if (rc) {
                goto out;
            }
            FIH_CALL(bootutil_verify_sig, valid_signature, hash, sizeof(hash),
                                                           buf, len, key_id);
            key_id = -1;

            if (fih_eq(FIH_SUCCESS, valid_signature)) {
                /* Encode succesful completion pattern to complex_result */
                fih_complex_result = fih_or(fih_complex_result,
                                                 FIH_MASK_SIG_TLV_EXPECTED);
            } else {
                BOOT_LOG_DBG("Invalid signature of bootable image %d",
                             image_index);
                rc = -1;
                goto out;
            }
            
#endif /* EXPECTED_SIG_TLV */
#ifdef MCUBOOT_HW_ROLLBACK_PROT
        } else if (type == IMAGE_TLV_SEC_CNT) {
            /*
             * Verify the image's security counter.
             * This must always be present.
             */
            if (len != sizeof(img_security_cnt)) {
                /* Security counter is not valid. */
                rc = -1;
                goto out;
            }

            rc = LOAD_IMAGE_DATA(hdr, fap, off, &img_security_cnt, len);
            if (rc) {
                goto out;
            }

            FIH_CALL(boot_nv_security_counter_get, fih_rc, image_index,
                                                           &security_cnt);
            if (!fih_eq(fih_rc, FIH_SUCCESS)) {
                rc = -1;
                goto out;
            }

            BOOT_LOG_DBG("NV Counter read from efuse = %u", fih_uint_decode(security_cnt));

#ifdef CYW20829
            BOOT_LOG_DBG("Full NV Counter read from image = 0x%X", img_security_cnt);

            FIH_CALL(platform_security_counter_check_extract, fih_rc, 
                    (uint32_t)image_index, fih_uint_encode(img_security_cnt), &extracted_img_cnt);

            if (!fih_eq(fih_rc, FIH_SUCCESS)) {
                /* The image's security counter exceeds registered value for this image */
                rc = -1;
                goto out;
            }

            img_security_cnt = fih_uint_decode(extracted_img_cnt);
#endif /*CYW20829*/

            BOOT_LOG_DBG("NV Counter read from image = %" PRIu32, img_security_cnt);

            /* Compare the new image's security counter value against the
             * stored security counter value.
             */
            if (fih_uint_ge(fih_uint_encode(img_security_cnt), security_cnt)) {
                /* Encode succesful completion pattern to complex_result */
                fih_complex_result = fih_or(fih_complex_result,
                                                 FIH_MASK_IMAGE_TLV_SEC_CNT);
#ifdef CYW20829
                /* The image's security counter has been successfully verified. */
                security_counter_valid = FIH_INT_INIT(HW_ROLLBACK_CNT_VALID);
#endif
            } else {
                /* The image's security counter is not accepted. */
                rc = -1;
                goto out;
            }

#ifdef CYW20829
        } else if (type == IMAGE_TLV_PROV_PACK) {

            if (fih_eq(security_counter_valid, FIH_INT_INIT(HW_ROLLBACK_CNT_VALID))) {
                /*
                * Verify the image reprovisioning packet.
                * This must always be present.
                */
                BOOT_LOG_INF("Prov packet length 0x51 TLV = %" PRIu16, len);

                if (len != sizeof(reprov_packet)) {
                    /* Re-provisioning packet is not valid. */
                    rc = -1;
                    goto out;
                }

                rc = LOAD_IMAGE_DATA(hdr, fap, off, reprov_packet, len);
                if (rc) {
                    goto out;
                }

                security_counter_valid = fih_int_encode(fih_int_decode(security_counter_valid) | REPROV_PACK_VALID);
            }
            else {
                rc = -1;
                goto out;
            }
#endif /* CYW20829 */
#endif /* MCUBOOT_HW_ROLLBACK_PROT */
        }
    }

#ifdef MCUBOOT_HW_ROLLBACK_PROT
#ifdef CYW20829
    if (!fih_eq(security_counter_valid, FIH_INT_INIT(REPROV_PACK_VALID | HW_ROLLBACK_CNT_VALID))) {
        BOOT_LOG_DBG("Reprovisioning packet TLV 0x51 is not present image = %d", image_index);
        rc = -1;
        goto out;
    }
#endif /* CYW20829 */
#endif /* MCUBOOT_HW_ROLLBACK_PROT */

out:
    if (rc < 0) {
        fih_complex_result = FIH_FAILURE;
    }

    FIH_RET(fih_complex_result);
}
