/***************************************************************************//**
* \file cybsp_dsram.c
*
* Description:
* Provides initialization code for handling deepsleep ram.
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
#include "cy_syspm.h"
#include "cy_sysclk.h"
#include "cybsp.h"
#if defined(CY_USING_HAL)
#include "cyhal_hwmgr.h"
#include "cyhal_syspm.h"
#endif
#include "cybsp_smif_init.h"
#if defined(CY_RTOS_AWARE) || defined(COMPONENT_RTOS_AWARE)
#include "cyabs_rtos_dsram.h"
#endif
#include "system_cat1b.h"
#include "cybsp_dsram.h"
#include "cmsis_compiler.h"

#if defined(__cplusplus)
extern "C" {
#endif



//--------------------------------------------------------------------------------------------------
// cybsp_syspm_do_warmboot
//--------------------------------------------------------------------------------------------------
__WEAK void cybsp_syspm_do_warmboot(void)
{
    #if defined(CY_RTOS_AWARE) || defined(COMPONENT_RTOS_AWARE)
    cyabs_rtos_exit_dsram();
    #endif
}


CY_SECTION_RAMFUNC_BEGIN
//--------------------------------------------------------------------------------------------------
// cybsp_warmboot_handler
//--------------------------------------------------------------------------------------------------
void cybsp_warmboot_handler(void)
{
    SystemInit_Warmboot_CAT1B_CM33();

    #if FLASH_BOOT
    //cybsp_smif_enable();
    //cybsp_smif_init();
    #endif

    init_cycfg_all();

    cybsp_syspm_do_warmboot();
}


CY_SECTION_RAMFUNC_END

/* DS-RAM Warmboot Re-entry*/
extern unsigned int __INITIAL_SP /*__StackTop*/;
cy_stc_syspm_warmboot_entrypoint_t syspmBspDeepSleepEntryPoint =
    { (uint32_t*)(&__INITIAL_SP), (uint32_t*)cybsp_warmboot_handler };

//--------------------------------------------------------------------------------------------------
// cybsp_syspm_dsram_init
//--------------------------------------------------------------------------------------------------
__WEAK cy_rslt_t cybsp_syspm_dsram_init(void)
{
/* Setup DS-RAM Warmboot Re-entry. IAR is not supported */
    #if defined(__ARMCC_VERSION) || defined (__GNUC__)
    Cy_Syslib_SetWarmBootEntryPoint((uint32_t*)&syspmBspDeepSleepEntryPoint, true);
    #endif

    return CY_RSLT_SUCCESS;
}


#if defined(__cplusplus)
}
#endif
