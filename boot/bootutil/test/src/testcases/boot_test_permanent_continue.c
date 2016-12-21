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

TEST_CASE(boot_test_permanent_continue)
{
    struct boot_status status;
    int rc;

    struct image_header hdr0 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 5 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 0, 5, 21, 432 },
    };

    struct image_header hdr1 = {
        .ih_magic = IMAGE_MAGIC,
        .ih_tlv_size = 4 + 32,
        .ih_hdr_size = BOOT_TEST_HEADER_SIZE,
        .ih_img_size = 32 * 1024,
        .ih_flags = IMAGE_F_SHA256,
        .ih_ver = { 1, 2, 3, 432 },
    };

    boot_test_util_init_flash();
    boot_test_util_write_image(&hdr0, 0);
    boot_test_util_write_hash(&hdr0, 0);
    boot_test_util_write_image(&hdr1, 1);
    boot_test_util_write_hash(&hdr1, 1);

    /* Indicate that the image in slot 0 is being permanently used. */
    boot_test_util_mark_swap_perm();

    boot_test_util_swap_areas(2, 5);

    status.idx = 1;
    status.state = 0;

    rc = boot_write_status(&status);
    TEST_ASSERT_FATAL(rc == 0);

    /* A permanent swap exhibits the same behavior as a revert. */
    boot_test_util_verify_all(BOOT_SWAP_TYPE_REVERT, &hdr0, &hdr1);
}
