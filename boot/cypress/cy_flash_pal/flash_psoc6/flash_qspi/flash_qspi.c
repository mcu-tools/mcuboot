/*
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
/***************************************************************************//**
* \file flash_qspi.c
* \version 1.0
*
* \brief
*  This is the source file of external flash driver adaptation layer between PSoC6
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
#include "cy_pdl.h"
#include <stdio.h>
#include "flash_qspi.h"

#define CY_SMIF_SYSCLK_HFCLK_DIVIDER     CY_SYSCLK_CLKHF_DIVIDE_BY_4

#define CY_SMIF_INIT_TRY_COUNT           (10U)

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

/* SMIF SlaveSelect Configurations */
struct qspi_ss_config
{
    GPIO_PRT_Type* SS_Port;
    uint32_t SS_Pin;
    en_hsiom_sel_t SS_Mux;
};

#if (defined(PSOC_064_2M) || \
    defined(PSOC_064_1M) || \
    defined(PSOC_062_2M) || \
    defined(PSOC_062_1M))
    #define CY_BOOTLOADER_SMIF_SS_CFG_NUM 4
#elif defined(PSOC_064_512K) || defined(PSOC_062_512K) || defined(CYW20829)
    #define CY_BOOTLOADER_SMIF_SS_CFG_NUM 3
#else
#error "Platform device name is unsupported."
#endif
static struct qspi_ss_config qspi_SS_Configuration[CY_BOOTLOADER_SMIF_SS_CFG_NUM] =
{
    {
        .SS_Port = GPIO_PRT11,
        .SS_Pin = 2u,
        .SS_Mux = P11_2_SMIF_SPI_SELECT0
    },
    {
        .SS_Port = GPIO_PRT11,
        .SS_Pin = 1u,
        .SS_Mux = P11_1_SMIF_SPI_SELECT1
    },
    {
        .SS_Port = GPIO_PRT11,
        .SS_Pin = 0u,
        .SS_Mux = P11_0_SMIF_SPI_SELECT2
    },
#if(CY_BOOTLOADER_SMIF_SS_CFG_NUM > 3)
    {
        .SS_Port = GPIO_PRT12,
        .SS_Pin = 4u,
        .SS_Mux = P12_4_SMIF_SPI_SELECT3
    }
#endif
};

static GPIO_PRT_Type *D3Port = GPIO_PRT11;
static uint32_t D3Pin = 3u;
static en_hsiom_sel_t D3MuxPort = P11_3_SMIF_SPI_DATA3;

static GPIO_PRT_Type *D2Port = GPIO_PRT11;
static uint32_t D2Pin = 4u;
static en_hsiom_sel_t D2MuxPort = P11_4_SMIF_SPI_DATA2;

static GPIO_PRT_Type *D1Port = GPIO_PRT11;
static uint32_t D1Pin = 5u;
static en_hsiom_sel_t D1MuxPort = P11_5_SMIF_SPI_DATA1;

static GPIO_PRT_Type *D0Port = GPIO_PRT11;
static uint32_t D0Pin = 6u;
static en_hsiom_sel_t D0MuxPort = P11_6_SMIF_SPI_DATA0;

static GPIO_PRT_Type *SCKPort = GPIO_PRT11;
static uint32_t SCKPin = 7u;
static en_hsiom_sel_t SCKMuxPort = P11_7_SMIF_SPI_CLK;

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
    .memMappedSize = CY_XIP_SIZE,
    .flags = CY_SMIF_FLAG_DETECT_SFDP | CY_SMIF_FLAG_MEMORY_MAPPED,
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

static cy_stc_smif_block_config_t *smif_blk_config;

static cy_stc_smif_context_t QSPI_context;

static cy_stc_smif_config_t const QSPI_config =
{
    .mode = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay = 1,
    .rxClockSel = (uint32_t)CY_SMIF_SEL_INV_INTERNAL_CLK,
    .blockEvent = (uint32_t)CY_SMIF_BUS_ERROR
};

#ifdef CM0P
static cy_stc_sysint_t smifIntConfig =
{/* ATTENTION: make sure proper Interrupts configured for CM0p or M4 cores */
    .intrSrc = NvicMux7_IRQn,
    .cm0pSrc = smif_interrupt_IRQn,
    .intrPriority = 1
};
#endif

/* SMIF pinouts configurations */
static cy_stc_gpio_pin_config_t QSPI_SS_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
    .hsiom = P11_2_SMIF_SPI_SELECT0, /* lets use SS0 by default */
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
    .hsiom = P11_3_SMIF_SPI_DATA3,
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
    .hsiom = P11_4_SMIF_SPI_DATA2,
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
    .hsiom = P11_5_SMIF_SPI_DATA1,
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
    .hsiom = P11_6_SMIF_SPI_DATA0,
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
    .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
    .hsiom = P11_7_SMIF_SPI_CLK,
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

#ifdef CM0P
static void Isr_SMIF(void)
{
    Cy_SMIF_Interrupt(QSPIPort, &QSPI_context);
}
#endif

cy_en_smif_status_t qspi_init_hardware(void)
{
    cy_en_smif_status_t st;


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

    (void)Cy_SysClk_ClkHfSetSource(CY_SYSCLK_CLKHF_IN_CLKPATH2, CY_SYSCLK_CLKHF_IN_CLKPATH0);
    (void)Cy_SysClk_ClkHfSetDivider(CY_SYSCLK_CLKHF_IN_CLKPATH2, CY_SMIF_SYSCLK_HFCLK_DIVIDER);
    (void)Cy_SysClk_ClkHfEnable(CY_SYSCLK_CLKHF_IN_CLKPATH2);

    /*
     * Setup the interrupt for the SMIF block.  For the CM0 there
     * is a two stage process to setup the interrupts.
     */
#ifdef CM0P
    (void)Cy_SysInt_Init(&smifIntConfig, Isr_SMIF);
#endif

    st = Cy_SMIF_Init(QSPIPort, &QSPI_config, 1000, &QSPI_context);
    if (st != CY_SMIF_SUCCESS)
    {
        return st;
    }

#ifdef CM0P
    NVIC_EnableIRQ(smifIntConfig.intrSrc); /* Finally, Enable the SMIF interrupt */
#endif

    Cy_SMIF_Enable(QSPIPort, &QSPI_context);

    return CY_SMIF_SUCCESS;
}

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

    st = qspi_init_hardware();
    if (st == CY_SMIF_SUCCESS)
    {
        smif_blk_config = blk_config;
        st = Cy_SMIF_MemInit(QSPIPort, smif_blk_config, &QSPI_context);
    }
    return st;
}

cy_en_smif_status_t qspi_init_sfdp(uint32_t smif_id)
{
    cy_en_smif_status_t stat = CY_SMIF_SUCCESS;

    cy_stc_smif_mem_config_t **memCfg = smifBlockConfig_sfdp.memConfig;

    GPIO_PRT_Type *SS_Port;
    uint32_t SS_Pin;
    en_hsiom_sel_t SS_MuxPort;

    switch(smif_id)
    {
    case 1:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_0;
        break;
    case 2:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_1;
        break;
    case 3:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_2;
        break;
#if(CY_BOOTLOADER_SMIF_SS_CFG_NUM > 3)
    case 4:
        (*memCfg)->slaveSelect = CY_SMIF_SLAVE_SELECT_3;
        break;
#endif
    default:
        stat = -1;
        break;
    }

    if(CY_SMIF_SUCCESS == stat)
    {
        SS_Port = qspi_SS_Configuration[smif_id-1U].SS_Port;
        SS_Pin = qspi_SS_Configuration[smif_id-1U].SS_Pin;
        SS_MuxPort = qspi_SS_Configuration[smif_id-1U].SS_Mux;

        QSPI_SS_config.hsiom = SS_MuxPort;

        (void)Cy_GPIO_Pin_Init(SS_Port, SS_Pin, &QSPI_SS_config);
        Cy_GPIO_SetHSIOM(SS_Port, SS_Pin, SS_MuxPort);

        uint32_t try_count = CY_SMIF_INIT_TRY_COUNT;
        do {
            stat = qspi_init(&smifBlockConfig_sfdp);

            try_count--;
            if (stat != CY_SMIF_SUCCESS)
            {
                Cy_SysLib_Delay(500U);
            }
        } while ((stat != CY_SMIF_SUCCESS) && (try_count > 0U));
    }
    return stat;
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

void qspi_deinit(uint32_t smif_id)
{
    Cy_SMIF_MemDeInit(QSPIPort);

    Cy_SMIF_Disable(QSPIPort);

    (void)Cy_SysClk_ClkHfDisable(CY_SYSCLK_CLKHF_IN_CLKPATH2);

#ifdef CM0P
    NVIC_DisableIRQ(smifIntConfig.intrSrc);
    Cy_SysInt_DisconnectInterruptSource(smifIntConfig.intrSrc, smifIntConfig.cm0pSrc);
#endif

    Cy_GPIO_Port_Deinit(qspi_SS_Configuration[smif_id-1U].SS_Port);
    Cy_GPIO_Port_Deinit(SCKPort);
    Cy_GPIO_Port_Deinit(D0Port);
    Cy_GPIO_Port_Deinit(D1Port);
    Cy_GPIO_Port_Deinit(D2Port);
    Cy_GPIO_Port_Deinit(D3Port);
}

void qspi_set_mode(cy_en_smif_mode_t mode)
{
    SMIF_Type* smif_mem = qspi_get_device();

    Cy_SMIF_SetMode(smif_mem, mode);
}

cy_en_smif_mode_t qspi_get_mode(void)
{
    SMIF_Type* smif_mem = qspi_get_device();

    return Cy_SMIF_GetMode(smif_mem);
}
