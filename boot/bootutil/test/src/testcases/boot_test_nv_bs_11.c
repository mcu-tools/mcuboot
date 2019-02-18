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

TEST_CASE(boot_test_nv_bs_11)
{
    struct boot_status status;
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
        .ih_img_size = 17 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 1, 5, 5 },
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, BOOT_PRIMARY_SLOT);
    boot_test_util_write_hash(&hdr0, BOOT_PRIMARY_SLOT);
    boot_test_util_write_image(&hdr1, BOOT_SECONDARY_SLOT);
    boot_test_util_write_hash(&hdr1, BOOT_SECONDARY_SLOT);
    rc = boot_set_pending(0);
    boot_test_util_copy_area(5, BOOT_TEST_AREA_IDX_SCRATCH);

    status.idx = 0;
    status.state = 1;

    rc = boot_write_status(&status);
    TEST_ASSERT(rc == 0);

    boot_test_util_verify_all(BOOT_SWAP_TYPE_TEST, &hdr0, &hdr1);
}
