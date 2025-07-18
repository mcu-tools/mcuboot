/*******************************************************************************
* File Name: cycfg_pins.c
*
* Description:
* Pin configuration
* This file was automatically generated and should not be modified.
* Configurator Backend 3.10.0
* device-db 4.100.0.4783
* mtb-pdl-cat1 3.9.0.29592
*
********************************************************************************
* Copyright 2025 Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cycfg_pins.h"

const cy_stc_gpio_pin_config_t CYBSP_BT_UART_CTS_config = 
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_ANALOG,
    .hsiom = CYBSP_BT_UART_CTS_HSIOM,
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
    .nonSec = 1UL,
};
#if defined (CY_USING_HAL)
    const cyhal_resource_inst_t CYBSP_BT_UART_CTS_obj = 
    {
        .type = CYHAL_RSC_GPIO,
        .block_num = CYBSP_BT_UART_CTS_PORT_NUM,
        .channel_num = CYBSP_BT_UART_CTS_PIN,
    };
#endif //defined (CY_USING_HAL)
const cy_stc_gpio_pin_config_t CYBSP_A1_config = 
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_ANALOG,
    .hsiom = CYBSP_A1_HSIOM,
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
    .nonSec = 1UL,
};
#if defined (CY_USING_HAL)
    const cyhal_resource_inst_t CYBSP_A1_obj = 
    {
        .type = CYHAL_RSC_GPIO,
        .block_num = CYBSP_A1_PORT_NUM,
        .channel_num = CYBSP_A1_PIN,
    };
#endif //defined (CY_USING_HAL)


void init_cycfg_pins(void)
{
    Cy_GPIO_Pin_Init(CYBSP_BT_UART_CTS_PORT, CYBSP_BT_UART_CTS_PIN, &CYBSP_BT_UART_CTS_config);
    Cy_GPIO_Pin_Init(CYBSP_A1_PORT, CYBSP_A1_PIN, &CYBSP_A1_config);
}

void reserve_cycfg_pins(void)
{
#if defined (CY_USING_HAL)
    cyhal_hwmgr_reserve(&CYBSP_BT_UART_CTS_obj);
    cyhal_hwmgr_reserve(&CYBSP_A1_obj);
#endif //defined (CY_USING_HAL)
}
