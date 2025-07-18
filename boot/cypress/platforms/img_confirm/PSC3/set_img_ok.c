/********************************************************************************
* Copyright 2025 Infineon Technologies AG
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

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE)

#include <string.h>
#include "set_img_ok.h"
#include "cy_flash.h"

static uint8_t row_buff[FLASH_ROW_BUF_SZ];

/**
 * @brief Function reads value of img_ok flag from address.
 * 
 * @param address - address of img_ok flag in primary img trailer
 * @return int - value at address
 */
static int read_img_ok_value(uint32_t address)
{
    return *(volatile uint8_t *)address;
}

/**
 * @brief Function sets img_ok flag value to primary image trailer.
 * 
 * @param address - address of img_ok flag in primary img trailer
 * @param value - value corresponding to img_ok set
 * 
 * @return - operation status. 0 - set succesfully, -1 - failed to set.
 */
static int write_img_ok_value(uint32_t address, uint8_t value)
{
    int rc = -1;
    uint32_t row_addr = 0;

    uint32_t row_mask = CY_FLASH_SIZEOF_ROW /* is a power of 2 */ - 1u;
    cy_en_flashdrv_status_t st;

    /* Accepting an arbitrary address */
    row_addr = address & ~row_mask;

    /* Preserving the row */
    (void)memcpy(row_buff, (void *)row_addr, sizeof(row_buff));

    /* Modifying the target byte */
    row_buff[address & row_mask] = value;

    /* Programming the updated row back */
    st = Cy_Flash_ProgramRow(row_addr, (const uint32_t *)row_buff);

    if (CY_FLASH_DRV_SUCCESS == st) {
        rc = 0;
    }

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

    /* Write Image OK flag to the slot trailer, so MCUBoot-loader
     * will not revert new image
     */

    if (read_img_ok_value(address) != value) {
        rc = write_img_ok_value(address, value);
    }
    else {
        rc = IMG_OK_ALREADY_SET;
    }

    return rc;
}

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */
