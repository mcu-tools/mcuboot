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
#ifndef _BOOT_TEST_H
#define _BOOT_TEST_H

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "testutil/testutil.h"
#include "hal/hal_flash.h"
#include "flash_map_backend/flash_map_backend.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "testutil/testutil.h"

#include "mbedtls/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_TEST_HEADER_SIZE       0x200

/** Internal flash layout. */
extern struct flash_area boot_test_area_descs[];

/** Areas representing the beginning of image slots. */
extern uint8_t boot_test_slot_areas[];

/** Flash offsets of the two image slots. */
struct boot_test_img_addrs {
    uint8_t flash_id;
    uint32_t address;
};
extern struct boot_test_img_addrs boot_test_img_addrs[];

#define BOOT_TEST_AREA_IDX_SCRATCH 6

uint8_t boot_test_util_byte_at(int img_msb, uint32_t image_offset);
uint8_t boot_test_util_flash_align(void);
void boot_test_util_init_flash(void);
void boot_test_util_copy_area(int from_area_idx, int to_area_idx);
void boot_test_util_swap_areas(int area_idx1, int area_idx2);
void boot_test_util_write_image(const struct image_header *hdr,
                                       int slot);
void boot_test_util_write_hash(const struct image_header *hdr, int slot);
void boot_test_util_mark_revert(void);
void boot_test_util_mark_swap_perm(void);
void boot_test_util_verify_area(const struct flash_area *area_desc,
                                       const struct image_header *hdr,
                                       uint32_t image_addr, int img_msb);
void boot_test_util_verify_status_clear(void);
void boot_test_util_verify_flash(const struct image_header *hdr0,
                                        int orig_slot_0,
                                        const struct image_header *hdr1,
                                        int orig_slot_1);
void boot_test_util_verify_all(int expected_swap_type,
                               const struct image_header *hdr0,
                               const struct image_header *hdr1);
#ifdef __cplusplus
}
#endif

#endif /*_BOOT_TEST_H */
