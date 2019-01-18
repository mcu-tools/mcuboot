/*
 * Copyright (c) 2019 Linaro Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>  // TODO: Remove
#include <stdlib.h>  // TODO: Remove

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SUIT

#include <string.h>
#include <inttypes.h>

#include <bootutil/image.h>
#include <bootutil/bootutil_log.h>
#include <bootutil/sha256.h>
#include "bootutil_priv.h"
#include "cbor.h"

MCUBOOT_LOG_MODULE_REGISTER(mcuboot);

#define COSE_Signed_Tagged 98

/* CBOR template for manifest outer wrapper+signature. */
static const uint8_t wrapper_template[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 2),
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
      /* Tagged signature */
      CBOR_ITEM_1(CBOR_MAJOR_TAG, COSE_Signed_Tagged),
        /* array[4] */
        CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 4),
          /* protected header, bstr encoded map */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 3),
            CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 3),
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 0),
          /* unprotected header, empty map */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 0),
          /* payload, null meaning external */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_OTHER, CBOR_OTHER_NULL),
          /* Signatures (TODO: escape here to handle multiple
           * signatures. */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 1),
            /* One signature. array[3] */
            CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 3),
              /* Protected header, << { 1: -37 } >>, indicates RS256 */
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 4),
                CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
                  CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
                  CBOR_ITEM_1(CBOR_MAJOR_NEGATIVE, -1 + 37),
              /* Unprotected header.
               * { 4: bstr }, gives key id. */
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
                CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 4),
                /* CAPTURE 0: key-id */
                CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(0)),
          /* signature itself, CAPTURE 1 */
          CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(1)),

      /* The manifest itself. CAPTURE 2 */
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 2),
      CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(2)),
};
static const struct slice wrapper_template_slice = {
    .base = wrapper_template,
    .len = sizeof(wrapper_template),
};

static const uint8_t sig_block_head[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 5),
};

static const uint8_t body_prot[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 3),
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 3),
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 0),
};

static const uint8_t sig_prot[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 4),
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
    CBOR_ITEM_1(CBOR_MAJOR_NEGATIVE, -1 + 37),
};

static const uint8_t signature_text[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_TEXT, 9),
    'S', 'i', 'g', 'n', 'a', 't', 'u', 'r', 'e',
};

static const uint8_t unprot[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 0),
};

static void
hash_bstr_header(bootutil_sha256_context *ctx, uint32_t len)
{
    if (len < 24) {
        uint8_t header[] = { CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, len) };
        bootutil_sha256_update(ctx, header, sizeof(header));
    } else if (len < 256) {
        uint8_t header[] = { CBOR_ITEM_1(CBOR_MAJOR_BSTR, len) };
        bootutil_sha256_update(ctx, header, sizeof(header));
    } else if (len < 65536) {
        uint8_t header[] = { CBOR_ITEM_2(CBOR_MAJOR_BSTR, len) };
        bootutil_sha256_update(ctx, header, sizeof(header));
    } else {
        BOOT_LOG_ERR("Manifest larger than supported");
        while (1) {
        }
    }
}

static int
validate_manifest_signature(struct slice cose, struct slice *manifest)
{
    int rc;
    struct cbor_capture captures[3];
    bootutil_sha256_context ctx;
    uint8_t manifest_hash[32];

    rc = cbor_template_decode(wrapper_template_slice, cose, captures,
                              sizeof(captures)/sizeof(captures[0]));
    if (rc) {
        return rc;
    }

    /* The three captures are: 0 - the key-id, a BSTR, 1 - the
     * signature, and 2 - the manifest itself.  Make sure they are all
     * bstrs. */
    if (captures[0].major != CBOR_MAJOR_BSTR ||
        captures[1].major != CBOR_MAJOR_BSTR ||
        captures[2].major != CBOR_MAJOR_BSTR)
    {
        BOOT_LOG_ERR("keyid, signature or manifest are not of type bstr");
        return -1;
    }

    bootutil_sha256_init(&ctx);
    bootutil_sha256_update(&ctx, sig_block_head, sizeof(sig_block_head));
    bootutil_sha256_update(&ctx, signature_text, sizeof(signature_text));
    bootutil_sha256_update(&ctx, body_prot, sizeof(body_prot));
    bootutil_sha256_update(&ctx, sig_prot, sizeof(sig_prot));
    bootutil_sha256_update(&ctx, unprot, sizeof(unprot));
    hash_bstr_header(&ctx, captures[2].data.len);
    bootutil_sha256_update(&ctx, captures[2].data.base, captures[2].data.len);
    bootutil_sha256_finish(&ctx, manifest_hash);

    /* Verify the signature itself.
     * TODO: Use the key-id to find the proper signature.
     */
    rc = bootutil_verify_sig(manifest_hash, sizeof(manifest_hash),
                             (void *)captures[1].data.base, captures[1].data.len,
                             0);
    if (rc != 0) {
        return rc;
    }

    manifest->base = captures[2].data.base;
    manifest->len = captures[2].data.len;

    return 0;
}

/* CBOR template for the manifest itself. */
static const uint8_t manifest_template[] = {
    /* Manifest is a map. */
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 3),
      /* 1: 1, manifest version. */
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
      /* 2: n, sequence number. */
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 2),
      CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(0)),
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 3),
      /* 3: payload */
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 1),
        CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 3),
          /* 1: [ bstr ], component designator. */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 1),
            CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(1)),
          /* 2: uint, payload length */
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 2),
          CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(2)),
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 3),
          CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 4),
            /* bstr: protected header. */
            CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 4),
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
              CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
              CBOR_ITEM_1(CBOR_MAJOR_UNSIGNED, 41),
            /* {}: unprotected header. */
            CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 0),
            /* null: payload */
            CBOR_ITEM_SIMPLE(CBOR_MAJOR_OTHER, CBOR_OTHER_NULL),
            /* bstr: The hash itself. */
            CBOR_ITEM_1(CBOR_MAJOR_OTHER, CBOR_OTHER_CAPTURE(3)),
};
static const struct slice manifest_template_slice = {
    .base = manifest_template,
    .len = sizeof(manifest_template),
};

static const uint8_t digest_header[] = {
    CBOR_ITEM_SIMPLE(CBOR_MAJOR_ARRAY, 3),
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_TEXT, 6),
      'D', 'i', 'g', 'e', 's', 't',
      CBOR_ITEM_SIMPLE(CBOR_MAJOR_BSTR, 4),
        /* {1: 41}, SHA256 digest. */
        CBOR_ITEM_SIMPLE(CBOR_MAJOR_MAP, 1),
        CBOR_ITEM_SIMPLE(CBOR_MAJOR_UNSIGNED, 1),
        CBOR_ITEM_1(CBOR_MAJOR_UNSIGNED, 41),
      /* The payload goes here. */
};

/*
 * Verify the manifest itself.  Make sure it is well formed, and then
 * ensure that the COSE hash it describes matches the image itself.
 */
int
verify_manifest(struct image_header *hdr, const struct flash_area *fap,
                struct slice manifest, uint8_t *out_hash)
{
    int rc;
    struct cbor_capture captures[4];
    bootutil_sha256_context ctx;
    uint8_t image_hash[32];
    static uint8_t hash_buf[256];
    unsigned this_size;

    /* Captures:
     * 0: uint: sequence number,
     * 1: bstr: component designator,
     * 2: uint: payload length,
     * 3: bstr: payload hash,
     */

    const uint32_t payload_len = hdr->ih_hdr_size + hdr->ih_img_size;

    rc = cbor_template_decode(manifest_template_slice, manifest, captures,
                              sizeof(captures)/sizeof(captures[0]));
    if (rc) {
        return rc;
    }

    /* Ensure the image header sequence number matches the manifest. */
    if (captures[0].major != CBOR_MAJOR_UNSIGNED ||
        hdr->ih_ver.iv_build_num != captures[0].minor)
    {
        BOOT_LOG_ERR("Invalid sequence number/iv_build_num against header");
        return -1;
    }

    /* TODO: We don't know what to do with the component designator.
     * For now, it just needs to be a bstr. */
    if (captures[1].major != CBOR_MAJOR_BSTR) {
        BOOT_LOG_ERR("Invalid component designator");
        return -1;
    }

    /* Ensure that the payload length matches what is in the header.
     * TODO: Ensure that overflow is not an issue here. */
    if (captures[2].major != CBOR_MAJOR_UNSIGNED ||
        captures[2].minor != payload_len)
    {
        BOOT_LOG_ERR("Invalid payload size in manifest");
        return -1;
    }

    /* Compute the digest of the image to compare. */
    bootutil_sha256_init(&ctx);
    bootutil_sha256_update(&ctx, digest_header, sizeof(digest_header));
    hash_bstr_header(&ctx, payload_len);

    /* Add in the hash through the image. */
    for (unsigned off = 0; off < payload_len; off += sizeof(hash_buf)) {
        this_size = payload_len - off;
        if (this_size > sizeof(hash_buf)) {
            this_size = sizeof(hash_buf);
        }
        rc = flash_area_read(fap, off, hash_buf, this_size);
        if (rc) {
            return rc;
        }
        bootutil_sha256_update(&ctx, hash_buf, this_size);
    }
    bootutil_sha256_finish(&ctx, image_hash);

    /* Make sure the hash matches. */
    if (captures[3].major != CBOR_MAJOR_BSTR ||
        captures[3].minor != 32 ||
        memcmp(captures[3].data.base, image_hash, 32) != 0)
    {
        BOOT_LOG_ERR("Image hash mismatch");
        return -1;
    }

    if (out_hash) {
        memcpy(out_hash, image_hash, 32);
    }

    return 0;
}

/*
 * Verify the integrity of the image.
 * Return non-zero if the image could not be validate or does not
 * validate.
 *
 * SUIT manifest version.
 */
int
bootutil_img_validate(struct image_header *hdr, const struct flash_area *fap,
                      uint8_t *tmp_buf, uint32_t tmp_buf_sz,
                      uint8_t *seed, int seed_len, uint8_t *out_hash)
{
    (void)seed;
    (void)seed_len;
    (void)out_hash;

    int rc;
    uint32_t len;
    struct slice cose;
    struct slice manifest;

    /* The SUIT image uses the same header as the TLVs, but with a
     * different magic. */
    uint32_t off = hdr->ih_img_size + hdr->ih_hdr_size;

    /* Load the header. */
    struct image_tlv_info info;
    rc = flash_area_read(fap, off, &info, sizeof(info));
    if (rc) {
        return rc;
    }
    if (info.it_magic != IMAGE_SUIT_INFO_MAGIC) {
        return -1;
    }
    len = info.it_tlv_tot - sizeof(info);
    off += sizeof(info);

    /* Make sure we have room in our tmp_buf to hold the manifest.
     * This will catch invalid or bogus lengths. */
    if (len > tmp_buf_sz) {
        BOOT_LOG_ERR("SUIT manifest larger than buffer: %d vs %d",
                     len, tmp_buf_sz);
        return -1;
    }

    /* Load the entire manifest from flash. */
    rc = flash_area_read(fap, off, tmp_buf, len);
    if (rc) {
        return rc;
    }

    cose.base = tmp_buf;
    cose.len = len;

    rc = validate_manifest_signature(cose, &manifest);
    if (rc) {
        return rc;
    }

    rc = verify_manifest(hdr, fap, manifest, out_hash);
    if (rc) {
        return rc;
    }

    return 0;
}

#endif
