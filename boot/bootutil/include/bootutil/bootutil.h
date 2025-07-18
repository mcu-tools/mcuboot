/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
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
#include "bootutil/image.h"

#ifdef MCUBOOT_ENC_IMAGES_XIP
#include "bootutil/enc_key.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MCUBOOT_IMAGE_NUMBER
#define BOOT_IMAGE_NUMBER          MCUBOOT_IMAGE_NUMBER
#else
#define BOOT_IMAGE_NUMBER          1
#endif

_Static_assert(BOOT_IMAGE_NUMBER > 0, "Invalid value for BOOT_IMAGE_NUMBER");

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

#ifdef MCUBOOT_ENC_IMAGES_XIP
    uint32_t xip_key[BOOTUTIL_CRYPTO_AES_CTR_KEY_SIZE/4];
    uint32_t xip_iv[BOOTUTIL_CRYPTO_AES_CTR_BLOCK_SIZE/4];
#endif
};

/* This is not actually used by mcuboot's code but can be used by apps
 * when attempting to read/write a trailer.
 */
struct image_trailer {
#if defined(MCUBOOT_DIRECT_XIP)
    uint8_t image_inactive;
    uint8_t pad0[BOOT_MAX_ALIGN - 1];
#endif
    uint8_t swap_type;
    uint8_t pad1[BOOT_MAX_ALIGN - 1];
    uint8_t copy_done;
    uint8_t pad2[BOOT_MAX_ALIGN - 1];
    uint8_t image_ok;
    uint8_t pad3[BOOT_MAX_ALIGN - 1];
#if BOOT_MAX_ALIGN > BOOT_MAGIC_SZ
    uint8_t pad4[BOOT_MAGIC_ALIGN_SIZE - BOOT_MAGIC_SZ];
#endif
    uint8_t magic[BOOT_MAGIC_SZ];
};

typedef enum
{
    MCUBOOT_SLOT_STATE_NO_IMAGE = 0,
    MCUBOOT_SLOT_STATE_ACTIVE,
    MCUBOOT_SLOT_STATE_PENDING,
    MCUBOOT_SLOT_STATE_VERIFYING,
    MCUBOOT_SLOT_STATE_INACTIVE
} boot_slot_state_t;

/* you must have pre-allocated all the entries within this structure */
fih_int boot_go(struct boot_rsp *rsp);
fih_int boot_go_for_image_id(struct boot_rsp *rsp, uint32_t image_id);

/**
 * @brief Performs slot validation without performing boot
 *
 * @param image_id - Image number, starting from 0
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot
 *
 * @retval FIH_SUCCESS on success
 */
fih_int
boot_validate_slot_for_image_id(uint32_t image_id, uint32_t slot_id);

/**
 * @brief Reads image version fields from the image header area
 *
 * @param image_id - Image number, starting from 0
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot
 * 
 * @param ver - Version structure output
 *
 * @retval 0 on success
 */
int
boot_get_image_version(uint32_t image_id, uint32_t slot_id, struct image_version* ver);

#if defined(MCUBOOT_DIRECT_XIP)
/**
 * @brief Marks the slot as inactive.
 *
 * @param image_id - Image number, starting from 0.
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot.
 *
 * @retval 0 on success
 */
int 
boot_set_inactive_slot(uint32_t image_id, uint32_t slot_id);

/**
 * @brief Checks if the slot is inactive.
 *
 * @param image_id - Image number, starting from 0.
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot.
 *
 * @retval 1 The slot is set as inactive
 *         0 The slot is not set as inactive
 *        -1 On error
 */
int 
boot_is_slot_inactive(uint32_t image_id, uint32_t slot_id);

/**
 * @brief Restores slot from inactive state to make it able to boot.
 *
 * @param image_id - Image number, starting from 0
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot
 *
 * @retval 0 on success
 * 
 */
int
boot_set_pending_slot(uint32_t image_id, uint32_t slot_id);


/**
 * @brief Sets the revert state to the slot. Image will be erased on next reboot
 *
 * @param image_id - Image number, starting from 0
 * 
 * @param slot_id - Slot index: 0 - primary slot, 1 - secondary slot
 *
 * @retval 0 on success
 * 
 */
int
boot_set_revert_slot(uint32_t image_id, uint32_t slot_id);

/**
 * @brief Finds the TLV (Type-Length-Value) metadata information for a specific image and slot.
 *
 * Retrieves offset and length information for the requested TLV entry.
 *
 * @param image_id Image identifier (starting from 0).
 * 
 * @param slot_id  Slot index (0 for primary slot, 1 for secondary slot).
 * 
 * @param type     TLV type identifier.
 * 
 * @param len      Output pointer for storing TLV length.
 * 
 * @param off      Output pointer for storing TLV offset.
 *
 * @retval 0 Success.
 * @retval Non-zero Error occurred or TLV not found.
 */
int
boot_find_image_tlv_info(uint32_t image_id, uint32_t slot_id, uint16_t type, uint16_t* len, uint32_t* off);

/**
 * @brief Reads the TLV metadata value from a specified image slot.
 *
 * Fetches the actual data associated with the specified TLV type.
 *
 * @param image_id Image identifier (starting from 0).
 * 
 * @param slot_id  Slot index (0 for primary slot, 1 for secondary slot).
 * 
 * @param type     TLV type identifier.
 * 
 * @param buf      Buffer pointer to store the retrieved TLV value.
 * 
 * @param buf_len  Length of provided buffer.
 * 
 * @param read_len Output pointer for storing the actual number of bytes read.
 *
 * @retval 0 Success.
 * @retval Non-zero Error occurred or insufficient buffer length.
 */
int
boot_read_image_tlv_value(uint32_t image_id, uint32_t slot_id, uint16_t type, uint8_t* buf, uint32_t buf_len, uint32_t* read_len);

/**
 * @brief Retrieves the current state of the specified slot.
 *
 * Provides details about the current slot status without image verification.
 *
 * @param image_id Image identifier (starting from 0).
 * 
 * @param slot_id  Slot index (0 for primary slot, 1 for secondary slot).
 * 
 * @param state    Output pointer to store the slot state information.
 *
 * @retval 0 Success.
 * @retval Non-zero Error occurred.
 */
int
boot_get_slot_state(uint32_t image_id, uint32_t slot_id, boot_slot_state_t* state);

/**
 * @brief Retrieves the current state of the specified image slot.
 *
 * Provides details about the current status (active, inactive, pending, etc.) of the specified slot.
 *
 * @param image_id Image identifier (starting from 0).
 * 
 * @param slot_id  Slot index (0 for primary slot, 1 for secondary slot).
 * 
 * @param state    Output pointer to store the slot state information.
 *
 * @retval 0 Success.
 * @retval Non-zero Error occurred.
 */
int
boot_get_image_state(uint32_t image_id, uint32_t slot_id, boot_slot_state_t* state);

#endif

struct boot_loader_state;
void boot_state_clear(struct boot_loader_state *state);
fih_int context_boot_go_flash(struct boot_loader_state *state, struct boot_rsp *rsp);

#if defined(MCUBOOT_RAM_LOAD) || defined(MCUBOOT_DIRECT_XIP)
fih_int context_boot_go_ram(struct boot_loader_state *state, struct boot_rsp *rsp);
fih_int boot_go_for_image_id_ram(struct boot_rsp *rsp, uint32_t image_id);
#endif


#define SPLIT_GO_OK                 (0)
#define SPLIT_GO_NON_MATCHING       (-1)
#define SPLIT_GO_ERR                (-2)

fih_int split_go(int loader_slot, int split_slot, void **entry);

#ifdef __cplusplus
}
#endif

#endif
