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
* \file flash_qspi.c
* \version 1.0
*
* \brief
*  This is the source file of external flash driver adaptation layer between pdl
*  and standard MCUBoot code.
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
#include <stdio.h>

#include "cy_sysclk.h"
#include "cy_gpio.h"
#include "cy_gpio.h"
#include "cy_smif.h"

#include "flash_qspi.h"
#include "cy_smif_hybrid_sect.h"
#include "flash_map_backend_platform.h"

#define CY_SMIF_SYSCLK_HFCLK_DIVIDER     CY_SYSCLK_CLKHF_DIVIDE_BY_2

#define CY_SMIF_INIT_TRY_COUNT           (10U)
#define CY_SMIF_INIT_TRY_DELAY           (500U)
#define CY_CHECK_MEMORY_AVAILABILITY_DELAY_US (1000U)

/* This is the board specific stuff that should align with your board.
 *
 * QSPI resources:
 *
 * SS0  - P11_2
 * SS1  - P11_1
 * SS2  - P11_0
 * SS3  - P12_4
 *
 * D3  - P11_3
 * D2  - P11_4
 * D1  - P11_5
 * D0  - P11_6
 *
 * SCK - P11_7
 *
 * SMIF Block - SMIF0
 *
 */

/* HSIOM values for SMIF pins */
#define CYBOOT_SMIF_SPI_CLOCK_HSIOM   (P2_5_SMIF_SPIHB_CLK)
#define CYBOOT_SMIF_SS0_HSIOM         (P2_0_SMIF_SPIHB_SELECT0)
#define CYBOOT_SMIF_SS1_HSIOM         (P0_5_SMIF_SPIHB_SELECT1)
#define CYBOOT_SMIF_DATA0_HSIOM       (P2_4_SMIF_SPIHB_DATA0)
#define CYBOOT_SMIF_DATA1_HSIOM       (P2_3_SMIF_SPIHB_DATA1)
#define CYBOOT_SMIF_DATA2_HSIOM       (P2_2_SMIF_SPIHB_DATA2)
#define CYBOOT_SMIF_DATA3_HSIOM       (P2_1_SMIF_SPIHB_DATA3)

/* SMIF ports and pins */
#define CYBOOT_SMIF_SPI_CLOCK_PORT    (GPIO_PRT2)
#define CYBOOT_SMIF_SPI_CLOCK_PIN     (5U)
#define CYBOOT_SMIF_SS0_PORT          (GPIO_PRT2)
#define CYBOOT_SMIF_SS0_PIN           (0U)
#define CYBOOT_SMIF_SS1_PORT          (GPIO_PRT0)
#define CYBOOT_SMIF_SS1_PIN           (5U)
#define CYBOOT_SMIF_DATA0_PORT        (GPIO_PRT2)
#define CYBOOT_SMIF_DATA0_PIN         (4U)
#define CYBOOT_SMIF_DATA1_PORT        (GPIO_PRT2)
#define CYBOOT_SMIF_DATA1_PIN         (3U)
#define CYBOOT_SMIF_DATA2_PORT        (GPIO_PRT2)
#define CYBOOT_SMIF_DATA2_PIN         (2U)
#define CYBOOT_SMIF_DATA3_PORT        (GPIO_PRT2)
#define CYBOOT_SMIF_DATA3_PIN         (1U)

/* SMIF SlaveSelect Configurations */
struct qspi_ss_config
{
    GPIO_PRT_Type* SS_Port;
    uint32_t SS_Pin;
    en_hsiom_sel_t SS_Mux;
};

static cy_stc_smif_context_t QSPI_context;

static cy_stc_smif_block_config_t *smif_blk_config;

static struct qspi_ss_config qspi_SS_Configuration[SMIF_CHIP_TOP_SPI_SEL_NR] =
{
    {
        .SS_Port = GPIO_PRT2,
        .SS_Pin = 0U,
        .SS_Mux = P2_0_SMIF_SPIHB_SELECT0
    },
    {
        .SS_Port = GPIO_PRT0,
        .SS_Pin = 5U,
        .SS_Mux = P0_5_SMIF_SPIHB_SELECT1
    }
};

static GPIO_PRT_Type *D3Port = GPIO_PRT2;
static uint32_t D3Pin = 1U;
static en_hsiom_sel_t D3MuxPort = P2_1_SMIF_SPIHB_DATA3;

static GPIO_PRT_Type *D2Port = GPIO_PRT2;
static uint32_t D2Pin = 2U;
static en_hsiom_sel_t D2MuxPort = P2_2_SMIF_SPIHB_DATA2;

static GPIO_PRT_Type *D1Port = GPIO_PRT2;
static uint32_t D1Pin = 3U;
static en_hsiom_sel_t D1MuxPort = P2_3_SMIF_SPIHB_DATA1;

static GPIO_PRT_Type *D0Port = GPIO_PRT2;
static uint32_t D0Pin = 4U;
static en_hsiom_sel_t D0MuxPort = P2_4_SMIF_SPIHB_DATA0;

static GPIO_PRT_Type *SCKPort = GPIO_PRT2;
static uint32_t SCKPin = 5U;
static en_hsiom_sel_t SCKMuxPort = P2_5_SMIF_SPIHB_CLK;

static GPIO_PRT_Type *SS_Port;
static uint32_t SS_Pin;
static en_hsiom_sel_t SS_MuxPort;

static SMIF_Type *QSPIPort  = SMIF0;

static cy_stc_smif_mem_cmd_t sfdpcmd =
{
    .command = 0x5A,
    .cmdWidth = CY_SMIF_WIDTH_SINGLE,
    .addrWidth = CY_SMIF_WIDTH_SINGLE,
    .mode = 0xFFFFFFFFU,
    .dummyCycles = 8,
    .dataWidth = CY_SMIF_WIDTH_SINGLE,
};

static cy_stc_smif_mem_cmd_t rdcmd0;
static cy_stc_smif_mem_cmd_t wrencmd0;
static cy_stc_smif_mem_cmd_t wrdiscmd0;
static cy_stc_smif_mem_cmd_t erasecmd0;
static cy_stc_smif_mem_cmd_t chiperasecmd0;
static cy_stc_smif_mem_cmd_t pgmcmd0;
static cy_stc_smif_mem_cmd_t readsts0;
static cy_stc_smif_mem_cmd_t readstsqecmd0;
static cy_stc_smif_mem_cmd_t writestseqcmd0;

static cy_stc_smif_mem_device_cfg_t dev_sfdp_0 =
{
    .numOfAddrBytes = 4,
    .readSfdpCmd = &sfdpcmd,
    .readCmd = &rdcmd0,
    .writeEnCmd = &wrencmd0,
    .writeDisCmd = &wrdiscmd0,
    .programCmd = &pgmcmd0,
    .eraseCmd = &erasecmd0,
    .chipEraseCmd = &chiperasecmd0,
    .readStsRegWipCmd = &readsts0,
    .readStsRegQeCmd = &readstsqecmd0,
    .writeStsRegQeCmd = &writestseqcmd0,
};

static cy_stc_smif_mem_config_t mem_sfdp_0 =
{
    /* The base address the memory slave is mapped to in the PSoC memory map.
    Valid when the memory-mapped mode is enabled. */
    .baseAddress = CY_XIP_BASE,
    /* The size allocated in the PSoC memory map, for the memory slave device.
    The size is allocated from the base address. Valid when the memory mapped mode is enabled. */
/*    .memMappedSize = 0x4000000U, */
    .flags = CY_SMIF_FLAG_DETECT_SFDP,
    .slaveSelect = CY_SMIF_SLAVE_SELECT_0,
    .dataSelect = CY_SMIF_DATA_SEL0,
    .deviceCfg = &dev_sfdp_0
};

static cy_stc_smif_mem_config_t *mems_sfdp[1] =
{
    &mem_sfdp_0
};

static cy_stc_smif_block_config_t smifBlockConfig_sfdp =
{
    .memCount = 1,
    .memConfig = mems_sfdp,
};

static cy_stc_smif_config_t const QSPI_config =
{
    .mode = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay = 1,

    .rxClockSel = (uint32_t)CY_SMIF_SEL_INVERTED_FEEDBACK_CLK,

    .blockEvent = (uint32_t)CY_SMIF_BUS_ERROR
};

/* SMIF pinouts configurations */
static cy_stc_gpio_pin_config_t QSPI_SS_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
    .hsiom = P2_0_SMIF_SPIHB_SELECT0, /* lets use SS0 by default */
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};
static const cy_stc_gpio_pin_config_t QSPI_DATA3_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG,
    .hsiom = P2_1_SMIF_SPIHB_DATA3,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};
static const cy_stc_gpio_pin_config_t QSPI_DATA2_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG,
    .hsiom = P2_2_SMIF_SPIHB_DATA2,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};
static const cy_stc_gpio_pin_config_t QSPI_DATA1_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG,
    .hsiom = P2_3_SMIF_SPIHB_DATA1,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};
static const cy_stc_gpio_pin_config_t QSPI_DATA0_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG,
    .hsiom = P2_4_SMIF_SPIHB_DATA0,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};
static const cy_stc_gpio_pin_config_t QSPI_SCK_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG,
    .hsiom = P2_5_SMIF_SPIHB_CLK,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_1_2,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};

static bool qspiInitiallyEnabled = false;
static int32_t qspiReservations = EXT_FLASH_DEV_FAILED;

static void qspi_get_initial_status(void)
{
    qspiInitiallyEnabled = (SMIF_CTL(QSPIPort) & SMIF_CTL_ENABLED_Msk) != 0u;
}

static bool qspi_is_configured(void)
{
    return qspiInitiallyEnabled;
}

cy_en_smif_status_t qspi_init_hardware(void)
{
    cy_en_smif_status_t st;

    qspi_get_initial_status();

    if (!qspi_is_configured()) {
        (void)Cy_GPIO_Pin_Init(D3Port, D3Pin, &QSPI_DATA3_config);
        Cy_GPIO_SetHSIOM(D3Port, D3Pin, D3MuxPort);

        (void)Cy_GPIO_Pin_Init(D2Port, D2Pin, &QSPI_DATA2_config);
        Cy_GPIO_SetHSIOM(D2Port, D2Pin, D2MuxPort);

        (void)Cy_GPIO_Pin_Init(D1Port, D1Pin, &QSPI_DATA1_config);
        Cy_GPIO_SetHSIOM(D1Port, D1Pin, D1MuxPort);

        (void)Cy_GPIO_Pin_Init(D0Port, D0Pin, &QSPI_DATA0_config);
        Cy_GPIO_SetHSIOM(D0Port, D0Pin, D0MuxPort);

        (void)Cy_GPIO_Pin_Init(SCKPort, SCKPin, &QSPI_SCK_config);
        Cy_GPIO_SetHSIOM(SCKPort, SCKPin, SCKMuxPort);

        (void)Cy_GPIO_Pin_Init(SS_Port, SS_Pin, &QSPI_SS_config);
        Cy_GPIO_SetHSIOM(SS_Port, SS_Pin, SS_MuxPort);

        (void)Cy_SysClk_ClkHfSetSource((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH2, CY_SYSCLK_CLKHF_IN_CLKPATH0);
        (void)Cy_SysClk_ClkHfSetDivider((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH2, CY_SMIF_SYSCLK_HFCLK_DIVIDER);
        (void)Cy_SysClk_ClkHfEnable((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH2);

        st = Cy_SMIF_Init(QSPIPort, &QSPI_config, 1000, &QSPI_context);
        if (st != CY_SMIF_SUCCESS) {
            return st;
        }
    }

    /* Set the polling delay in micro seconds to check memory device availability */
    Cy_SMIF_SetReadyPollingDelay(CY_CHECK_MEMORY_AVAILABILITY_DELAY_US, &QSPI_context);
    qspiReservations = EXT_FLASH_DEV_DISABLED;

    return CY_SMIF_SUCCESS;
}

void qspi_enable(void)
{
    if (EXT_FLASH_DEV_FAILED != qspiReservations) {
        if (EXT_FLASH_DEV_DISABLED == qspiReservations) {
            /*
             * Setup the interrupt for the SMIF block.  For the CM33 there
             * is a two stage process to setup the interrupts.
             */
            if (!qspi_is_configured()) {
                Cy_SMIF_Enable(QSPIPort, &QSPI_context);
            }
        }
        qspiReservations++;
    }
}

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_BEGIN /* SMIF is deinitialized inside! */
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
void qspi_disable(void)
{
    if (EXT_FLASH_DEV_FAILED != qspiReservations) {
        if (qspiReservations > EXT_FLASH_DEV_DISABLED) {
            qspiReservations--;
            if (EXT_FLASH_DEV_DISABLED == qspiReservations) {
                if (!qspi_is_configured()) {
                    Cy_SMIF_Disable(QSPIPort);
                }
            }
        }
    }
}
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_END /* SMIF will be deinitialized in this case! */
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */

cy_stc_smif_mem_config_t *qspi_get_memory_config(uint8_t index)
{
    return smif_blk_config->memConfig[index];
}

SMIF_Type *qspi_get_device(void)
{
    return QSPIPort;
}

cy_stc_smif_context_t *qspi_get_context(void)
{
    return &QSPI_context;
}

cy_en_smif_status_t qspi_init(cy_stc_smif_block_config_t *blk_config)
{
    cy_en_smif_status_t st;
    uint8_t devIdBuff[EXT_MEMORY_ID_LENGTH];

    st = qspi_init_hardware();
    if (st == CY_SMIF_SUCCESS) {
        qspi_enable();

        smif_blk_config = blk_config;
        st = Cy_SMIF_MemInit(QSPIPort, smif_blk_config, &QSPI_context);

        if(st == CY_SMIF_SUCCESS) {
            st = qspi_read_memory_id(devIdBuff, EXT_MEMORY_ID_LENGTH);
        }

        if(st == CY_SMIF_SUCCESS) {
            if(qspi_is_semper_flash(devIdBuff, EXT_MEMORY_ID_LENGTH) == true) {
                st = qspi_configure_semper_flash();
            }
        }

        qspi_disable();
    }

    if(st != CY_SMIF_SUCCESS) {
        qspiReservations = EXT_FLASH_DEV_FAILED;
    }
    return st;
}

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_BEGIN /* SMIF is deinitialized inside! */
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
void qspi_deinit(uint32_t smif_id)
{
    qspiReservations = EXT_FLASH_DEV_DISABLED + 1;

    qspi_disable();

    if (!qspi_is_configured()) {
        Cy_SMIF_MemDeInit(QSPIPort);

        (void)Cy_SysClk_ClkHfDisable((uint32_t)CY_SYSCLK_CLKHF_IN_CLKPATH1);

        Cy_GPIO_Port_Deinit(qspi_SS_Configuration[smif_id-1U].SS_Port);
        Cy_GPIO_Port_Deinit(SCKPort);
        Cy_GPIO_Port_Deinit(D0Port);
        Cy_GPIO_Port_Deinit(D1Port);
        Cy_GPIO_Port_Deinit(D2Port);
        Cy_GPIO_Port_Deinit(D3Port);
    }
}
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_END /* SMIF will be deinitialized in this case! */
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */

cy_en_smif_status_t qspi_init_sfdp(uint32_t smif_id)
{
    cy_en_smif_status_t stat = CY_SMIF_SUCCESS;

    cy_stc_smif_mem_config_t **memCfg = smifBlockConfig_sfdp.memConfig;


    switch(smif_id) {
    case 1:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_0;
        break;
    case 2:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_1;
        break;
    default:
        stat = CY_SMIF_BAD_PARAM;
        break;
    }

    if(CY_SMIF_SUCCESS == stat) {
        SS_Port = qspi_SS_Configuration[smif_id-1U].SS_Port;
        SS_Pin = qspi_SS_Configuration[smif_id-1U].SS_Pin;
        SS_MuxPort = qspi_SS_Configuration[smif_id-1U].SS_Mux;

        QSPI_SS_config.hsiom = SS_MuxPort;


        uint32_t try_count = CY_SMIF_INIT_TRY_COUNT;
        do {
            stat = qspi_init(&smifBlockConfig_sfdp);

            try_count--;
            if (stat != CY_SMIF_SUCCESS) {
                Cy_SysLib_Delay(CY_SMIF_INIT_TRY_DELAY);
            }
        } while ((stat != CY_SMIF_SUCCESS) && (try_count > 0U));
    }
    return stat;
}

uint8_t qspi_get_erased_val(void) 
{
    return EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM;
}

uint32_t qspi_get_prog_size(void)
{
    cy_stc_smif_mem_config_t **memCfg = smifBlockConfig_sfdp.memConfig;
    return (*memCfg)->deviceCfg->programSize;
}

uint32_t qspi_get_erase_size(void)
{
    cy_stc_smif_mem_config_t **memCfg = smifBlockConfig_sfdp.memConfig;
    return (*memCfg)->deviceCfg->eraseSize;
}

uint32_t qspi_get_mem_size(void)
{
    cy_stc_smif_mem_config_t **memCfg = smifBlockConfig_sfdp.memConfig;
    return (*memCfg)->deviceCfg->memSize;
}

int32_t qspi_get_status(void)
{
    return qspiReservations;
}
