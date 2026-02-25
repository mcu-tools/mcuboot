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

#include <limits.h>

#include <flash_map_backend/flash_map_backend.h>

#include "bootutil/image.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/sign_key.h"
#include "bootutil/security_cnt.h"
#include "bootutil/fault_injection_hardening.h"

#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);
#if defined(MCUBOOT_UUID_VID) || defined(MCUBOOT_UUID_CID)
#include "bootutil/mcuboot_uuid.h"
#endif /* MCUBOOT_UUID_VID || MCUBOOT_UUID_CID */

#ifdef MCUBOOT_ENC_IMAGES
#include "bootutil/enc_key.h"
#endif
#include "bootutil_priv.h"

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
#endif /* !MCUBOOT_BUILTIN_KEY */
#endif /* EXPECTED_SIG_TLV */

#if defined(MCUBOOT_SIGN_PURE)
/* Returns:
 *  0 -- found
 *  1 -- not found or found but not true
 * -1 -- failed for some reason
 *
 * Value of TLV does not matter, presence decides.
 */
#if defined(MCUBOOT_SWAP_USING_OFFSET)
static int bootutil_check_for_pure(const struct image_header *hdr, const struct flash_area *fap,
                                   uint32_t start_off)
#else
static int bootutil_check_for_pure(const struct image_header *hdr, const struct flash_area *fap)
#endif
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    int32_t rc;

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    it.start_off = start_off;
#endif

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

#ifdef MCUBOOT_USE_TLV_ALLOW_LIST
/*
 * The following list of TLVs are the only entries allowed in the unprotected
 * TLV section.  All other TLV entries must be in the protected section.
 */
static const uint16_t allowed_unprot_tlvs[] = {
     IMAGE_TLV_KEYHASH,
     IMAGE_TLV_PUBKEY,
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
#if (defined(EXPECTED_KEY_TLV) && defined(MCUBOOT_HW_KEY)) || \
    (defined(EXPECTED_SIG_TLV) && defined(MCUBOOT_BUILTIN_KEY)) || \
    defined(MCUBOOT_HW_ROLLBACK_PROT) || \
    defined(MCUBOOT_UUID_VID) || defined(MCUBOOT_UUID_CID)
    int image_index = (state == NULL ? 0 : BOOT_CURR_IMG(state));
#endif
    uint32_t off;
    uint16_t len;
    uint16_t type;
    uint32_t img_sz;
#ifdef EXPECTED_SIG_TLV
    FIH_DECLARE(valid_signature, FIH_FAILURE);
#ifndef MCUBOOT_BUILTIN_KEY
    int key_id = -1;
#else
    /* Pass a key ID equal to the image index, the underlying crypto library
     * is responsible for mapping the image index to a builtin key ID.
     */
    int key_id = image_index;
#endif /* !MCUBOOT_BUILTIN_KEY */
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
#if defined(MCUBOOT_SIGN_PURE)
    uintptr_t base = 0;
#endif
#ifdef MCUBOOT_HW_ROLLBACK_PROT
    fih_int security_cnt = fih_int_encode(INT_MAX);
    uint32_t img_security_cnt = 0;
    FIH_DECLARE(security_counter_valid, FIH_FAILURE);
#endif
#ifdef MCUBOOT_UUID_VID
    struct image_uuid img_uuid_vid = {0x00};
    FIH_DECLARE(uuid_vid_valid, FIH_FAILURE);
#endif
#ifdef MCUBOOT_UUID_CID
    struct image_uuid img_uuid_cid = {0x00};
    FIH_DECLARE(uuid_cid_valid, FIH_FAILURE);
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

#if defined(MCUBOOT_SWAP_USING_OFFSET)
    it.start_off = boot_get_state_secondary_offset(state, fap);
#endif

#if defined(MCUBOOT_SIGN_PURE)
    /* If Pure type signature is expected then it has to be there */
#if defined(MCUBOOT_SWAP_USING_OFFSET)
    rc = bootutil_check_for_pure(hdr, fap, it.start_off);
#else
    rc = bootutil_check_for_pure(hdr, fap);
#endif
    if (rc != 0) {
        BOOT_LOG_DBG("bootutil_img_validate: pure expected");
        goto out;
    }
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
    BOOT_LOG_DBG("bootutil_img_validate: TLV off %" PRIu32 ", end %" PRIu32,
                 it.tlv_off, it.tlv_end);

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

#ifdef MCUBOOT_USE_TLV_ALLOW_LIST
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
            key_id = bootutil_find_key(buf, len);
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
            rc = flash_device_base(flash_area_get_device_id(fap), &base);
            if (rc != 0) {
                goto out;
            }

#if defined(MCUBOOT_SWAP_USING_OFFSET)
            base += boot_get_state_secondary_offset(state, fap);
#endif

            /* Directly check signature on the image, by using the mapping of
             * a device to memory. The pointer is beginning of image in flash,
             * so offset of area, the range is header + image + protected tlvs.
             */
            FIH_CALL(bootutil_verify_sig, valid_signature, (void *)(base + flash_area_get_off(fap)),
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
                BOOT_LOG_ERR("Image security counter value %u lower than monotonic value %u",
                             img_security_cnt, (uint32_t)fih_int_decode(security_cnt));
                FIH_SET(fih_rc, FIH_FAILURE);
                goto out;
            }

#ifdef MCUBOOT_HW_ROLLBACK_PROT_COUNTER_LIMITED
            if (img_security_cnt > (uint32_t)fih_int_decode(security_cnt)) {
                FIH_CALL(boot_nv_security_counter_is_update_possible, fih_rc, image_index,
                         img_security_cnt);
                if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                    FIH_SET(fih_rc, FIH_FAILURE);
                    BOOT_LOG_ERR("Security counter update is not possible, possibly the maximum "
                                 "number of security updates has been reached.");
                    goto out;
                }
            }
#endif

            /* The image's security counter has been successfully verified. */
            security_counter_valid = fih_rc;
            break;
        }
#endif /* MCUBOOT_HW_ROLLBACK_PROT */
#ifdef MCUBOOT_UUID_VID
        case IMAGE_TLV_UUID_VID:
        {
            /*
             * Verify the image's vendor ID length.
             * This must always be present.
             */
            if (len != sizeof(img_uuid_vid)) {
                /* Vendor UUID is not valid. */
                rc = -1;
                goto out;
            }

            rc = LOAD_IMAGE_DATA(hdr, fap, off, img_uuid_vid.raw, len);
            if (rc) {
                goto out;
            }

            FIH_CALL(boot_uuid_vid_match, fih_rc, image_index, &img_uuid_vid);
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                FIH_SET(uuid_vid_valid, FIH_FAILURE);
                goto out;
            }

            /* The image's vendor identifier has been successfully verified. */
            uuid_vid_valid = fih_rc;
            break;
        }
#endif
#ifdef MCUBOOT_UUID_CID
        case IMAGE_TLV_UUID_CID:
        {
            /*
             * Verify the image's class ID length.
             * This must always be present.
             */
            if (len != sizeof(img_uuid_cid)) {
                /* Image class UUID is not valid. */
                rc = -1;
                goto out;
            }

            rc = LOAD_IMAGE_DATA(hdr, fap, off, img_uuid_cid.raw, len);
            if (rc) {
                goto out;
            }

            FIH_CALL(boot_uuid_cid_match, fih_rc, image_index, &img_uuid_cid);
            if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
                FIH_SET(uuid_cid_valid, FIH_FAILURE);
                goto out;
            }

            /* The image's class identifier has been successfully verified. */
            uuid_cid_valid = fih_rc;
            break;
        }
#endif
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

#ifdef MCUBOOT_UUID_VID
    if (FIH_NOT_EQ(uuid_vid_valid, FIH_SUCCESS)) {
        rc = -1;
        goto out;
    }
#endif
#ifdef MCUBOOT_UUID_CID
    if (FIH_NOT_EQ(uuid_cid_valid, FIH_SUCCESS)) {
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
