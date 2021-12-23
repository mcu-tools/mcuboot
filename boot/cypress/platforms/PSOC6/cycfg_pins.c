/*******************************************************************************
* File Name: cycfg_pins.c
*
* Description:
* Pin configuration
* This file was automatically generated and should not be modified.
* Device Configurator: 2.0.0.1483
* Device Support Library (../../../psoc6pdl): 1.3.1.1499
*
********************************************************************************
* Copyright 2017-2019 Cypress Semiconductor Corporation
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

const cy_stc_gpio_pin_config_t CYBSP_UART_RX_config =
{
	.outVal = 1,
	.driveMode = CY_GPIO_DM_HIGHZ,
	.hsiom = CYBSP_UART_RX_HSIOM,
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
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t CYBSP_UART_RX_obj =
	{
		.type = CYHAL_RSC_GPIO,
		.block_num = CYBSP_UART_RX_PORT_NUM,
		.channel_num = CYBSP_UART_RX_PIN,
	};
#endif //defined (CY_USING_HAL)
const cy_stc_gpio_pin_config_t CYBSP_UART_TX_config =
{
	.outVal = 1,
	.driveMode = CY_GPIO_DM_STRONG_IN_OFF,
	.hsiom = CYBSP_UART_TX_HSIOM,
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
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t CYBSP_UART_TX_obj =
	{
		.type = CYHAL_RSC_GPIO,
		.block_num = CYBSP_UART_TX_PORT_NUM,
		.channel_num = CYBSP_UART_TX_PIN,
	};
#endif //defined (CY_USING_HAL)

void init_cycfg_pins(void)
{
	Cy_GPIO_Pin_Init(CYBSP_UART_RX_PORT, CYBSP_UART_RX_PIN, &CYBSP_UART_RX_config);
#if defined (CY_USING_HAL)
	cyhal_hwmgr_reserve(&CYBSP_UART_RX_obj);
#endif //defined (CY_USING_HAL)

	Cy_GPIO_Pin_Init(CYBSP_UART_TX_PORT, CYBSP_UART_TX_PIN, &CYBSP_UART_TX_config);
#if defined (CY_USING_HAL)
	cyhal_hwmgr_reserve(&CYBSP_UART_TX_obj);
#endif //defined (CY_USING_HAL)
}
