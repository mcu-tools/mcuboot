/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
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

#ifndef H_BOOTUTIL_
#define H_BOOTUTIL_

#include <inttypes.h>
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/bootutil_public.h"

#ifdef __cplusplus
extern "C" {
#endif

struct image_header;
/**
 * A response object provided by the boot loader code; indicates where to jump
 * to execute the main image.
 */
struct boot_rsp {
    /** A pointer to the header of the image to be executed. */
    const struct image_header *br_hdr;

    /**
     * The flash offset of the image to execute.  Indicates the position of
     * the image header within its flash device.
     */
    uint8_t br_flash_dev_id;
    uint32_t br_image_off;
};

/* This is not actually used by mcuboot's code but can be used by apps
 * when attempting to read/write a trailer.
 */
struct image_trailer {
    uint8_t swap_type;
    uint8_t pad1[BOOT_MAX_ALIGN - 1];
    uint8_t copy_done;
    uint8_t pad2[BOOT_MAX_ALIGN - 1];
    uint8_t image_ok;
    uint8_t pad3[BOOT_MAX_ALIGN - 1];
#if BOOT_MAX_ALIGN > 16
    uint8_t magic[BOOT_MAX_ALIGN];
#else
    uint8_t magic[16];
#endif
};


/* you must have pre-allocated all the entries within this structure */
fih_int boot_go(struct boot_rsp *rsp);

struct boot_loader_state;
fih_int context_boot_go(struct boot_loader_state *state, struct boot_rsp *rsp);

#define SPLIT_GO_OK                 (0)
#define SPLIT_GO_NON_MATCHING       (-1)
#define SPLIT_GO_ERR                (-2)

fih_int split_go(int loader_slot, int split_slot, void **entry);

#ifdef __cplusplus
}
#endif

#endif
