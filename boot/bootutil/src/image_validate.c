/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2024 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
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
#include <errno.h>

#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/sign_key.h"
#include "bootutil/security_cnt.h"
#include "bootutil/fault_injection_hardening.h"

#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif
#if defined(MCUBOOT_SIGN_RSA)
#include "mbedtls/rsa.h"
#endif
#if defined(MCUBOOT_SIGN_EC256)
#include "mbedtls/ecdsa.h"
#endif
#if defined(MCUBOOT_ENC_IMAGES) || defined(MCUBOOT_SIGN_RSA) || \
    defined(MCUBOOT_SIGN_EC256)
#include "mbedtls/asn1.h"
#endif

#include "bootutil_priv.h"

#ifndef MCUBOOT_SIGN_PURE
/*
 * Compute SHA hash over the image.
 * (SHA384 if ECDSA-P384 is being used,
 *  SHA256 otherwise).
 */
static int
bootutil_img_hash(struct boot_loader_state *state,
                  struct image_header *hdr, const struct flash_area *fap,
                  uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *hash_result,
                  uint8_t *seed, int seed_len
                 )
{
    bootutil_sha_context sha_ctx;
    uint32_t size;
    uint16_t hdr_size;
    uint32_t blk_off;
    uint32_t tlv_off;
#if !defined(MCUBOOT_HASH_STORAGE_DIRECTLY)
    int rc;
    uint32_t off;
    uint32_t blk_sz;
#endif
#ifdef MCUBOOT_HASH_STORAGE_DIRECTLY
    uintptr_t base = 0;
    int fa_ret;
#endif
#if defined(MCUBOOT_ENC_IMAGES)
    struct enc_key_data *enc_state;
    int image_index;
#endif
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    uint32_t sector_off = 0;
#endif

#if (BOOT_IMAGE_NUMBER == 1) || !defined(MCUBOOT_ENC_IMAGES) || \
    defined(MCUBOOT_RAM_LOAD)
    (void)state;
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
    BOOT_LOG_DBG("bootutil_img_hash");

#ifdef MCUBOOT_ENC_IMAGES
    if (state == NULL) {
        enc_state = NULL;
        image_index = 0;
    } else {
        enc_state = BOOT_CURR_ENC(state);
        image_index = BOOT_CURR_IMG(state);
    }

    /* Encrypted images only exist in the secondary slot */
    if (MUST_DECRYPT(fap, image_index, hdr) &&
            !boot_enc_valid(enc_state, 1)) {
        BOOT_LOG_DBG("bootutil_img_hash: error encrypted image found in primary slot");
        return -1;
    }
#endif

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    /* For swap using offset mode, the image starts in the second sector of the upgrade slot, so
     * apply the offset when this is needed
     */
    sector_off = boot_get_state_secondary_offset(state, fap);
#endif

    bootutil_sha_init(&sha_ctx);

    /* in some cases (split image) the hash is seeded with data from
     * the loader image */
    if (seed && (seed_len > 0)) {
        bootutil_sha_update(&sha_ctx, seed, seed_len);
    }

    /* Hash is computed over image header and image itself. */
    size = hdr_size = hdr->ih_hdr_size;
    size += hdr->ih_img_size;
    tlv_off = size;

    /* If protected TLVs are present they are also hashed. */
    size += hdr->ih_protect_tlv_size;

#ifdef MCUBOOT_HASH_STORAGE_DIRECTLY
    /* No chunk loading, storage is mapped to address space and can
     * be directly given to hashing function.
     */
    fa_ret = flash_device_base(flash_area_get_device_id(fap), &base);
    if (fa_ret != 0) {
        base = 0;
    }

    bootutil_sha_update(&sha_ctx, (void *)(base + flash_area_get_off(fap)), size);
#else /* MCUBOOT_HASH_STORAGE_DIRECTLY */
#ifdef MCUBOOT_RAM_LOAD
    bootutil_sha_update(&sha_ctx,
                        (void*)(IMAGE_RAM_BASE + hdr->ih_load_addr),
                        size);
#else
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
#endif
#if defined(MCUBOOT_SWAP_USING_OFFSET)
        rc = flash_area_read(fap, off + sector_off, tmp_buf, blk_sz);
#else
        rc = flash_area_read(fap, off, tmp_buf, blk_sz);
#endif
        if (rc) {
            bootutil_sha_drop(&sha_ctx);
            BOOT_LOG_DBG("bootutil_img_validate Error %d reading data chunk %p %u %u",
                         rc, fap, off, blk_sz);
            return rc;
        }
#ifdef MCUBOOT_ENC_IMAGES
        if (MUST_DECRYPT(fap, image_index, hdr)) {
            /* Only payload is encrypted (area between header and TLVs) */
            int slot = flash_area_id_to_multi_image_slot(image_index,
                            flash_area_get_id(fap));

            if (off >= hdr_size && off < tlv_off) {
                blk_off = (off - hdr_size) & 0xf;
                boot_enc_decrypt(enc_state, slot, off - hdr_size,
                                 blk_sz, blk_off, tmp_buf);
            }
        }
#endif
        bootutil_sha_update(&sha_ctx, tmp_buf, blk_sz);
    }
#endif /* MCUBOOT_RAM_LOAD */
#endif /* MCUBOOT_HASH_STORAGE_DIRECTLY */
    bootutil_sha_finish(&sha_ctx, hash_result);
    bootutil_sha_drop(&sha_ctx);

    return 0;
}
#endif

/*
 * Currently, we only support being able to verify one type of
 * signature, because there is a single verification function that we
 * call.  List the type of TLV we are expecting.  If we aren't
 * configured for any signature, don't define this macro.
 */
#if (defined(MCUBOOT_SIGN_RSA)      + \
     defined(MCUBOOT_SIGN_EC256)    + \
     defined(MCUBOOT_SIGN_EC384)    + \
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
#elif defined(MCUBOOT_SIGN_EC256) || \
      defined(MCUBOOT_SIGN_EC384) || \
      defined(MCUBOOT_SIGN_EC)
#    define EXPECTED_SIG_TLV IMAGE_TLV_ECDSA_SIG
#    define SIG_BUF_SIZE 128
#    define EXPECTED_SIG_LEN(x) (1) /* always true, ASN.1 will validate */
#elif defined(MCUBOOT_SIGN_ED25519)
#    define EXPECTED_SIG_TLV IMAGE_TLV_ED25519
#    define SIG_BUF_SIZE 64
#    define EXPECTED_SIG_LEN(x) ((x) == SIG_BUF_SIZE)
#else
#    define SIG_BUF_SIZE 32 /* no signing, sha256 digest only */
#endif

#if (defined(MCUBOOT_HW_KEY)       + \
     defined(MCUBOOT_BUILTIN_KEY)) > 1
#error "Please use either MCUBOOT_HW_KEY or the MCUBOOT_BUILTIN_KEY feature."
#endif

#ifdef EXPECTED_SIG_TLV

#if !defined(MCUBOOT_BUILTIN_KEY)
#if !defined(MCUBOOT_HW_KEY)
/* The key TLV contains the hash of the public key. */
#   define EXPECTED_KEY_TLV     IMAGE_TLV_KEYHASH
#   define KEY_BUF_SIZE         IMAGE_HASH_SIZE
#else
/* The key TLV contains the whole public key.
 * Add a few extra bytes to the key buffer size for encoding and
 * for public exponent.
 */
#   define EXPECTED_KEY_TLV     IMAGE_TLV_PUBKEY
#   define KEY_BUF_SIZE         (SIG_BUF_SIZE + 24)
#endif /* !MCUBOOT_HW_KEY */

#if !defined(MCUBOOT_HW_KEY)
static int
bootutil_find_key(uint8_t image_index, uint8_t *keyhash, uint8_t keyhash_len)
{
    bootutil_sha_context sha_ctx;
    int i;
    const struct bootutil_key *key;
    (void)image_index;

    BOOT_LOG_DBG("bootutil_find_key");

    if (keyhash_len > IMAGE_HASH_SIZE) {
        return -1;
    }

    for (i = 0; i < bootutil_key_cnt; i++) {
        key = &bootutil_keys[i];
        bootutil_sha_init(&sha_ctx);
        bootutil_sha_update(&sha_ctx, key->key, *key->len);
        bootutil_sha_finish(&sha_ctx, hash);
        bootutil_sha_drop(&sha_ctx);
        if (!memcmp(hash, keyhash, keyhash_len)) {
            return i;
        }
    }
    return -1;
}
#else /* !MCUBOOT_HW_KEY */
extern unsigned int pub_key_len;
static int
bootutil_find_key(uint8_t image_index, uint8_t *key, uint16_t key_len)
{
    bootutil_sha_context sha_ctx;
    uint8_t hash[IMAGE_HASH_SIZE];
    uint8_t key_hash[IMAGE_HASH_SIZE];
    size_t key_hash_size = sizeof(key_hash);
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    BOOT_LOG_DBG("bootutil_find_key: image_index %d", image_index);

    bootutil_sha_init(&sha_ctx);
    bootutil_sha_update(&sha_ctx, key, key_len);
    bootutil_sha_finish(&sha_ctx, hash);
    bootutil_sha_drop(&sha_ctx);

    rc = boot_retrieve_public_key_hash(image_index, key_hash, &key_hash_size);
    if (rc) {
        return -1;
    }

    /* Adding hardening to avoid this potential attack:
     *  - Image is signed with an arbitrary key and the corresponding public
     *    key is added as a TLV field.
     * - During public key validation (comparing against key-hash read from
     *   HW) a fault is injected to accept the public key as valid one.
     */
    FIH_CALL(boot_fih_memequal, fih_rc, hash, key_hash, key_hash_size);
    if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
        bootutil_keys[0].key = key;
        pub_key_len = key_len;
        return 0;
    }

    return -1;
}
#endif /* !MCUBOOT_HW_KEY */

#else
/* For MCUBOOT_BUILTIN_KEY, key id is passed */
#define EXPECTED_KEY_TLV     IMAGE_TLV_KEYID
#define KEY_BUF_SIZE         sizeof(int32_t)

static int bootutil_find_key(uint8_t image_index, uint8_t *key_id_buf, uint8_t key_id_buf_len)
{
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* Key id is passed */
    assert(key_id_buf_len == sizeof(int32_t));
    int32_t key_id = (((int32_t)key_id_buf[0] << 24) |
                      ((int32_t)key_id_buf[1] << 16) |
                      ((int32_t)key_id_buf[2] << 8)  |
                      ((int32_t)key_id_buf[3]));

    /* Check if key id is associated with the image */
    FIH_CALL(boot_verify_key_id_for_image, fih_rc, image_index, key_id);
    if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
        return key_id;
    }

    return -1;
}
#endif /* !MCUBOOT_BUILTIN_KEY */
#endif /* EXPECTED_SIG_TLV */

/**
 * Reads the value of an image's security counter.
 *
 * @param state         Pointer to the boot state object.
 * @param slot          Slot of the current image to get the security counter of.
 * @param fap           Pointer to a description structure of the image's
 *                      flash area.
 * @param security_cnt  Pointer to store the security counter value.
 *
 * @return              0 on success; nonzero on failure.
 */
int32_t
bootutil_get_img_security_cnt(struct boot_loader_state *state, int slot,
                              const struct flash_area *fap,
                              uint32_t *img_security_cnt)
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    int32_t rc;

    if ((state == NULL) ||
        (boot_img_hdr(state, slot) == NULL) ||
        (fap == NULL) ||
        (img_security_cnt == NULL)) {
        /* Invalid parameter. */
        return BOOT_EBADARGS;
    }

    /* The security counter TLV is in the protected part of the TLV area. */
    if (boot_img_hdr(state, slot)->ih_protect_tlv_size == 0) {
        return BOOT_EBADIMAGE;
    }

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    it.start_off = boot_get_state_secondary_offset(state, fap);
#endif

    rc = bootutil_tlv_iter_begin(&it, boot_img_hdr(state, slot), fap, IMAGE_TLV_SEC_CNT, true);
    if (rc) {
        return rc;
    }

    /* Traverse through the protected TLV area to find
     * the security counter TLV.
     */

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        /* Security counter TLV has not been found. */
        return -1;
    }

    if (len != sizeof(*img_security_cnt)) {
        /* Security counter is not valid. */
        return BOOT_EBADIMAGE;
    }

    rc = LOAD_IMAGE_DATA(boot_img_hdr(state, slot), fap, off, img_security_cnt, len);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    return 0;
}

#if defined(MCUBOOT_SIGN_PURE)
/* Returns:
 *  0 -- found
 *  1 -- not found or found but not true
 * -1 -- failed for some reason
 *
 * Value of TLV does not matter, presence decides.
 */
static int bootutil_check_for_pure(const struct image_header *hdr,
                                   const struct flash_area *fap)
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    int32_t rc;

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_SIG_PURE, false);
    if (rc) {
        return -1;
    }

    /* Search for the TLV */
    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc == 0 && len == 1) {
        uint8_t val;

        rc = LOAD_IMAGE_DATA(hdr, fap, off, &val, sizeof(val));
        if (rc == 0) {
            return (val == 1) ? 0 : 1;
        } else {
            return -1;
        }
    }

    return 1;
}
#endif

#ifndef ALLOW_ROGUE_TLVS
/*
 * The following list of TLVs are the only entries allowed in the unprotected
 * TLV section.  All other TLV entries must be in the protected section.
 */
static const uint16_t allowed_unprot_tlvs[] = {
     IMAGE_TLV_KEYHASH,
     IMAGE_TLV_PUBKEY,
     IMAGE_TLV_KEYID,
     IMAGE_TLV_SHA256,
     IMAGE_TLV_SHA384,
     IMAGE_TLV_SHA512,
     IMAGE_TLV_RSA2048_PSS,
     IMAGE_TLV_ECDSA224,
     IMAGE_TLV_ECDSA_SIG,
     IMAGE_TLV_RSA3072_PSS,
     IMAGE_TLV_ED25519,
#if defined(MCUBOOT_SIGN_PURE)
     IMAGE_TLV_SIG_PURE,
#endif
     IMAGE_TLV_ENC_RSA2048,
     IMAGE_TLV_ENC_KW,
     IMAGE_TLV_ENC_EC256,
#if !defined(MCUBOOT_HMAC_SHA512)
     IMAGE_TLV_ENC_X25519,
#else
     IMAGE_TLV_ENC_X25519_SHA512,
#endif
     /* Mark end with ANY. */
     IMAGE_TLV_ANY,
};
#endif

/*
 * Verify the integrity of the image.
 * Return non-zero if image could not be validated/does not validate.
 */
fih_ret
bootutil_img_validate(struct boot_loader_state *state,
                      struct image_header *hdr, const struct flash_area *fap,
                      uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *seed,
                      int seed_len, uint8_t *out_hash
                     )
{
#if (defined(EXPECTED_KEY_TLV) && defined(MCUBOOT_HW_KEY)) || defined(MCUBOOT_HW_ROLLBACK_PROT)
    int image_index = (state == NULL ? 0 : BOOT_CURR_IMG(state));
#endif
    uint32_t off;
    uint16_t len;
    uint16_t type;
    uint32_t img_sz;
#ifdef EXPECTED_SIG_TLV
    FIH_DECLARE(valid_signature, FIH_FAILURE);
    int key_id = -1;
#ifdef MCUBOOT_HW_KEY
    uint8_t key_buf[KEY_BUF_SIZE];
#endif
#endif /* EXPECTED_SIG_TLV */
    struct image_tlv_iter it;
    uint8_t buf[SIG_BUF_SIZE];
#if defined(EXPECTED_HASH_TLV) && !defined(MCUBOOT_SIGN_PURE)
    int image_hash_valid = 0;
    uint8_t hash[IMAGE_HASH_SIZE];
#endif
    int rc = 0;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
#ifdef MCUBOOT_HW_ROLLBACK_PROT
    fih_int security_cnt = fih_int_encode(INT_MAX);
    uint32_t img_security_cnt = 0;
    FIH_DECLARE(security_counter_valid, FIH_FAILURE);
#endif

    BOOT_LOG_DBG("bootutil_img_validate: flash area %p", fap);

#if defined(EXPECTED_HASH_TLV) && !defined(MCUBOOT_SIGN_PURE)
    rc = bootutil_img_hash(state, hdr, fap, tmp_buf, tmp_buf_sz, hash, seed, seed_len);
    if (rc) {
        goto out;
    }

    if (out_hash) {
        memcpy(out_hash, hash, IMAGE_HASH_SIZE);
    }
#endif

#if defined(MCUBOOT_SIGN_PURE)
    /* If Pure type signature is expected then it has to be there */
    rc = bootutil_check_for_pure(hdr, fap);
    if (rc != 0) {
        BOOT_LOG_DBG("bootutil_img_validate: pure expected");
        goto out;
    }
#endif

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    it.start_off = boot_get_state_secondary_offset(state, fap);
#endif

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_ANY, false);
    if (rc) {
        BOOT_LOG_DBG("bootutil_img_validate: TLV iteration failed %d", rc);
        goto out;
    }

#ifdef MCUBOOT_SWAP_USING_OFFSET
    img_sz = it.tlv_end - it.start_off;
#else
    img_sz = it.tlv_end;
#endif
    BOOT_LOG_DBG("bootutil_img_validate: TLV off %u, end %u", it.tlv_off, it.tlv_end);

    if (img_sz > bootutil_max_image_size(state, fap)) {
        rc = -1;
        BOOT_LOG_DBG("bootutil_img_validate: TLV beyond image size");
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

#ifndef ALLOW_ROGUE_TLVS
        /*
         * Ensure that the non-protected TLV only has entries necessary to hold
         * the signature.  We also allow encryption related keys to be in the
         * unprotected area.
         */
        if (!bootutil_tlv_iter_is_prot(&it, off)) {
             bool found = false;
             for (const uint16_t *p = allowed_unprot_tlvs; *p != IMAGE_TLV_ANY; p++) {
                  if (type == *p) {
                       found = true;
                       break;
                  }
             }
             if (!found) {
                  BOOT_LOG_DBG("bootutil_img_validate: TLV %d not permitted", type);
                  FIH_SET(fih_rc, FIH_FAILURE);
                  goto out;
             }
        }
#endif
        switch(type) {
#if defined(EXPECTED_HASH_TLV) && !defined(MCUBOOT_SIGN_PURE)
        case EXPECTED_HASH_TLV:
        {
            BOOT_LOG_DBG("bootutil_img_validate: EXPECTED_HASH_TLV == %d", EXPECTED_HASH_TLV);
            /* Verify the image hash. This must always be present. */
            if (len != sizeof(hash)) {
                rc = -1;
                goto out;
            }
            rc = LOAD_IMAGE_DATA(hdr, fap, off, buf, sizeof(hash));
            if (rc) {
                goto out;
            }

            FIH_CALL(boot_fih_memequal, fih_rc, hash, buf, sizeof(hash));
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                FIH_SET(fih_rc, FIH_FAILURE);
                goto out;
            }

            image_hash_valid = 1;
            break;
        }
#endif /* defined(EXPECTED_HASH_TLV) && !defined(MCUBOOT_SIGN_PURE) */
#ifdef EXPECTED_KEY_TLV
        case EXPECTED_KEY_TLV:
        {
            BOOT_LOG_DBG("bootutil_img_validate: EXPECTED_KEY_TLV == %d", EXPECTED_KEY_TLV);
            /*
             * Determine which key we should be checking.
             */
            if (len > KEY_BUF_SIZE) {
                rc = -1;
                goto out;
            }
#ifndef MCUBOOT_HW_KEY
            rc = LOAD_IMAGE_DATA(hdr, fap, off, buf, len);
            if (rc) {
                goto out;
            }
            key_id = bootutil_find_key(image_index, buf, len);
#else
            rc = LOAD_IMAGE_DATA(hdr, fap, off, key_buf, len);
            if (rc) {
                goto out;
            }
            key_id = bootutil_find_key(image_index, key_buf, len);
#endif /* !MCUBOOT_HW_KEY */
            /*
             * The key may not be found, which is acceptable.  There
             * can be multiple signatures, each preceded by a key.
             */
            break;
        }
#endif /* EXPECTED_KEY_TLV */
#ifdef EXPECTED_SIG_TLV
        case EXPECTED_SIG_TLV:
        {
            BOOT_LOG_DBG("bootutil_img_validate: EXPECTED_SIG_TLV == %d", EXPECTED_SIG_TLV);
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
#ifndef MCUBOOT_SIGN_PURE
            FIH_CALL(bootutil_verify_sig, valid_signature, hash, sizeof(hash),
                                                           buf, len, key_id);
#else
            /* Directly check signature on the image, by using the mapping of
             * a device to memory. The pointer is beginning of image in flash,
             * so offset of area, the range is header + image + protected tlvs.
             */
            FIH_CALL(bootutil_verify_img, valid_signature, (void *)flash_area_get_off(fap),
                     hdr->ih_hdr_size + hdr->ih_img_size + hdr->ih_protect_tlv_size,
                     buf, len, key_id);
#endif
            key_id = -1;
            break;
        }
#endif /* EXPECTED_SIG_TLV */
#ifdef MCUBOOT_HW_ROLLBACK_PROT
        case IMAGE_TLV_SEC_CNT:
        {
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
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                FIH_SET(fih_rc, FIH_FAILURE);
                goto out;
            }

            /* Compare the new image's security counter value against the
             * stored security counter value.
             */
            fih_rc = fih_ret_encode_zero_equality(img_security_cnt <
                                   (uint32_t)fih_int_decode(security_cnt));
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                FIH_SET(fih_rc, FIH_FAILURE);
                goto out;
            }

            /* The image's security counter has been successfully verified. */
            security_counter_valid = fih_rc;
            break;
        }
#endif /* MCUBOOT_HW_ROLLBACK_PROT */
        }
    }

#if defined(EXPECTED_HASH_TLV) && !defined(MCUBOOT_SIGN_PURE)
    rc = !image_hash_valid;
    if (rc) {
        goto out;
    }
#elif defined(MCUBOOT_SIGN_PURE)
    /* This returns true on EQ, rc is err on non-0 */
    rc = FIH_NOT_EQ(valid_signature, FIH_SUCCESS);
#endif
#ifdef EXPECTED_SIG_TLV
    FIH_SET(fih_rc, valid_signature);
#endif
#ifdef MCUBOOT_HW_ROLLBACK_PROT
    if (FIH_NOT_EQ(security_counter_valid, FIH_SUCCESS)) {
        rc = -1;
        goto out;
    }
#endif

out:
    if (rc) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

    FIH_RET(fih_rc);
}
