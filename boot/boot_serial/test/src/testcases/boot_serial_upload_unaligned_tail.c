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

#include <flash_map_backend/flash_map_backend.h>

#include "boot_test.h"
#include "zcbor_encode.h"
#include "zcbor_bulk.h"

/*
 * Send one upload request carrying len bytes of img from offset off, and
 * return the rc and the next offset from the response.
 */
static void
upload_chunk(const uint8_t *img, uint32_t img_len, uint32_t off, uint32_t len,
             int32_t *rsp_rc, uint32_t *rsp_off)
{
    char buf[sizeof(struct nmgr_hdr) + 128];
    uint8_t rsp[sizeof(struct nmgr_hdr) + 32];
    struct nmgr_hdr *hdr;
    zcbor_state_t zse[5];
    zcbor_state_t zsd[5];
    size_t decoded;
    int payload_len;
    int rsp_len;
    bool ok;

    hdr = (struct nmgr_hdr *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->nh_op = NMGR_OP_WRITE;
    hdr->nh_group = htons(MGMT_GROUP_ID_IMAGE);
    hdr->nh_id = IMGMGR_NMGR_ID_UPLOAD;

    /* The image length is only sent with the first chunk */
    zcbor_new_encode_state(zse, ZCBOR_ARRAY_SIZE(zse), (uint8_t *)(hdr + 1),
                           sizeof(buf) - sizeof(*hdr), 0);
    ok = zcbor_map_start_encode(zse, 3) &&
         zcbor_tstr_put_lit(zse, "data") &&
         zcbor_bstr_encode_ptr(zse, (const char *)&img[off], len) &&
         (off != 0 ||
          (zcbor_tstr_put_lit(zse, "len") && zcbor_uint32_put(zse, img_len))) &&
         zcbor_tstr_put_lit(zse, "off") &&
         zcbor_uint32_put(zse, off) &&
         zcbor_map_end_encode(zse, 3);
    assert(ok);

    payload_len = zse->payload_mut - (uint8_t *)(hdr + 1);
    hdr->nh_len = htons(payload_len);

    tx_msg(buf, sizeof(*hdr) + payload_len);

    rsp_len = rx_msg(rsp, sizeof(rsp));
    assert(rsp_len > (int)sizeof(struct nmgr_hdr));

    struct zcbor_map_decode_key_val rsp_decode[] = {
        ZCBOR_MAP_DECODE_KEY_DECODER("rc", zcbor_int32_decode, rsp_rc),
        ZCBOR_MAP_DECODE_KEY_DECODER("off", zcbor_uint32_decode, rsp_off),
    };

    *rsp_rc = INT32_MIN;
    *rsp_off = UINT32_MAX;
    zcbor_new_decode_state(zsd, ZCBOR_ARRAY_SIZE(zsd),
                           &rsp[sizeof(struct nmgr_hdr)],
                           rsp_len - sizeof(struct nmgr_hdr), 1, NULL, 0);
    ok = zcbor_map_decode_bulk(zsd, rsp_decode, ZCBOR_ARRAY_SIZE(rsp_decode),
                               &decoded) == 0;
    assert(ok);
}

/*
 * Upload images whose length is not a multiple of the flash write alignment,
 * so that the last chunk ends with a partial write unit that has to be padded
 * (syscfg.yml raises the native flash write alignment for this).
 */
TEST_CASE(boot_serial_upload_unaligned_tail)
{
    uint8_t img[512];
    uint8_t rbuf[sizeof(img)];
    const struct flash_area *fap;
    uint32_t align;
    uint32_t chunk;
    uint32_t img_len;
    uint32_t pad_end;
    uint32_t off;
    uint32_t len;
    uint32_t rsp_off;
    int32_t rsp_rc;
    uint8_t erased_val;
    size_t t;
    uint32_t i;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &fap);
    assert(rc == 0);

    align = flash_area_align(fap);
    erased_val = flash_area_erased_val(fap);
    assert(align > 1 && 6 * align <= sizeof(img));

    /* Every chunk but the last is a whole number of write units */
    chunk = 2 * align;

    const uint32_t img_lens[] = {
        1,                      /* Single chunk, shorter than a write unit */
        align - 1,              /* Single chunk, one byte short of a unit */
        chunk + align + 1,      /* Last chunk has whole units and a tail */
        2 * chunk + 1,          /* Last chunk is a 1 byte tail */
        2 * chunk + align - 1,  /* Last chunk is an align - 1 byte tail */
    };

    for (t = 0; t < ZCBOR_ARRAY_SIZE(img_lens); t++) {
        img_len = img_lens[t];

        /* Image bytes are never 0x00 nor 0xff, so differ from erased flash */
        for (i = 0; i < img_len; i++) {
            img[i] = (i + t) % 251 + 1;
        }

        for (off = 0; off < img_len; off += len) {
            len = img_len - off;
            if (len > chunk) {
                len = chunk;
            }

            upload_chunk(img, img_len, off, len, &rsp_rc, &rsp_off);
            assert(rsp_rc == 0);
            assert(rsp_off == off + len);
        }

        /*
         * The whole image, including the tail, must be in the primary slot,
         * and the padding of the last write unit, as well as the following
         * unit, must be left erased.
         */
        pad_end = (img_len + align - 1) / align * align;
        rc = flash_area_read(fap, 0, rbuf, pad_end + align);
        assert(rc == 0);
        assert(!memcmp(rbuf, img, img_len));
        for (i = img_len; i < pad_end + align; i++) {
            assert(rbuf[i] == erased_val);
        }
    }

    flash_area_close(fap);
}
