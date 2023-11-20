/*
 * Copyright (c) 2021 Infineon Technologies AG
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
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "flash_map_backend_platform.h"
#include "cy_service_app.h"
#include "flash_qspi.h"

extern const struct flash_area_interface external_mem_interface;

#if defined MCUBOOT_HW_ROLLBACK_PROT

#ifndef CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE
/* This is the value of external flash bytes after an erase */
#define CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE  (0xFFu)
#endif /* CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE */

/* Status code for the successful completion of the service application.  */
#define CYAPP_SUCCESS       (0xF2A00001U)

/*
* Function is used to write data to external flash. Input address can be unaligned
*/
static int flash_write_packet(uint32_t address, uint8_t * data, uint32_t len)
{
    uint32_t row_addr = 0;

    int32_t rc = -1;
    cy_en_smif_status_t st = CY_SMIF_BAD_PARAM;
    cy_stc_smif_mem_config_t *cfg = NULL;
    SMIF_Type * smif_device = NULL;
    uint32_t erase_size = 0U;
    cy_stc_smif_context_t * smif_context = NULL;
    uint8_t row_buff[CY_MAX_EXT_FLASH_ERASE_SIZE];

    cfg = qspi_get_memory_config(0);
    smif_device = qspi_get_device();
    erase_size = qspi_get_erase_size();
    smif_context = qspi_get_context();

    if ( (erase_size > CY_MAX_EXT_FLASH_ERASE_SIZE) ||
        (((address % erase_size) + len) > erase_size) ) {

        return rc;
    }

    if ( (NULL != cfg) && (NULL != smif_device) && (NULL != smif_context) ) {
        uint32_t row_mask = erase_size /* is a power of 2 */ - 1U;

        /* Accepting an arbitrary address */
        row_addr = (address - CY_XIP_BASE) & ~row_mask;

        /* Preserving the block */
        st = Cy_SMIF_MemRead(smif_device, cfg, row_addr, row_buff, erase_size, smif_context);

        if (CY_SMIF_SUCCESS == st) {
            /* Modifying target bytes */
            (void)memcpy(row_buff + (address & row_mask), data, len);

            /* Programming the updated block back */
            st = Cy_SMIF_MemEraseSector(smif_device, cfg, row_addr, erase_size, smif_context);

            if (CY_SMIF_SUCCESS == st) {
                st = Cy_SMIF_MemWrite(smif_device, cfg, row_addr, row_buff, erase_size, smif_context);

                if (CY_SMIF_SUCCESS == st) {
                    rc = 0;
                }
            }
        }
    }

    return rc;
}

/*
* Reads data from the external flash by arbitrary address.
*/
static int32_t flash_read(uint32_t address, uint8_t * data, uint32_t len)
{
    int32_t rc = -1;
    cy_en_smif_status_t st = CY_SMIF_BAD_PARAM;
    cy_stc_smif_mem_config_t *cfg = NULL;
    SMIF_Type * smif_device = NULL;
    cy_stc_smif_context_t * smif_context = NULL;

    cfg = qspi_get_memory_config(0);
    smif_device = qspi_get_device();
    smif_context = qspi_get_context();

    if ( (NULL != cfg) && (NULL != smif_device) && (NULL != smif_context) && (NULL != data) &&
         (address >= CY_XIP_BASE) ) {
        st = Cy_SMIF_MemRead(smif_device, cfg, (address - CY_XIP_BASE), data, len, smif_context);

        if (CY_SMIF_SUCCESS == st) {
            rc = 0;
        }
    }

    return rc;
}

/*
* In CYW20829 security counter can only be updated using special service application,
* which is executed by BootROM. Function initializes suplement data for service app and
* triggers system reset. BootROM is then runs service app, which performs actual update
* of security counter value in chips efuse.
*/
void call_service_app(uint8_t * reprov_packet)
{
    int32_t rc = -1;
    service_app_desc_type reprov_app_desc;

    /* Initialize service app descriptor */
    reprov_app_desc.service_app_descr_size = SERVICE_APP_DESC_SIZE;
    reprov_app_desc.service_app_addr = SERVICE_APP_OFFSET;
    reprov_app_desc.service_app_size = SERVICE_APP_SIZE;
    reprov_app_desc.input_param_addr = SERVICE_APP_INPUT_PARAMS_OFFSET;
    reprov_app_desc.input_param_size = REPROV_PACK_SIZE;

    /* Put service app suplement data in flash */
    if (NULL != reprov_packet)
    {
        /* Write input params */
        rc = flash_write_packet((CY_XIP_BASE + SERVICE_APP_INPUT_PARAMS_OFFSET),
                                reprov_packet, REPROV_PACK_SIZE);
        if (0 == rc) {
            /* Write application descriptor. The address of the application
             * descriptor is already present in the TOC2 (offset 0x8). */
            rc = flash_write_packet((CY_XIP_BASE + SERVICE_APP_DESC_OFFSET),
                                    (uint8_t *)&reprov_app_desc, sizeof(reprov_app_desc));
            if (0 == rc) {
                /* Set code to tell BootROM to launch a service app downloaded to RAM from an external memory */
                SRSS->BOOT_DLM_CTL = CYBOOT_REQUEST_EXT_APP;
                /* Trigger device reset */
                __NVIC_SystemReset();
            }
        }
    }
}

/*
* Checks the service application completion status.
*
* Reads the service app descriptor from the flash. If it is populated, erases the service app
* descriptor and verifies that the application status in the BOOT_DLM_CTL2 register
* contains the CYAPP_SUCCESS value.
* Function limitations:
*     - assumes that the service app descriptor is located in external flash;
*     - erases the entire sector where the service app descriptor is located.
*
* Returns 0 if the service app descriptor is empty or BOOT_DLM_CTL2 register
* contains the CYAPP_SUCCESS value. Otherwise it returns -1.
*/
int32_t check_service_app_status(void)
{
    int32_t rc = -1;
    uint8_t reprov_app_desc[sizeof(service_app_desc_type)] = {0};

    rc = flash_read((CY_XIP_BASE + SERVICE_APP_DESC_OFFSET),
                    reprov_app_desc, sizeof(reprov_app_desc));
    if (0 == rc) {
        if (bootutil_buffer_is_filled(reprov_app_desc, CY_BOOT_EXTERNAL_FLASH_ERASE_VALUE, sizeof(reprov_app_desc))) {
            rc = 0;
        }
        else {
            rc = external_mem_interface.erase(
                FLASH_DEVICE_EXTERNAL_FLASH(0),
                (CY_XIP_BASE + SERVICE_APP_DESC_OFFSET), qspi_get_erase_size());
            if (0 == rc) {
                if (CYAPP_SUCCESS == SRSS->BOOT_DLM_CTL2) {
                    rc = 0;
                } else {
                    rc = -1;
                }
            }
        }
    }

    return rc;
}

#endif /* MCUBOOT_HW_ROLLBACK_PROT */
