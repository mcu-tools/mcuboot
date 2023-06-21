/**
 * \file external_memory.c
 * \version 1.0
 *
 * \brief
 *  This is the source file of external flash driver adoption layer between
 *PSoC6 and standard MCUBoot code.
 *
 ********************************************************************************
 * \copyright
 *
 * (c) 2020, Cypress Semiconductor Corporation
 * or a subsidiary of Cypress Semiconductor Corporation. All rights
 * reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software"), is owned by Cypress Semiconductor
 * Corporation or one of its subsidiaries ("Cypress") and is protected by
 * and subject to worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-
 * exclusive, non-transferable license to copy, modify, and compile the
 * Software source code solely for use in connection with Cypress?s
 * integrated circuit products. Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO
 * WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING,
 * BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE. Cypress reserves the right to make
 * changes to the Software without notice. Cypress does not assume any
 * liability arising out of the application or use of the Software or any
 * product or circuit described in the Software. Cypress does not
 * authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *
 ******************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysflash/sysflash.h>

#include "cy_device_headers.h"
#include "cy_flash.h"
#include "cy_syspm.h"
#include "flash_map_backend/flash_map_backend.h"
#include "flash_map_backend_platform.h"
#include "flash_qspi.h"

static uint32_t get_base_address(uint8_t fa_device_id)
{
    uint32_t res = 0U;

    if ((fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) ==
        FLASH_DEVICE_EXTERNAL_FLAG) {
        res = SMIF_MEM_START_PLATFORM;
    } else {
        res = 0U;
    }

    return res;
}

static uint32_t get_min_erase_size(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return qspi_get_erase_size();
}

static uint8_t get_erase_val(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM;
}

static int read(uint8_t fa_device_id, uintptr_t addr, void *data, uint32_t len)
{
    (void)fa_device_id;

    int rc = -1;
    cy_stc_smif_mem_config_t *cfg;
    cy_en_smif_status_t st;
    uint32_t offset;

    cfg = qspi_get_memory_config(0u);

    offset = (uint32_t)addr - SMIF_MEM_START_PLATFORM;

    st = Cy_SMIF_MemRead(qspi_get_device(), cfg, offset, data, len,
                         qspi_get_context());
    if (st == CY_SMIF_SUCCESS) {
        rc = 0;
    }
    return rc;
}

static int write(uint8_t fa_device_id, uintptr_t addr, const void *data,
                 uint32_t len)
{
    (void)fa_device_id;

    int rc                 = -1;
    cy_en_smif_status_t st = CY_SMIF_SUCCESS;
    cy_stc_smif_mem_config_t *cfg;
    uint32_t offset;

    cfg = qspi_get_memory_config(0u);

    offset = (uint32_t)addr - SMIF_MEM_START_PLATFORM;

#if defined CYW20829
    st = Cy_SMIF_MemEraseSector(qspi_get_device(), cfg, offset,
                                qspi_get_erase_size(), qspi_get_context());

    if (st == CY_SMIF_SUCCESS) {
#endif /* CYW20829 */
        st = Cy_SMIF_MemWrite(qspi_get_device(), cfg, offset, data, len,
                              qspi_get_context());
#if defined CYW20829
    }
#endif
    if (st == CY_SMIF_SUCCESS) {
        rc = 0;
    }
    return rc;
}

static int erase(uint8_t fa_device_id, uintptr_t addr, uint32_t size)
{
    (void)fa_device_id;

    int rc                 = -1;
    cy_en_smif_status_t st = CY_SMIF_SUCCESS;
    uint32_t offset;

    if (size > 0u) {
        /* It is erase sector-only
         *
         * There is no power-safe way to erase flash partially
         * this leads upgrade slots have to be at least
         * eraseSectorSize far from each other;
         */
        cy_stc_smif_mem_config_t *memCfg = qspi_get_memory_config(0);
        uint32_t eraseSize               = qspi_get_erase_size();

        offset = ((uint32_t)addr - SMIF_MEM_START_PLATFORM) & ~((uint32_t)(eraseSize - 1u));

        while ((size > 0u) && (CY_SMIF_SUCCESS == st)) {
            st = Cy_SMIF_MemEraseSector(qspi_get_device(), memCfg, offset,
                                        eraseSize, qspi_get_context());

            size -= (size >= eraseSize) ? eraseSize : size;
            offset += eraseSize;
        }

        if (st == CY_SMIF_SUCCESS) {
            rc = 0;
        }
    }

    return rc;
}

static int open(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return 0;
}

static void close(uint8_t fa_device_id)
{
    (void)fa_device_id;
}

const struct flash_area_interface external_mem_interface = {
    .open             = &open,
    .close            = &close,
    .read             = &read,
    .write            = &write,
    .erase            = &erase,
    .get_erase_val    = &get_erase_val,
    .get_erase_size   = &get_min_erase_size,
    .get_base_address = &get_base_address};
