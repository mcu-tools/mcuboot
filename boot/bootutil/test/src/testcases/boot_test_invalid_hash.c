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

TEST_CASE(boot_test_invalid_hash)
{
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 12 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 2, 3, 4 },
    };
    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = 0,
        .ih_ver = { 1, 2, 3, 432 },
    };

    struct image_tlv tlv = {
        .it_type = IMAGE_TLV_SHA256,
        .it_len = 32
    };
    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    rc = hal_flash_write(boot_test_img_addrs[1].flash_id,
      boot_test_img_addrs[1].address + hdr1.ih_hdr_size + hdr1.ih_img_size,
      &tlv, sizeof(tlv));
    TEST_ASSERT(rc == 0);

    rc = boot_set_pending(0);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(BOOT_SWAP_TYPE_NONE, &hdr0, NULL);
}
