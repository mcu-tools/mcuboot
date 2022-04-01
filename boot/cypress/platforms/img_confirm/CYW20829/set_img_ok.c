/********************************************************************************
* Copyright 2021 Infineon Technologies AG
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

#include "set_img_ok.h"

static uint8_t row_buff[FLASH_ROW_BUF_SZ];

/**
 * @brief Function reads value of img_ok flag from address.
 * 
 * @param address - address of img_ok flag in primary img trailer
 * @return int - value at address
 */
static int read_img_ok_value(uint32_t address)
{
    uint32_t row_mask = qspi_get_erase_size() /* is a power of 2 */ - 1u;
    uint32_t row_addr = (address - CY_XIP_BASE) & ~row_mask;

    cy_stc_smif_mem_config_t *cfg = qspi_get_memory_config(0);
    cy_en_smif_status_t st = Cy_SMIF_MemRead(qspi_get_device(), cfg,
                                             row_addr, row_buff, qspi_get_erase_size(),
                                             qspi_get_context());
    if (CY_SMIF_SUCCESS == st) {
        return row_buff[address & row_mask];
    }

    return -1;
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
    int rc = -1;
    uint32_t row_addr = 0;

    cy_stc_smif_mem_config_t *cfg = qspi_get_memory_config(0);
    uint32_t row_mask = qspi_get_erase_size() /* is a power of 2 */ - 1u;
    cy_en_smif_status_t st;

    /* Accepting an arbitrary address */
    row_addr = (address - CY_XIP_BASE) & ~row_mask;

    /* Preserving the block */
    st = Cy_SMIF_MemRead(qspi_get_device(), cfg,
                         row_addr, row_buff, qspi_get_erase_size(),
                         qspi_get_context());

    if (CY_SMIF_SUCCESS == st) {
        /* Modifying the target byte */
        row_buff[address & row_mask] = src;

        /* Programming the updated block back */
        st = Cy_SMIF_MemEraseSector(qspi_get_device(), cfg,
                                    row_addr, qspi_get_erase_size(),
                                    qspi_get_context());

        if (CY_SMIF_SUCCESS == st) {
            st = Cy_SMIF_MemWrite(qspi_get_device(), cfg,
                                  row_addr, row_buff, qspi_get_erase_size(),
                                  qspi_get_context());
        }
    }

    if (CY_SMIF_SUCCESS == st) {
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

    if (read_img_ok_value(address) != value) {
        rc = write_img_ok_value(address, value);
    }
    else {
        rc = IMG_OK_ALREADY_SET;
    }

    return rc;
}

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */
