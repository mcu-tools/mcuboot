/***************************************************************************//**
* \file cybsp_pm_callbacks.c
*
* Description:
* Provides initialization code for starting up the hardware contained on the
* Infineon board.
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

#include <stdlib.h>
#include "cybsp_pm_callbacks.h"
#include "cycfg_qspi_memslot.h"
#include "cy_sysclk.h"
#include "cybsp_dsram.h"
#include "cybsp_smif_init.h"

// Must be defined in file for RAM functions to utilize during Deep Sleep callback to wake
// up external flash memory. Needs reference here when external flash is powered down in DS.
#if CY_PDL_FLASH_BOOT
cy_stc_smif_mem_config_t** smifConfigLocal = smifMemConfigs;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if (CY_PDL_FLASH_BOOT && (defined(CY_DEVICE_CYW20829) && \
    (CY_SYSLIB_GET_SILICON_REV_ID != CY_SYSLIB_20829A0_SILICON_REV)))
#define CY_EXT_MEM_POWER_DOWN_SUPPORTED
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// BSP PM callbacks order values //////////////////////////////////////

// CAT1B device QSPI memory power-down / power-up PM callback order.
#ifndef CYBSP_EXT_MEMORY_PM_CALLBACK_ORDER
    #define CYBSP_EXT_MEMORY_PM_CALLBACK_ORDER  (254u)
#endif
// The sysclk deep sleep callback is recommended to be the last callback that is executed before
// entry into deep sleep mode and the first one upon exit the deep sleep mode.
// Doing so minimizes the time spent on low power mode entry and exit.
#ifndef CYBSP_SYSCLK_PM_CALLBACK_ORDER
    #define CYBSP_SYSCLK_PM_CALLBACK_ORDER      (255u)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// BSP PM callbacks ///////////////////////////////////////////////////

// CAT1B device QSPI memory power-down / power-up PM callback.
// Intended to put external QSPI memory in low power state
// upon device transition to Deepsleep power mode and to wake up
// external QSPI device from low power mode upon device wakeup
#if defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
CY_RAMFUNC_BEGIN
cy_en_syspm_status_t cybsp_smif_power_up_callback(cy_stc_syspm_callback_params_t* callbackParams,
                                                  cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t retVal = CY_SYSPM_FAIL;
    CY_UNUSED_PARAMETER(callbackParams);
    extern cy_stc_smif_context_t cybsp_smif_context;

    switch (mode)
    {
        case CY_SYSPM_CHECK_READY:
            retVal = CY_SYSPM_SUCCESS;
            break;

        case CY_SYSPM_CHECK_FAIL:
            break;

        case CY_SYSPM_BEFORE_TRANSITION:
        {
            // Put external memory to low power mode
            // SMIF IOs aren't disabled here as the device enters into DSRAM then the IOs will be
            // disabled automatically
            retVal = (cy_en_syspm_status_t)Cy_SMIF_MemCmdPowerDown(SMIF0, smifConfigLocal[0],
                                                                   &cybsp_smif_context);
            cybsp_smif_disable();
            break;
        }

        case CY_SYSPM_AFTER_DS_WFI_TRANSITION:
        {
            cybsp_smif_enable();
            // Return external memory to normal operation from low power modes
            cy_en_smif_status_t smif_functions_status = Cy_SMIF_MemCmdReleasePowerDown(SMIF0,
                                                                                       smifConfigLocal[
                                                                                           0],
                                                                                       &cybsp_smif_context);

            if (CY_SMIF_SUCCESS == smif_functions_status)
            {
                smif_functions_status = cybsp_is_memory_ready(smifConfigLocal[0]);
                if (CY_SMIF_SUCCESS == smif_functions_status)
                {
                    retVal = CY_SYSPM_SUCCESS;
                }
            }
            break;
        }

        case CY_SYSPM_AFTER_TRANSITION:
        {
            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        default:
            break;
    }

    return retVal;
}


CY_RAMFUNC_END
#endif // defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)

// CAT1B device QSPI memory power-down / power-up PM callback.
// Intended to put external QSPI memory in low power state
// upon device transition to DeepsleepRam power mode and to wake up
// external QSPI device from low power mode upon device wakeup
#if defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
CY_RAMFUNC_BEGIN
cy_en_syspm_status_t cybsp_dsram_smif_power_up_callback(
    cy_stc_syspm_callback_params_t* callbackParams,
    cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t retVal = CY_SYSPM_FAIL;
    CY_UNUSED_PARAMETER(callbackParams);
    extern cy_stc_smif_context_t cybsp_smif_context;

    switch (mode)
    {
        case CY_SYSPM_CHECK_READY:
            retVal = CY_SYSPM_SUCCESS;
            break;

        case CY_SYSPM_CHECK_FAIL:
            break;

        case CY_SYSPM_BEFORE_TRANSITION:
        {
            // Put external memory to low power mode
            retVal = (cy_en_syspm_status_t)Cy_SMIF_MemCmdPowerDown(SMIF0, smifConfigLocal[0],
                                                                   &cybsp_smif_context);
            cybsp_smif_disable();
            break;
        }

        case CY_SYSPM_AFTER_DS_WFI_TRANSITION:
        {
            if (!Cy_SysLib_IsDSRAMWarmBootEntry())
            {
                cybsp_smif_enable();
                // Return external memory to normal operation from low power modes
                cy_en_smif_status_t smif_functions_status = Cy_SMIF_MemCmdReleasePowerDown(SMIF0,
                                                                                           smifConfigLocal[
                                                                                               0],
                                                                                           &cybsp_smif_context);

                if (CY_SMIF_SUCCESS == smif_functions_status)
                {
                    smif_functions_status = cybsp_is_memory_ready(smifConfigLocal[0]);
                    if (CY_SMIF_SUCCESS == smif_functions_status)
                    {
                        retVal = CY_SYSPM_SUCCESS;
                    }
                }
            }
            break;
        }

        case CY_SYSPM_AFTER_TRANSITION:
        {
            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        default:
            break;
    }

    return retVal;
}


CY_RAMFUNC_END
#endif // defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)

//--------------------------------------------------------------------------------------------------
// cybsp_deepsleep_ram_callback
//--------------------------------------------------------------------------------------------------
cy_en_syspm_status_t cybsp_deepsleep_ram_callback(cy_stc_syspm_callback_params_t* callbackParams,
                                                  cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t retVal = CY_SYSPM_FAIL;

    CY_UNUSED_PARAMETER(callbackParams);

    switch (mode)
    {
        case CY_SYSPM_CHECK_READY:
        case CY_SYSPM_CHECK_FAIL:
        case CY_SYSPM_BEFORE_TRANSITION:
        {
            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        case CY_SYSPM_AFTER_TRANSITION:
        {
            /* Currently GCC and ARMCC supported */
            #if defined(__GNUC__) || defined(__ARMCC_VERSION)
            Cy_Syslib_SetWarmBootEntryPoint((uint32_t*)&syspmBspDeepSleepEntryPoint, true);
            #endif

            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        default:
            break;
    }

    return retVal;
}


//--------------------------------------------------------------------------------------------------
// cybsp_hibernate_callback
//--------------------------------------------------------------------------------------------------
#if defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
CY_SECTION_RAMFUNC_BEGIN
cy_en_syspm_status_t cybsp_hibernate_callback(cy_stc_syspm_callback_params_t* callbackParams,
                                              cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t retVal = CY_SYSPM_FAIL;
    CY_UNUSED_PARAMETER(callbackParams);
    extern cy_stc_smif_context_t cybsp_smif_context;

    switch (mode)
    {
        case CY_SYSPM_CHECK_READY:
        {
            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        case CY_SYSPM_CHECK_FAIL:
        {
            retVal = CY_SYSPM_SUCCESS;
            break;
        }

        case CY_SYSPM_BEFORE_TRANSITION:
        {
            // Put external memory to low power mode
            retVal = (cy_en_syspm_status_t)Cy_SMIF_MemCmdPowerDown(SMIF0, smifConfigLocal[0],
                                                                   &cybsp_smif_context);
            break;
        }

        case CY_SYSPM_AFTER_TRANSITION:
        {
            // Return external memory to normal operation from low power modes
            cy_en_smif_status_t smif_functions_status = Cy_SMIF_MemCmdReleasePowerDown(SMIF0,
                                                                                       smifConfigLocal[
                                                                                           0],
                                                                                       &cybsp_smif_context);
            if (CY_SMIF_SUCCESS == smif_functions_status)
            {
                smif_functions_status = cybsp_is_memory_ready(smifConfigLocal[0]);
                if (CY_SMIF_SUCCESS == smif_functions_status)
                {
                    retVal = CY_SYSPM_SUCCESS;
                }
            }
            break;
        }

        default:
            break;
    }

    return retVal;
}


CY_SECTION_RAMFUNC_END
#endif // defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)

////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// BSP PM callbacks config structures ////////////////////////////////////

#if !defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK)
static cy_stc_syspm_callback_params_t cybsp_sysclk_pm_callback_param = { NULL, NULL };
static cy_stc_syspm_callback_t        cybsp_sysclk_pm_callback       =
{
    .callback       = &Cy_SysClk_DeepSleepCallback,
    #if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP_RAM)
    .type           = (cy_en_syspm_callback_type_t)CY_SYSPM_MODE_DEEPSLEEP_RAM,
    #else
    .type           = (cy_en_syspm_callback_type_t)CY_SYSPM_MODE_DEEPSLEEP,
    #endif
    .callbackParams = &cybsp_sysclk_pm_callback_param,
    .order          = CYBSP_SYSCLK_PM_CALLBACK_ORDER
};
#endif // if !defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK)

#if defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
static cy_stc_syspm_callback_params_t cybsp_smif_pu_callback_param = { NULL, NULL };
static cy_stc_syspm_callback_t        cybsp_smif_pu_callback       =
{
    .callback       = &cybsp_smif_power_up_callback,
    .type           = CY_SYSPM_DEEPSLEEP,
    .callbackParams = &cybsp_smif_pu_callback_param,
    .order          = CYBSP_EXT_MEMORY_PM_CALLBACK_ORDER
};

static cy_stc_syspm_callback_params_t cybsp_dsram_smif_pu_callback_param = { NULL, NULL };
static cy_stc_syspm_callback_t        cybsp_dsram_smif_pu_callback       =
{
    .callback       = &cybsp_dsram_smif_power_up_callback,
    .type           = CY_SYSPM_DEEPSLEEP_RAM,
    .callbackParams = &cybsp_dsram_smif_pu_callback_param,
    .order          = CYBSP_EXT_MEMORY_PM_CALLBACK_ORDER
};

static cy_stc_syspm_callback_params_t cybsp_hibernate_pm_callback_param = { NULL, NULL };
static cy_stc_syspm_callback_t        cybsp_hibernate_pm_callback       =
{
    .callback       = &cybsp_hibernate_callback,
    .type           = CY_SYSPM_HIBERNATE,
    .callbackParams = &cybsp_hibernate_pm_callback_param,
    .order          = CYBSP_EXT_MEMORY_PM_CALLBACK_ORDER
};
#endif // defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)

static cy_stc_syspm_callback_params_t cybsp_ds_ram_pm_callback_param = { NULL, NULL };
static cy_stc_syspm_callback_t        cybsp_ds_ram_pm_callback       =
{
    .callback       = &cybsp_deepsleep_ram_callback,
    .type           = CY_SYSPM_DEEPSLEEP_RAM,
    .callbackParams = &cybsp_ds_ram_pm_callback_param,
    .order          = 0u
};

////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// BSP PM callbacks array ////////////////////////////////////////////////

cy_stc_syspm_callback_t* _cybsp_callbacks_array[] =
{
    #if !defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK)
    &cybsp_sysclk_pm_callback,
    #endif // if !defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK)
    #if defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
    &cybsp_smif_pu_callback,
    &cybsp_dsram_smif_pu_callback,
    &cybsp_hibernate_pm_callback,
    #endif // defined(CY_EXT_MEM_POWER_DOWN_SUPPORTED)
    &cybsp_ds_ram_pm_callback
};

////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// BSP PM callbacks array helper function ////////////////////////////////

void _cybsp_pm_callbacks_get_ptr_and_number(cy_stc_syspm_callback_t*** arr_ptr,
                                            size_t* number_of_elements)
{
    *number_of_elements = 0;
    if (sizeof(_cybsp_callbacks_array) != 0)
    {
        *arr_ptr = _cybsp_callbacks_array;
        *number_of_elements = sizeof(_cybsp_callbacks_array) / sizeof(_cybsp_callbacks_array[0]);
    }
}


#if defined(__cplusplus)
}
#endif
