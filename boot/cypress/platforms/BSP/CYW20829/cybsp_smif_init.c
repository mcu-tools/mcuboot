/***************************************************************************//**
* \file cybsp_smif_init.c
*
* Description:
* Provides initialization code for SMIF.
*
********************************************************************************
* \copyright
* Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
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
*******************************************************************************/
#if CY_PDL_FLASH_BOOT
#include "cybsp_smif_init.h"
#include "cycfg_pins.h"
#include "cyhal_pin_package.h"

cy_stc_smif_context_t cybsp_smif_context;

static uint32_t SMIF_PORT_SEL0;
static uint32_t SMIF_PORT_SEL1;
static uint32_t SMIF_CFG;
static uint32_t SMIF_OUT;

/*******************************************************************************
* Function Name: smif_disable
****************************************************************************//**
*
* it disable the the SMIF.
*
* \return NULL.
*
*******************************************************************************/
CY_RAMFUNC_BEGIN
void cybsp_smif_disable()
{
    // to minimize DeepSleep latency this code assumes that all of the SMIF pins are on the same
    // port
    int port_number= CYHAL_GET_PORT(CYBSP_QSPI_SS);
    SMIF0->CTL = SMIF0->CTL & ~SMIF_CTL_ENABLED_Msk;
    SMIF_PORT_SEL0 = ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL0;
    SMIF_PORT_SEL1 = ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL1;
    SMIF_CFG = ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->CFG;
    SMIF_OUT = ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->OUT;
    ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL0 = 0x00;
    ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL1 = 0x00;
    ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->CFG = 0x600006;
    ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->OUT = 0x1;
}


CY_RAMFUNC_END

/*******************************************************************************
* Function Name: smif_enable
****************************************************************************//**
*
* it enable the the SMIF.
*
* \return NULL.
*
*******************************************************************************/
CY_RAMFUNC_BEGIN
void cybsp_smif_enable()
{
    int port_number= CYHAL_GET_PORT(CYBSP_QSPI_SS);
    SMIF0->CTL = SMIF0->CTL | SMIF_CTL_ENABLED_Msk;
    ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL0 = SMIF_PORT_SEL0;
    ((HSIOM_PRT_Type*)&HSIOM->PRT[port_number])->PORT_SEL1 = SMIF_PORT_SEL1;
    ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->CFG = SMIF_CFG;
    ((GPIO_PRT_Type*)&GPIO->PRT[port_number])->OUT = SMIF_OUT;
}


CY_RAMFUNC_END

/*******************************************************************************
* Function Name: cybsp_is_memory_ready
****************************************************************************//**
*
* Polls the memory device to check whether it is ready to accept new commands or
* not until either it is ready or the retries have exceeded the limit.
*
* \param memConfig
* memory device configuration
*
* \return Status of the operation.
* CY_SMIF_SUCCESS          - Memory is ready to accept new commands.
* CY_SMIF_EXCEED_TIMEOUT - Memory is busy.
*
*******************************************************************************/
#if defined (__ICCARM__)
/* Suppress warnings originating from configurator */
/* Ta022 : "Call to a non __ramfunc function from within a __ramfunc function"  */
CY_PRAGMA(diag_suppress = Ta022)
#endif
//--------------------------------------------------------------------------------------------------
// cybsp_is_memory_ready
//--------------------------------------------------------------------------------------------------
CY_RAMFUNC_BEGIN

cy_en_smif_status_t cybsp_is_memory_ready(cy_stc_smif_mem_config_t const* memConfig)
{
    uint32_t retries = 0;
    bool isBusy;

    do
    {
        isBusy =
            Cy_SMIF_Memslot_IsBusy(SMIF_HW, (cy_stc_smif_mem_config_t*)memConfig,
                                   &cybsp_smif_context);
        Cy_SysLib_DelayUs(15);
        retries++;
    } while(isBusy && (retries < MEMORY_BUSY_CHECK_RETRIES));

    return (isBusy ? CY_SMIF_EXCEED_TIMEOUT : CY_SMIF_SUCCESS);
}


CY_RAMFUNC_END
#if defined (__ICCARM__)
CY_PRAGMA(diag_default = Ta022)
#endif

/*******************************************************************************
* Function Name: cybsp_enable_quad_mode
****************************************************************************//*
*
* This function sets the QE (QUAD Enable) bit in the external memory
* configuration register to enable Quad SPI mode.
*
* \param memConfig
* Memory device configuration
*
* \return Status of the operation. See cy_en_smif_status_t.
*
*******************************************************************************/
#if defined (__ICCARM__)
/* Suppress warnings originating from configurator */
/* Ta022 : "Call to a non __ramfunc function from within a __ramfunc function"  */
CY_PRAGMA(diag_suppress = Ta022)
#endif
//--------------------------------------------------------------------------------------------------
// cybsp_enable_quad_mode
//--------------------------------------------------------------------------------------------------
CY_RAMFUNC_BEGIN

cy_en_smif_status_t cybsp_enable_quad_mode(cy_stc_smif_mem_config_t const* memConfig)
{
    cy_en_smif_status_t status;

    /* Send Write Enable to external memory */
    status = Cy_SMIF_Memslot_CmdWriteEnable(SMIF_HW, smifMemConfigs[0], &cybsp_smif_context);

    if (CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_Memslot_QuadEnable(SMIF_HW, (cy_stc_smif_mem_config_t*)memConfig,
                                            &cybsp_smif_context);

        if (CY_SMIF_SUCCESS == status)
        {
            /* Poll memory for the completion of operation */
            status = cybsp_is_memory_ready(memConfig);
        }
    }

    return status;
}


CY_RAMFUNC_END
#if defined (__ICCARM__)
CY_PRAGMA(diag_default = Ta022)
#endif


/*******************************************************************************
* Function Name: cybsp_is_quad_enabled
****************************************************************************//**
*
* Checks whether QE (Quad Enable) bit is set or not in the configuration
* register of the memory.
*
* \param memConfig
* Memory device configuration
*
* \param isQuadEnabled
* This parameter is updated to indicate whether Quad mode is enabled (true) or
* not (false). The value is valid only when the function returns
* CY_SMIF_SUCCESS.
*
* \return Status of the operation. See cy_en_smif_status_t.
*
*******************************************************************************/
#if defined (__ICCARM__)
/* Suppress warnings originating from configurator */
/* Ta022 : "Call to a non __ramfunc function from within a __ramfunc function"  */
CY_PRAGMA(diag_suppress = Ta022)
#endif
//--------------------------------------------------------------------------------------------------
// cybsp_is_quad_enabled
//--------------------------------------------------------------------------------------------------
CY_RAMFUNC_BEGIN

cy_en_smif_status_t cybsp_is_quad_enabled(cy_stc_smif_mem_config_t const* memConfig,
                                          bool* isQuadEnabled)
{
    cy_en_smif_status_t status;
    uint8_t readStatus = 0;
    uint32_t statusCmd = memConfig->deviceCfg->readStsRegQeCmd->command;
    uint8_t maskQE = (uint8_t)memConfig->deviceCfg->stsRegQuadEnableMask;

    status = Cy_SMIF_Memslot_CmdReadSts(SMIF_HW, memConfig, &readStatus, statusCmd,
                                        &cybsp_smif_context);

    *isQuadEnabled = false;
    if (CY_SMIF_SUCCESS == status)
    {
        /* Check whether Quad mode is already enabled or not */
        *isQuadEnabled = (maskQE == (readStatus & maskQE));
    }

    return status;
}


CY_RAMFUNC_END
#if defined (__ICCARM__)
CY_PRAGMA(diag_default = Ta022)
#endif


cy_stc_smif_config_t cybsp_smif_config =
{
    .mode          = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay = 7,
    .rxClockSel    = (uint32_t)CY_SMIF_SEL_INVERTED_FEEDBACK_CLK,
    .blockEvent    = (uint32_t)CY_SMIF_BUS_ERROR,
};

/********************************************************
* cybsp_smif_start
*********************************************************
* Initializes the SMIF hardware, sets the slave select
* and enables the SMIF block.
*
* returns: the status of the block during initialization
*
********************************************************/
#if defined (__ICCARM__)
/* Suppress warnings originating from configurator */
/* Ta022 : "Call to a non __ramfunc function from within a __ramfunc function"  */
CY_PRAGMA(diag_suppress = Ta022)
#endif
//--------------------------------------------------------------------------------------------------
// cybsp_smif_start
//--------------------------------------------------------------------------------------------------
CY_RAMFUNC_BEGIN

cy_en_smif_status_t cybsp_smif_start(void)
{
    cy_en_smif_status_t cybsp_smif_status = CY_SMIF_BAD_PARAM;

    cybsp_smif_status =
        Cy_SMIF_Init(SMIF_HW, &cybsp_smif_config, TIMEOUT_1_MS, &cybsp_smif_context);

    if (cybsp_smif_status == CY_SMIF_SUCCESS)
    {
        Cy_SMIF_SetDataSelect(SMIF_HW, smifMemConfigs[0]->slaveSelect,
                              smifMemConfigs[0]->dataSelect);
        Cy_SMIF_Enable(SMIF_HW, &cybsp_smif_context);
    }

    return cybsp_smif_status;
}


CY_RAMFUNC_END
#if defined (__ICCARM__)
CY_PRAGMA(diag_default = Ta022)
#endif


/********************************************************
* cybsp_smif_init
*********************************************************
* Configures the SMIF hardware
********************************************************/
#if defined (__ICCARM__)
/* Suppress warnings originating from configurator */
/* Ta022 : "Call to a non __ramfunc function from within a __ramfunc function"  */
CY_PRAGMA(diag_suppress = Ta022)
#endif
//--------------------------------------------------------------------------------------------------
// cybsp_smif_init
//--------------------------------------------------------------------------------------------------
CY_RAMFUNC_BEGIN

cy_en_smif_status_t cybsp_smif_init(void)
{
    /* Initalization Status Holders */
    cy_en_smif_status_t cybsp_smif_status = CY_SMIF_BAD_PARAM;
    bool QE_status = false;

    cybsp_smif_status = cybsp_smif_start();

    if (cybsp_smif_status == CY_SMIF_SUCCESS)
    {
        cybsp_smif_status = Cy_SMIF_MemCmdReleasePowerDown(SMIF0,
                                                           smifMemConfigs[0],
                                                           &cybsp_smif_context);

        if (CY_SMIF_SUCCESS == cybsp_smif_status)
        {
            cybsp_smif_status = cybsp_is_memory_ready(smifMemConfigs[0]);
            if (CY_SMIF_SUCCESS == cybsp_smif_status)
            {
                /* Map memory device to memory map */
                cybsp_smif_status = Cy_SMIF_Memslot_Init(SMIF_HW,
                                                         (cy_stc_smif_block_config_t*)&smifBlockConfig,
                                                         &cybsp_smif_context);
                if (cybsp_smif_status == CY_SMIF_SUCCESS)
                {
                    /* Even after SFDP enumeration QE command is not initialised */
                    /* So, it should be 1.0 device */
                    if ((smifMemConfigs[0]->deviceCfg->readStsRegQeCmd->command ==
                         CY_SMIF_NO_COMMAND_OR_MODE) ||
                        (smifMemConfigs[0]->deviceCfg->readStsRegQeCmd->command == 0))
                    {
                        cybsp_smif_status = Cy_SMIF_MemInitSfdpMode(SMIF_HW,
                                                                    smifMemConfigs[0],
                                                                    CY_SMIF_WIDTH_QUAD,
                                                                    CY_SMIF_SFDP_QER_1,
                                                                    &cybsp_smif_context);
                    }

                    if (cybsp_smif_status == CY_SMIF_SUCCESS)
                    {
                        /* Check if Quad mode is already enabled */
                        cybsp_is_quad_enabled(smifMemConfigs[0], &QE_status);

                        /* If not enabled, enable quad mode */
                        if (!QE_status)
                        {
                            /* Enable Quad mode */
                            cybsp_smif_status = cybsp_enable_quad_mode(smifMemConfigs[0]);
                        }
                        if (cybsp_smif_status == CY_SMIF_SUCCESS)
                        {
                            /* Put the device in XIP mode */
                            Cy_SMIF_SetMode(SMIF_HW, CY_SMIF_MEMORY);
                        }
                    }
                }
            }
        }
    }

    return cybsp_smif_status;
}


CY_RAMFUNC_END
#if defined (__ICCARM__)
CY_PRAGMA(diag_default = Ta022)
#endif

#endif // if CY_PDL_FLASH_BOOT
