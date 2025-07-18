/********************************************************************************
 * Copyright 2018-2025 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ********************************************************************************/

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) || defined(MCUBOOT_DIRECT_XIP)

#include "set_img_ok.h"
#include <flash_map_backend/flash_map_backend.h>

#include <string.h>

#define EXT_MEM_INTERFACE_ID 0

extern const struct flash_area_interface external_mem_interface;

static uint8_t row_buff[FLASH_ROW_BUF_SZ];

/**
 * @brief Function reads value of img_ok flag from address.
 *
 * @param address - address of img_ok flag in primary img trailer
 * @return int - value at address
 */

static int read_img_ok_value(uint32_t address)
{
    uint8_t tmp = 0U;
    
    external_mem_interface.read(EXT_MEM_INTERFACE_ID, address, &tmp, 1);

    return tmp;
}


/**
 * @brief Function sets img_ok flag value to primary image trailer.
 *
 * @param address - address of img_ok flag in primary img trailer
 * @param value - value corresponding to img_ok set
 *
 * @return - operation status. 0 - set succesfully, -1 - failed to set.
 */

static int write_img_ok_value(uint32_t address, uint8_t src)
{
    int rc = 0;
    /* Accepting an arbitrary address */
    uint32_t row_mask = external_mem_interface.get_erase_size(0) - 1U;
    uint32_t erase_val = external_mem_interface.get_erase_val(0);
    uint32_t index = address & row_mask;

    rc |= external_mem_interface.read(EXT_MEM_INTERFACE_ID, address & ~row_mask, row_buff, FLASH_ROW_BUF_SZ);

    /* Modifying the target byte */
    memset(&row_buff[index], erase_val, sizeof(uint64_t));
    row_buff[index] = src;
    

    rc |= external_mem_interface.erase(EXT_MEM_INTERFACE_ID, address & ~row_mask, FLASH_ROW_BUF_SZ);

    rc |= external_mem_interface.write(EXT_MEM_INTERFACE_ID, address & ~row_mask, row_buff, FLASH_ROW_BUF_SZ);

    return rc;
}


/**
 * @brief Public function to confirm that upgraded application is operable
 * after swap. Should be called from main code of user application.
 * It sets mcuboot flag img_ok in primary (boot) image trailer.
 * MCUBootApp checks img_ok flag at first reset after upgrade and
 * validates successful swap.
 *
 * @param address - address of img_ok flag in primary img trailer
 * @param value - value corresponding to img_ok set
 *
 * @return - operation status. 1 - already set, 0 - set succesfully,
 *                              -1 - failed to set.
 */

int set_img_ok(uint32_t address, uint8_t value)
{
    int32_t rc = -1;

    if (read_img_ok_value(address) != value) {
        rc = write_img_ok_value(address, value);
    } else {
        rc = IMG_OK_ALREADY_SET;
    }

    return rc;
}

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */
