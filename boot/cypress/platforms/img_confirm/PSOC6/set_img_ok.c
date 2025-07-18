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

#include "set_img_ok.h"

#include "cy_flash.h"
#include "string.h"

#if defined(CY_BOOT_USE_EXTERNAL_FLASH)
#include "flash_qspi.h"
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) */

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

#ifndef USE_XIP
/**
 * @brief Function sets img_ok flag value to primary image trailer.
 *
 * @param address - address of img_ok flag in primary img trailer
 * @param value - value corresponding to img_ok set
 *
 * @return - operation status. 0 - set succesfully, -1 - failed to set.
 */
CY_RAMFUNC_BEGIN
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
    st = Cy_Flash_WriteRow(row_addr, (const uint32_t *)row_buff);

    if (CY_FLASH_DRV_SUCCESS == st) {
        rc = 0;
    }

    return rc;
}
CY_RAMFUNC_END

#else

/**
 * @brief Function sets img_ok value to primary slot trailer
 *        when application is executed from external memory
 *        in XIP mode. This function is executed from RAM since
 *        it reconfigures SMIF block from XIP to MMIO mode, then
 *        writes img_ok set value and switches back to XIP mode.
 *
 * @param address - address of img_ok flag in primary img trailer
 * @param value - value corresponding to img_ok set
 *
 * @return - operation status. 1 - already set, 0 - set succesfully,
 *                              -1 - failed to set.
 */
CY_RAMFUNC_BEGIN
static int set_img_ok_ram(uint32_t address, uint8_t value)
{
    int32_t rc = IMG_OK_SET_FAILED;

    SMIF_Type *QSPIPort = SMIF0;
    cy_stc_smif_context_t QSPI_context = {0};
    cy_stc_smif_mem_config_t *cfg = smifBlockConfig_sfdp.memConfig[0];

    const uint32_t trailer_row_abs_addr = address & ~(MEMORY_ALIGN - 1);
    const uint32_t trailer_row_addr = (address - CY_XIP_BASE) & ~(MEMORY_ALIGN - 1);

    memcpy(row_buff, (void *)trailer_row_abs_addr, MEMORY_ALIGN);

    Cy_SMIF_SetMode(SMIF0, CY_SMIF_NORMAL);

    Cy_SMIF_MemDeInit(QSPIPort);

    cy_en_smif_status_t status = Cy_SMIF_MemInit(QSPIPort, &smifBlockConfig_sfdp, &QSPI_context);

    if (status == CY_SMIF_SUCCESS) {
        row_buff[address & (MEMORY_ALIGN - 1)] = value;

        if (CY_SMIF_SUCCESS == Cy_SMIF_MemEraseSector(QSPIPort, cfg,
                                                      trailer_row_addr, MEMORY_ALIGN, &QSPI_context)) {
            if (CY_SMIF_SUCCESS == Cy_SMIF_MemWrite(QSPIPort, cfg, trailer_row_addr,
                                                    row_buff, MEMORY_ALIGN, &QSPI_context)) {
                rc = IMG_OK_SET_SUCCESS;
            }
        }
    }

    Cy_SMIF_SetMode(SMIF0, CY_SMIF_MEMORY);

    return rc;
}
CY_RAMFUNC_END

#endif /* USE_XIP */

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
#ifdef USE_XIP
        rc = set_img_ok_ram(address, value);
#else
        rc = write_img_ok_value(address, value);
#endif /* USE_XIP */
    } else {
        rc = IMG_OK_ALREADY_SET;
    }

    return rc;
}

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */
