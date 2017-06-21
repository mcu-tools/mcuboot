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

#ifndef H_BOOTUTIL_
#define H_BOOTUTIL_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Attempt to boot the contents of slot 0. */
#define BOOT_SWAP_TYPE_NONE     1

/** Swap to slot 1.  Absent a confirm command, revert back on next boot. */
#define BOOT_SWAP_TYPE_TEST     2

/** Swap to slot 1, and permanently switch to booting its contents. */
#define BOOT_SWAP_TYPE_PERM     3

/** Swap back to alternate slot.  A confirm changes this state to NONE. */
#define BOOT_SWAP_TYPE_REVERT   4

/** Swap failed because image to be run is not valid */
#define BOOT_SWAP_TYPE_FAIL     5

/** Swapping encountered an unrecoverable error */
#define BOOT_SWAP_TYPE_PANIC    0xff

#define MAX_FLASH_ALIGN         8
extern const uint32_t BOOT_MAX_ALIGN;

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
    uint8_t copy_done;
    uint8_t pad1[MAX_FLASH_ALIGN - 1];
    uint8_t image_ok;
    uint8_t pad2[MAX_FLASH_ALIGN - 1];
    uint8_t magic[16];
};

/* you must have pre-allocated all the entries within this structure */
int boot_go(struct boot_rsp *rsp);

int boot_swap_type(void);

int boot_set_pending(int permanent);
int boot_set_confirmed(void);

#define MCUBOOT_REQ_FLASH_MAP_SIZE          0
#define MCUBOOT_REQ_FLASH_MAP_INFO          1

struct mcuboot_api_itf {
    uint32_t mcuboot_api_magic;
    uint32_t mcuboot_version;
    int (*mcuboot_ioctl)(int req, void *data, size_t len);
};

extern const struct mcuboot_api_itf mcuboot_api_vt;
extern struct mcuboot_api_itf *p_mcuboot_api_vt;

#if defined(__arm__)
    #define MCUBOOT_API_INIT_(m)                                            \
        extern const struct mcuboot_api_itf mcuboot_api_vt;                 \
        asm("mov r4, %0\n\t"                                                \
            "mov r5, %1"  :: "I" (m), "r" (&mcuboot_api_vt) : "r4", "r5");
#elif defined(__mips__)
    /* TODO */
    #define MCUBOOT_API_INIT_(m)
#elif defined(__i386__)
    /* TODO */
    #define MCUBOOT_API_INIT_(m)
#endif

/* api virtual function call table magic marker */
#define MCUBOOT_API_MAGIC             0x8b2d7757

/* magic added to a register to mark bootloader api is valid */
#define MCUBOOT_BOOT_MAGIC            0xb00710ad
#define MCUBOOT_API_INIT(x)           MCUBOOT_API_INIT_(MCUBOOT_BOOT_MAGIC)

#define SPLIT_GO_OK                 (0)
#define SPLIT_GO_NON_MATCHING       (-1)
#define SPLIT_GO_ERR                (-2)
int
split_go(int loader_slot, int split_slot, void **entry);

#ifdef __cplusplus
}
#endif

#endif
