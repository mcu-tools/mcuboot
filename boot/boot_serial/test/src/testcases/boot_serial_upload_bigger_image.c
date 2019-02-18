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

#include <tinycbor/cborconstants_p.h>

#include "boot_test.h"

TEST_CASE(boot_serial_upload_bigger_image)
{
    char img[256];
    char enc_img[64];
    char buf[sizeof(struct nmgr_hdr) + 128];
    int len;
    int off;
    int rc;
    struct nmgr_hdr *hdr;
    const struct flash_area *fap;
    int i;

    const int payload_off = sizeof *hdr;
    const int img_data_off = payload_off + 8;

    /* 00000000  a3 64 64 61 74 61 58 20  |.ddataX.|
     * 00000008  00 00 00 00 00 00 00 00  |........|
     * 00000010  00 00 00 00 00 00 00 00  |........|
     * 00000018  00 00 00 00 00 00 00 00  |........|
     * 00000020  00 00 00 00 00 00 00 00  |........|
     * 00000028  63 6c 65 6e 1a 00 01 14  |clen....|
     * 00000030  e8 63 6f 66 66 00        |.coff.|
     */
    static const uint8_t payload_first[] = {
        0xa3, 0x64, 0x64, 0x61, 0x74, 0x61, 0x58, 0x20,
        /* 32 bytes of image data starts here. */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x63, 0x6c, 0x65, 0x6e, 0x1a, 0x00, 0x01, 0x14,
        0xe8, 0x63, 0x6f, 0x66, 0x66, 0x00,
    };

    /* 00000000  a3 64 64 61 74 61 58 20  |.ddataX.|
     * 00000008  00 00 00 00 00 00 00 00  |........|
     * 00000010  00 00 00 00 00 00 00 00  |........|
     * 00000018  00 00 00 00 00 00 00 00  |........|
     * 00000020  00 00 00 00 00 00 00 00  |........|
     * 00000028  63 6f 66 66 00 00        |coff..|
     */
    static const uint8_t payload_next[] = {
        0xa2, 0x64, 0x64, 0x61, 0x74, 0x61, 0x58, 0x20,
        /* 32 bytes of image data starts here. */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x63, 0x6f, 0x66, 0x66,
        /* 2 bytes of offset value starts here. */
        0x00, 0x00
    };

    for (i = 0; i < sizeof(img); i++) {
        img[i] = i;
    }

    for (off = 0; off < sizeof(img); off += 32) {
        hdr = (struct nmgr_hdr *)buf;
        memset(hdr, 0, sizeof(*hdr));
        hdr->nh_op = NMGR_OP_WRITE;
        hdr->nh_group = htons(MGMT_GROUP_ID_IMAGE);
        hdr->nh_id = IMGMGR_NMGR_ID_UPLOAD;

        if (off) {
            memcpy(buf + payload_off, payload_next, sizeof payload_next);
            len = sizeof payload_next;
            buf[payload_off + len - 2] = Value8Bit;
            buf[payload_off + len - 1] = off;
        } else {
            memcpy(buf + payload_off, payload_first, sizeof payload_first);
            len = sizeof payload_first;
        }
        memcpy(buf + img_data_off, img + off, 32);
        hdr->nh_len = htons(len);

        len = sizeof(*hdr) + len;

        tx_msg(buf, len);
    }

    /*
     * Validate contents inside the primary slot
     */
    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY, &fap);
    assert(rc == 0);

    for (off = 0; off < sizeof(img); off += sizeof(enc_img)) {
        rc = flash_area_read(fap, off, enc_img, sizeof(enc_img));
        assert(rc == 0);
        assert(!memcmp(enc_img, &img[off], sizeof(enc_img)));
    }
}
