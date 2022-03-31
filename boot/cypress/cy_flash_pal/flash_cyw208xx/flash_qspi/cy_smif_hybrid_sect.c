/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*******************************************************************************
* \file cy_smif_hybrid_sect.c
* \version 1.0
*
* \brief
*  This is the source file of external flash driver for Semper Flash with
*  hybrid sectors.
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

#include "flash_qspi.h"
#include "cy_smif_hybrid_sect.h"

#define SMIF_TRANSFER_TIMEOUT   (1000UL) /* The timeout (microseconds) to use in polling of
                                          * the transfer status of the SMIF block
                                          */

#define SEMPER_ID_MANUF         (0x34U)
#define SEMPER_ID_DEV_MSB1      (0x2AU)
#define SEMPER_ID_DEV_MSB2      (0x2BU)
#define SEMPER_ID_DEV_LSB1      (0x19U)
#define SEMPER_ID_DEV_LSB2      (0x1AU)
#define SEMPER_ID_DEV_LSB3      (0x1BU)
#define SEMPER_ID_LEN           (0x0FU)
#define SEMPER_ID_SECTARCH      (0x03U)
#define SEMPER_ID_FAMILY        (0x90U)

#define SEMPER_WRARG_CMD        (0x71U) /* Write Any Register command */
#define SEMPER_RDARG_CMD        (0x65U) /* Read Any Register command */
#define SEMPER_EN4BA_CMD        (0xB7U) /* Enter 4 Byte Address Mode */
#define SEMPER_ER256_CMD        (0xD8U) /* Erase 256-KB Sector */
#define SEMPER_RDIDN_CMD        (0x9FU) /* Read manufacturer and device identification */

#define SEMPER_CFR2N_ADDR       (0x00000003UL) /* Non-volatile Configuration Register 2 address */
#define SEMPER_CFR3N_ADDR       (0x00000004UL) /* Non-volatile Configuration Register 3 address */

#define SEMPER_CFR2N_ADRBYT     (1U << 7U) /* Address Byte Length selection bit offset in CFR2N register */
#define SEMPER_CFR3N_UNHYSA     (1U << 3U) /* Uniform or Hybrid Sector Architecture selection bit offset in CFR3N register */

#define SEMPER_WRARG_DATA_INDEX (4U) /* Input data index for WRARG command */

#define SEMPER_WR_NV_TIMEOUT    (500000U) /* Nonvolatile Register Write operation timeout */
#define SEMPER_ERASE_256KB_TIME (6000U)   /* 256 KB Sector Erase Time in ms */

#define SEMPER_ADDR_LEN         (4U)

#define SEMPER_ERASE_SIZE       (262144u) /* Erase size for Semper Flash is 256KB */

#define PARAM_ID_MSB_OFFSET     (0x08U)  /* The offset of Parameter ID MSB */
#define PARAM_ID_LSB_MASK       (0xFFUL) /* The mask of Parameter ID LSB */

static cy_en_smif_status_t qspi_read_register(uint32_t address, uint8_t *value);
static cy_en_smif_status_t qspi_write_register(uint32_t address, uint8_t value);
static cy_en_smif_status_t qspi_enter_4byte_addr_mode(void);
static void value_to_byte_array(uint32_t value, uint8_t *byteArray,
                                uint32_t startPos, uint32_t size);

/* Checks device and manufacturer ID. Expects ID buffer to be 6 byte length */
bool qspi_is_semper_flash(uint8_t const id[], uint16_t length)
{
    bool isSemper = false;

    if(id != NULL)
    {
        isSemper = true;

        /* Check Manufacturer and Device ID if it is Semper flash */
        if(id[length - 6u] != SEMPER_ID_MANUF)
        {
            isSemper = false;
        }

        if(isSemper && ((id[length - 5u] != SEMPER_ID_DEV_MSB1) &&
                        (id[length - 5u] != SEMPER_ID_DEV_MSB2)))
        {
            isSemper = false;
        }

        if(isSemper && ((id[length - 4u] != SEMPER_ID_DEV_LSB1) &&
                        (id[length - 4u] != SEMPER_ID_DEV_LSB2) &&
                        (id[length - 4u] != SEMPER_ID_DEV_LSB3)))
        {
            isSemper = false;
        }

        if(isSemper && (id[length - 3u] != SEMPER_ID_LEN))
        {
            isSemper = false;
        }

        if(isSemper && (id[length - 2u] != SEMPER_ID_SECTARCH))
        {
            isSemper = false;
        }

        if(isSemper && (id[length - 1u] != SEMPER_ID_FAMILY))
        {
            isSemper = false;
        }
    }

    return isSemper;
}

cy_en_smif_status_t qspi_configure_semper_flash(void)
{
    cy_en_smif_status_t status;
    uint8_t regVal;
    cy_stc_smif_mem_config_t *memCfg;

    status = qspi_enter_4byte_addr_mode();

    if(CY_SMIF_SUCCESS == status)
    {
        /* Set Address Byte Length selection to 4 bytes for instructions */
        status = qspi_read_register(SEMPER_CFR2N_ADDR, &regVal);
        if((CY_SMIF_SUCCESS == status) &&
           ((regVal & SEMPER_CFR2N_ADRBYT) == 0U))
        {
            regVal |= SEMPER_CFR2N_ADRBYT;
            status = qspi_write_register(SEMPER_CFR2N_ADDR, regVal);
        }

        /* Enable Uniform Sector Architecture selection */
        if(CY_SMIF_SUCCESS == status)
        {
            status = qspi_read_register(SEMPER_CFR3N_ADDR, &regVal);
            if((CY_SMIF_SUCCESS == status) &&
               ((regVal & SEMPER_CFR3N_UNHYSA) == 0U))
            {
                regVal |= SEMPER_CFR3N_UNHYSA;
                status = qspi_write_register(SEMPER_CFR3N_ADDR, regVal);
            }
        }

        if(CY_SMIF_SUCCESS == status)
        {
            memCfg = qspi_get_memory_config(0);
            memCfg->deviceCfg->eraseSize = SEMPER_ERASE_SIZE;
            memCfg->deviceCfg->eraseCmd->command = SEMPER_ER256_CMD;
            memCfg->deviceCfg->eraseTime = SEMPER_ERASE_256KB_TIME;
        }
    }

    return status;
}

/* Read 6 bytes of Manufacturer and Device ID */
cy_en_smif_status_t qspi_read_memory_id(uint8_t *id, uint16_t length)
{
    cy_en_smif_status_t status;
    cy_stc_smif_mem_config_t *memCfg;
    SMIF_Type *QSPIPort;
    cy_stc_smif_context_t *QSPI_context;
    uint16_t dummyCycles = 64u;

    memCfg = qspi_get_memory_config(0);
    QSPIPort = qspi_get_device();
    QSPI_context = qspi_get_context();

    status = Cy_SMIF_TransmitCommand(QSPIPort,
                                    SEMPER_RDIDN_CMD,
                                    CY_SMIF_WIDTH_SINGLE,
                                    NULL,
                                    0u,
                                    CY_SMIF_WIDTH_SINGLE,
                                    memCfg->slaveSelect,
                                    CY_SMIF_TX_NOT_LAST_BYTE,
                                    QSPI_context);

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_SendDummyCycles(QSPIPort, dummyCycles);
    }

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_ReceiveDataBlocking(QSPIPort, id, length,
                                    CY_SMIF_WIDTH_SINGLE,
                                    QSPI_context);
    }

    return status;
}

static cy_en_smif_status_t qspi_read_register(uint32_t address, uint8_t *value)
{
    cy_en_smif_status_t status;
    cy_stc_smif_mem_config_t *memCfg;
    SMIF_Type *QSPIPort;
    cy_stc_smif_context_t *QSPI_context;
    uint8_t addressArray[SEMPER_ADDR_LEN];
    uint16_t dummyCycles = 8u;

    value_to_byte_array(address, addressArray, 0u, sizeof(addressArray));

    memCfg = qspi_get_memory_config(0);
    QSPIPort = qspi_get_device();
    QSPI_context = qspi_get_context();

    status = Cy_SMIF_TransmitCommand(QSPIPort,
                                    SEMPER_RDARG_CMD,
                                    CY_SMIF_WIDTH_SINGLE,
                                    addressArray,
                                    sizeof(addressArray),
                                    CY_SMIF_WIDTH_SINGLE,
                                    memCfg->slaveSelect,
                                    CY_SMIF_TX_NOT_LAST_BYTE,
                                    QSPI_context);

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_SendDummyCycles(QSPIPort, dummyCycles);
    }

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_ReceiveDataBlocking(QSPIPort, value,
                    CY_SMIF_READ_ONE_BYTE, CY_SMIF_WIDTH_SINGLE, QSPI_context);
    }

    return status;
}

static cy_en_smif_status_t qspi_write_register(uint32_t address, uint8_t value)
{
    cy_en_smif_status_t status;
    cy_stc_smif_mem_config_t *memCfg;
    SMIF_Type *QSPIPort;
    cy_stc_smif_context_t *QSPI_context;
    uint8_t data[SEMPER_ADDR_LEN + 1u];

    value_to_byte_array(address, data, 0u, SEMPER_ADDR_LEN);

    data[SEMPER_WRARG_DATA_INDEX] = value;

    memCfg = qspi_get_memory_config(0);
    QSPIPort = qspi_get_device();
    QSPI_context = qspi_get_context();

    status = Cy_SMIF_MemCmdWriteEnable(QSPIPort, memCfg, QSPI_context);

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_TransmitCommand(QSPIPort, SEMPER_WRARG_CMD,
                                        CY_SMIF_WIDTH_SINGLE,
                                        data, sizeof(data),
                                        CY_SMIF_WIDTH_SINGLE,
                                        memCfg->slaveSelect,
                                        CY_SMIF_TX_LAST_BYTE,
                                        QSPI_context);
    }

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_MemIsReady(QSPIPort, memCfg, SEMPER_WR_NV_TIMEOUT,
                                    QSPI_context);
    }

    return status;
}

static cy_en_smif_status_t qspi_enter_4byte_addr_mode(void)
{
    cy_en_smif_status_t status;
    cy_stc_smif_mem_config_t *memCfg;
    SMIF_Type *QSPIPort;
    cy_stc_smif_context_t *QSPI_context;

    memCfg = qspi_get_memory_config(0);
    QSPIPort = qspi_get_device();
    QSPI_context = qspi_get_context();

    status = Cy_SMIF_TransmitCommand(QSPIPort, SEMPER_EN4BA_CMD,
                                    CY_SMIF_WIDTH_SINGLE,
                                    NULL,
                                    CY_SMIF_CMD_WITHOUT_PARAM,
                                    CY_SMIF_WIDTH_SINGLE,
                                    memCfg->slaveSelect,
                                    CY_SMIF_TX_LAST_BYTE,
                                    QSPI_context);

    return status;
}

static void value_to_byte_array(uint32_t value, uint8_t *byteArray, uint32_t startPos, uint32_t size)
{
    do
    {
        size--;
        byteArray[size + startPos] = (uint8_t)(value & PARAM_ID_LSB_MASK);
        value >>= PARAM_ID_MSB_OFFSET; /* Shift to get the next byte */
    } while (size > 0U);
}
