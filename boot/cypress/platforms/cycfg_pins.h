/*******************************************************************************
* File Name: cycfg_pins.h
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

#if !defined(CYCFG_PINS_H)
#define CYCFG_PINS_H

#include "cy_gpio.h"
#if defined (CY_USING_HAL)
	#include "cyhal_hwmgr.h"
#endif //defined (CY_USING_HAL)
#include "cycfg_routing.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CYBSP_UART_RX_ENABLED 1U
#define CYBSP_UART_RX_PORT GPIO_PRT5
#define CYBSP_UART_RX_PORT_NUM 5U
#define CYBSP_UART_RX_PIN 0U
#define CYBSP_UART_RX_NUM 0U
#define CYBSP_UART_RX_DRIVEMODE CY_GPIO_DM_HIGHZ
#define CYBSP_UART_RX_INIT_DRIVESTATE 1
#ifndef ioss_0_port_5_pin_0_HSIOM
	#define ioss_0_port_5_pin_0_HSIOM HSIOM_SEL_GPIO
#endif
#define CYBSP_UART_RX_HSIOM ioss_0_port_5_pin_0_HSIOM
#define CYBSP_UART_RX_IRQ ioss_interrupts_gpio_5_IRQn
#if defined (CY_USING_HAL)
	#define CYBSP_UART_RX_HAL_PORT_PIN P5_0
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_RX_HAL_IRQ CYHAL_GPIO_IRQ_NONE
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_RX_HAL_DIR CYHAL_GPIO_DIR_INPUT
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_RX_HAL_DRIVEMODE CYHAL_GPIO_DRIVE_NONE
#endif //defined (CY_USING_HAL)
#define CYBSP_UART_TX_ENABLED 1U
#define CYBSP_UART_TX_PORT GPIO_PRT5
#define CYBSP_UART_TX_PORT_NUM 5U
#define CYBSP_UART_TX_PIN 1U
#define CYBSP_UART_TX_NUM 1U
#define CYBSP_UART_TX_DRIVEMODE CY_GPIO_DM_STRONG_IN_OFF
#define CYBSP_UART_TX_INIT_DRIVESTATE 1
#ifndef ioss_0_port_5_pin_1_HSIOM
	#define ioss_0_port_5_pin_1_HSIOM HSIOM_SEL_GPIO
#endif
#define CYBSP_UART_TX_HSIOM ioss_0_port_5_pin_1_HSIOM
#define CYBSP_UART_TX_IRQ ioss_interrupts_gpio_5_IRQn
#if defined (CY_USING_HAL)
	#define CYBSP_UART_TX_HAL_PORT_PIN P5_1
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_TX_HAL_IRQ CYHAL_GPIO_IRQ_NONE
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_TX_HAL_DIR CYHAL_GPIO_DIR_OUTPUT
#endif //defined (CY_USING_HAL)
#if defined (CY_USING_HAL)
	#define CYBSP_UART_TX_HAL_DRIVEMODE CYHAL_GPIO_DRIVE_STRONG
#endif //defined (CY_USING_HAL)

extern const cy_stc_gpio_pin_config_t CYBSP_WCO_IN_config;
#if defined (CY_USING_HAL)
	extern const cyhal_resource_inst_t CYBSP_WCO_IN_obj;
#endif //defined (CY_USING_HAL)
extern const cy_stc_gpio_pin_config_t CYBSP_WCO_OUT_config;
#if defined (CY_USING_HAL)
	extern const cyhal_resource_inst_t CYBSP_WCO_OUT_obj;
#endif //defined (CY_USING_HAL)
extern const cy_stc_gpio_pin_config_t CYBSP_UART_RX_config;
#if defined (CY_USING_HAL)
	extern const cyhal_resource_inst_t CYBSP_UART_RX_obj;
#endif //defined (CY_USING_HAL)
extern const cy_stc_gpio_pin_config_t CYBSP_UART_TX_config;
#if defined (CY_USING_HAL)
	extern const cyhal_resource_inst_t CYBSP_UART_TX_obj;
#endif //defined (CY_USING_HAL)

void init_cycfg_pins(void);

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_PINS_H */
