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

#ifndef USE_XIP

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
    st = Cy_Flash_WriteRow(row_addr, (const uint32_t *)row_buff);

    if (CY_FLASH_DRV_SUCCESS == st) {
        rc = 0;
    }

    return rc;
}

#else

extern cy_stc_smif_block_config_t smifBlockConfig_sfdp;
extern cy_stc_smif_mem_config_t *mems_sfdp[1];
extern cy_stc_smif_mem_config_t mem_sfdp_0;
extern cy_stc_smif_mem_device_cfg_t dev_sfdp_0;

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
    uint32_t try_count = 10U;

    cy_en_smif_status_t stat = CY_SMIF_BUSY;
    SMIF_Type *QSPIPort = SMIF0;
    cy_stc_smif_context_t QSPI_context;
    cy_en_smif_mode_t mode = CY_SMIF_NORMAL;

    Cy_SMIF_SetMode(SMIF0, CY_SMIF_NORMAL);
    mode = Cy_SMIF_GetMode(QSPIPort);

    if (mode != CY_SMIF_NORMAL) {
        CY_HALT();
    }

    for (try_count = 0U; try_count < 10U; try_count++) {

        stat = Cy_SMIF_MemInit(QSPIPort, &smifBlockConfig_sfdp, &QSPI_context);

        if (CY_SMIF_SUCCESS == stat) {
            break;
        }

        Cy_SysLib_Delay(500U);
    }

    if (stat == CY_SMIF_SUCCESS) {

        cy_stc_smif_mem_config_t *cfg = smifBlockConfig_sfdp.memConfig[0];
        /* Determine row start address, where image trailer is allocated */
        uint32_t erase_len = cfg->deviceCfg->eraseSize;
        uint32_t row_mask = erase_len /* is a power of two */ - 1u;
        uint32_t row_addr = (address - CY_XIP_BASE) & ~row_mask;
        /* Determine start address of image trailer
         * The minimum erase size area is allocated
         * for trailer, but reading the whole area is
         * not nessesary since data is only located at
         * first 0x200 bytes. Trailer size is taken as 0x200
         * to keep consistency with internal memory
         * implementation, where min_erase_size is 0x200
         */
        uint32_t img_trailer_addr = address - CY_XIP_BASE + USER_SWAP_IMAGE_OK_OFFS - IMG_TRAILER_SZ;
        uint32_t img_ok_mask = FLASH_ROW_BUF_SZ /* is a power of 2 */ - 1u; 

        cy_en_smif_status_t st = Cy_SMIF_MemRead(QSPIPort, cfg,
                                                img_trailer_addr, row_buff, FLASH_ROW_BUF_SZ,
                                                &QSPI_context);

        if (CY_SMIF_SUCCESS == st) {
            
            if (row_buff[address & img_ok_mask] != value) {
                
                row_buff[address & img_ok_mask] = value;

                /* Programming the updated block back */
                st = Cy_SMIF_MemEraseSector(QSPIPort, cfg,
                                            row_addr, erase_len,
                                            &QSPI_context);

                if (CY_SMIF_SUCCESS == st) {
                    st = Cy_SMIF_MemWrite(QSPIPort, cfg,
                                            img_trailer_addr, row_buff, FLASH_ROW_BUF_SZ,
                                            &QSPI_context);
                    if (CY_SMIF_SUCCESS == st) {
                        rc = IMG_OK_SET_SUCCESS;
                    }
                }
                else {
                    rc = IMG_OK_SET_FAILED;
                }
            }
            else {
                rc = IMG_OK_ALREADY_SET;
            }
        }
        else {
            rc = IMG_OK_SET_FAILED;
        }

        stat = Cy_SMIF_CacheEnable(QSPIPort, CY_SMIF_CACHE_FAST);

        if (CY_SMIF_SUCCESS == stat) {
            Cy_SMIF_SetMode(QSPIPort, CY_SMIF_MEMORY);
            mode = Cy_SMIF_GetMode(QSPIPort);

            if (mode != CY_SMIF_MEMORY) {
                CY_HALT();
            }
        }
    }
    else {
        /* do nothing */
    }

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
#ifdef USE_XIP
    /*
     * When switching from XIP execution mode to RAM function
     * it is required to clear and disable SMIF cache. set_img_ok_ram
     * is then turns cache on before return. If it is not done - return
     * to execution from RAM to XIP hangs indefinitely.
     */
    Cy_SMIF_CacheDisable(SMIF0, CY_SMIF_CACHE_FAST);
    Cy_SMIF_CacheInvalidate(SMIF0, CY_SMIF_CACHE_FAST);
    rc = set_img_ok_ram(address, value);

#else

    if (read_img_ok_value(address) != value) {
        rc = write_img_ok_value(address, value);
    }
    else {
        rc = IMG_OK_ALREADY_SET;
    }
#endif /* USE_XIP */

    return rc;
}

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */
