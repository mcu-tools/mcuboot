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

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "bootutil/image.h"
#include "bootutil/sha256.h"
#include "bootutil/sign_key.h"

#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA)
#include "mbedtls/rsa.h"
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC) || MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
#include "mbedtls/ecdsa.h"
#endif
#include "mbedtls/asn1.h"

#include "bootutil_priv.h"

/*
 * Compute SHA256 over the image.
 */
static int
bootutil_img_hash(struct image_header *hdr, const struct flash_area *fap,
                  uint8_t *tmp_buf, uint32_t tmp_buf_sz,
                  uint8_t *hash_result, uint8_t *seed, int seed_len)
{
    bootutil_sha256_context sha256_ctx;
    uint32_t blk_sz;
    uint32_t size;
    uint32_t off;
    int rc;

    bootutil_sha256_init(&sha256_ctx);

    /* in some cases (split image) the hash is seeded with data from
     * the loader image */
    if(seed && (seed_len > 0)) {
        bootutil_sha256_update(&sha256_ctx, seed, seed_len);
    }

    size = hdr->ih_img_size + hdr->ih_hdr_size;

    /*
     * Hash is computed over image header and image itself. No TLV is
     * included ATM.
     */
    size = hdr->ih_img_size + hdr->ih_hdr_size;
    for (off = 0; off < size; off += blk_sz) {
        blk_sz = size - off;
        if (blk_sz > tmp_buf_sz) {
            blk_sz = tmp_buf_sz;
        }
        rc = flash_area_read(fap, off, tmp_buf, blk_sz);
        if (rc) {
            return rc;
        }
        bootutil_sha256_update(&sha256_ctx, tmp_buf, blk_sz);
    }
    bootutil_sha256_finish(&sha256_ctx, hash_result);

    return 0;
}

/*
 * Verify the integrity of the image.
 * Return non-zero if image could not be validated/does not validate.
 */
int
bootutil_img_validate(struct image_header *hdr, const struct flash_area *fap,
                      uint8_t *tmp_buf, uint32_t tmp_buf_sz,
                      uint8_t *seed, int seed_len, uint8_t *out_hash)
{
    uint32_t off;
    uint32_t size;
    uint32_t sha_off = 0;
#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA) || MYNEWT_VAL(BOOTUTIL_SIGN_EC) || \
    MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
    uint32_t sig_off = 0;
    uint32_t sig_len = 0;
#endif
    struct image_tlv tlv;
    uint8_t buf[256];
    uint8_t hash[32];
    int rc;

#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA)
    if ((hdr->ih_flags & IMAGE_F_PKCS15_RSA2048_SHA256) == 0) {
        return -1;
    }
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC)
    if ((hdr->ih_flags & IMAGE_F_ECDSA224_SHA256) == 0) {
        return -1;
    }
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
    if ((hdr->ih_flags & IMAGE_F_ECDSA256_SHA256) == 0) {
        return -1;
    }
#endif
    if ((hdr->ih_flags & IMAGE_F_SHA256) == 0) {
        return -1;
    }

    rc = bootutil_img_hash(hdr, fap, tmp_buf, tmp_buf_sz, hash,
                           seed, seed_len);
    if (rc) {
        return rc;
    }

    if (out_hash) {
        memcpy(out_hash, hash, 32);
    }

    /* After image there are TLVs. */
    off = hdr->ih_img_size + hdr->ih_hdr_size;
    size = off + hdr->ih_tlv_size;

    for (; off < size; off += sizeof(tlv) + tlv.it_len) {
        rc = flash_area_read(fap, off, &tlv, sizeof tlv);
        if (rc) {
            return rc;
        }
        if (tlv.it_type == IMAGE_TLV_SHA256) {
            if (tlv.it_len != sizeof(hash)) {
                return -1;
            }
            sha_off = off + sizeof(tlv);
        }
#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA)
        if (tlv.it_type == IMAGE_TLV_RSA2048) {
            if (tlv.it_len != 256) { /* 2048 bits */
                return -1;
            }
            sig_off = off + sizeof(tlv);
            sig_len = tlv.it_len;
        }
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC)
        if (tlv.it_type == IMAGE_TLV_ECDSA224) {
            if (tlv.it_len < 64) { /* oids + 2 * 28 bytes */
                return -1;
            }
            sig_off = off + sizeof(tlv);
            sig_len = tlv.it_len;
        }
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
        if (tlv.it_type == IMAGE_TLV_ECDSA256) {
            if (tlv.it_len < 72) { /* oids + 2 * 32 bytes */
                return -1;
            }
            sig_off = off + sizeof(tlv);
            sig_len = tlv.it_len;
        }
#endif
    }
    if (hdr->ih_flags & IMAGE_F_SHA256) {
        if (!sha_off) {
            /*
             * Header said there should be hash TLV, no TLV found.
             */
            return -1;
        }
        rc = flash_area_read(fap, sha_off, buf, sizeof hash);
        if (rc) {
            return rc;
        }
        if (memcmp(hash, buf, sizeof(hash))) {
            return -1;
        }
    }
#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA) || MYNEWT_VAL(BOOTUTIL_SIGN_EC) || \
    MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
    if (!sig_off) {
        /*
         * Header said there should be PKCS1.v5 signature, no TLV
         * found.
         */
        return -1;
    }
    rc = flash_area_read(fap, sig_off, buf, sig_len);
    if (rc) {
        return -1;
    }

    if (hdr->ih_key_id >= bootutil_key_cnt) {
        return -1;
    }
    rc = bootutil_verify_sig(hash, sizeof(hash), buf, sig_len, hdr->ih_key_id);
    if (rc) {
        return -1;
    }
#endif
    return 0;
}
