/*******************************************************************************
* File Name: cycfg_routing.h
*
* Description:
* Establishes all necessary connections between hardware elements.
* This file was automatically generated and should not be modified.
* Device Configurator: 2.0.0.1483
* Device Support Library (../../../psoc6pdl): 1.4.0.1889
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

#if !defined(CYCFG_ROUTING_H)
#define CYCFG_ROUTING_H

#include "bsp.h"

#if defined(__cplusplus)
extern "C" {
#endif

void init_cycfg_routing(void);

#define init_cycfg_connectivity() init_cycfg_routing()

#define ioss_port_pin_rx_HSIOM  JOIN(JOIN(JOIN(JOIN(JOIN(JOIN(P, BSP_UART_PORT), _), BSP_UART_RX_PIN), _SCB), BSP_UART_SCB_NUMBER), _UART_RX)
#define ioss_port_pin_tx_HSIOM  JOIN(JOIN(JOIN(JOIN(JOIN(JOIN(P, BSP_UART_PORT), _), BSP_UART_TX_PIN), _SCB), BSP_UART_SCB_NUMBER), _UART_TX)

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_ROUTING_H */
