/*******************************************************************************
* File Name: cycfg_peripherals.h
*
* Description:
* Peripheral Hardware Block configuration
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

#if !defined(CYCFG_PERIPHERALS_H)
#define CYCFG_PERIPHERALS_H

// #include "cycfg_notices.h"
#include "cy_scb_uart.h"
#include "cy_sysclk.h"
#if defined (CY_USING_HAL)
    #include "cyhal_hwmgr.h"
#endif //defined (CY_USING_HAL)

#include "bsp.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CYBSP_UART_ENABLED  1U
#define CYBSP_UART_HW       BSP_UART_HW
#define CYBSP_UART_IRQ      JOIN(JOIN(scb_, USE_UART_SCB_NUMBER), _interrupt_IRQn)
#define PCLK_SCBx_CLOCK     JOIN(JOIN(PCLK_SCB, BSP_UART_SCB_NUMBER), _CLOCK)

extern const cy_stc_scb_uart_config_t CYBSP_UART_config;
#if defined (CY_USING_HAL)
    extern const cyhal_resource_inst_t CYBSP_UART_obj;
#endif //defined (CY_USING_HAL)

void init_cycfg_peripherals(void);

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_PERIPHERALS_H */
