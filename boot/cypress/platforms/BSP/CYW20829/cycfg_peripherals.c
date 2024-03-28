/*******************************************************************************
* File Name: cycfg_peripherals.c
*
* Description:
* Peripheral Hardware Block configuration
* This file was automatically generated and should not be modified.
* Configurator Backend 3.10.0
* device-db 4.100.0.4783
* mtb-pdl-cat1 3.9.0.29592
*
********************************************************************************
* Copyright 2024 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
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

#include "cycfg_peripherals.h"

cy_stc_adcmic_context_t adcmic_0_context;
const cy_stc_adcmic_dc_config_t adcmic_0_dc_config = 
{
    .range = CY_ADCMIC_DC_RANGE_3_6V,
    .channel = CY_ADCMIC_GPIO5,
    .timerPeriod = 10000,
    .timerInput = CY_ADCMIC_TIMER_COUNT_INPUT_CIC_UPDATE,
    .context = &adcmic_0_context,
};
const cy_stc_adcmic_config_t adcmic_0_config = 
{
    .micConfig = NULL,
    .pdmConfig = NULL,
    .dcConfig = (cy_stc_adcmic_dc_config_t*)&adcmic_0_dc_config,
};
#if defined (CY_USING_HAL)
    const cyhal_resource_inst_t adcmic_0_obj = 
    {
        .type = CYHAL_RSC_ADC,
        .block_num = 0,
        .channel_num = 0,
    };
#endif //defined (CY_USING_HAL)


void reserve_cycfg_peripherals(void)
{
#if defined (CY_USING_HAL)
    cyhal_hwmgr_reserve(&adcmic_0_obj);
#endif //defined (CY_USING_HAL)
}
