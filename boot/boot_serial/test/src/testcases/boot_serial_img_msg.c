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

#include "boot_test.h"

TEST_CASE(boot_serial_img_msg)
{
    char img[16];
    char enc_img[BASE64_ENCODE_SIZE(sizeof(img)) + 1];
    char buf[sizeof(struct nmgr_hdr) + sizeof(enc_img) + 32];
    int len;
    int rc;
    struct nmgr_hdr *hdr;
    const struct flash_area *fap;

    /* 00000000  a3 64 64 61 74 61 58 10  |.ddataX.|
     * 00000008  a5 a5 a5 a5 a5 a5 a5 a5  |........|
     * 00000010  a5 a5 a5 a5 a5 a5 a5 a5  |........|
     * 00000018  63 6c 65 6e 1a 00 01 14  |clen....|
     * 00000020  e8 63 6f 66 66 00        |.coff.|
     */
    static const uint8_t payload[] = { 
        0xa3, 0x64, 0x64, 0x61, 0x74, 0x61, 0x58, 0x10,
        /* 16 bytes of image data starts here. */
        0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
        0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
        0x63, 0x6c, 0x65, 0x6e, 0x1a, 0x00, 0x01, 0x14,
        0xe8, 0x63, 0x6f, 0x66, 0x66, 0x00,
    };

    memset(img, 0xa5, sizeof(img));

    hdr = (struct nmgr_hdr *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->nh_op = NMGR_OP_WRITE;
    hdr->nh_group = htons(MGMT_GROUP_ID_IMAGE);
    hdr->nh_id = IMGMGR_NMGR_ID_UPLOAD;

    memcpy(hdr + 1, payload, sizeof payload);
    hdr->nh_len = htons(sizeof payload);

    len = sizeof(*hdr) + sizeof payload;
    tx_msg(buf, len);

    /*
     * Validate contents inside the primary slot
     */
    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &fap);
    assert(rc == 0);

    rc = flash_area_read(fap, 0, enc_img, sizeof(img));
    assert(rc == 0);
    assert(!memcmp(enc_img, img, sizeof(img)));
}
