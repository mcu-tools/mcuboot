/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2020 Nordic Semiconductor ASA
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

#ifndef H_BOOTUTIL_PUBLIC
#define H_BOOTUTIL_PUBLIC

#include <inttypes.h>
#include <string.h>
#include <flash_map_backend/flash_map_backend.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Attempt to boot the contents of the primary slot. */
#define BOOT_SWAP_TYPE_NONE     1

/**
 * Swap to the secondary slot.
 * Absent a confirm command, revert back on next boot.
 */
#define BOOT_SWAP_TYPE_TEST     2

/**
 * Swap to the secondary slot,
 * and permanently switch to booting its contents.
 */
#define BOOT_SWAP_TYPE_PERM     3

/** Swap back to alternate slot.  A confirm changes this state to NONE. */
#define BOOT_SWAP_TYPE_REVERT   4

/** Swap failed because image to be run is not valid */
#define BOOT_SWAP_TYPE_FAIL     5

/** Swapping encountered an unrecoverable error */
#define BOOT_SWAP_TYPE_PANIC    0xff

#define BOOT_MAX_ALIGN          8

#define BOOT_MAGIC_GOOD     1
#define BOOT_MAGIC_BAD      2
#define BOOT_MAGIC_UNSET    3
#define BOOT_MAGIC_ANY      4  /* NOTE: control only, not dependent on sector */
#define BOOT_MAGIC_NOTGOOD  5  /* NOTE: control only, not dependent on sector */

/*
 * NOTE: leave BOOT_FLAG_SET equal to one, this is written to flash!
 */
#define BOOT_FLAG_SET       1
#define BOOT_FLAG_BAD       2
#define BOOT_FLAG_UNSET     3
#define BOOT_FLAG_ANY       4  /* NOTE: control only, not dependent on sector */

#define BOOT_MAGIC_SZ (sizeof boot_img_magic)

int boot_swap_type_multi(int image_index);
int boot_swap_type(void);

int boot_set_pending(int permanent);
int boot_set_confirmed(void);
uint32_t boot_swap_info_off(const struct flash_area *fap);
int boot_read_image_ok(const struct flash_area *fap, uint8_t *image_ok);

#define BOOT_MAGIC_ARR_SZ \
    (sizeof boot_img_magic / sizeof boot_img_magic[0])

extern const uint32_t boot_img_magic[4];


#ifdef __cplusplus
}
#endif

#endif
