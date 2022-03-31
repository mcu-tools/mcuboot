/*
********************************************************************************
* Copyright 2021 Infineon Technologies AG
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

#if !defined(BSP_H)
#define BSP_H

#include "custom_debug_uart_cfg.h"

/*https://wiki.sei.cmu.edu/confluence/display/c/PRE05-C.+Understand+macro+replacement+when+concatenating+tokens+or+performing+stringification*/
#define JOIN(x, y) JOIN_AGAIN(x, y)
#define JOIN_AGAIN(x, y) x ## y

#if defined(__cplusplus)
extern "C" {
#endif

/* If this flash is set use user defined HW */
#if defined(USE_CUSTOM_DEBUG_UART)

#ifndef CUSTOM_UART_HW
#error "Option USE_CUSTOM_DEBUG_UART turned on, but CUSTOM_UART_HW is undefined \r\n";
#else
#define BSP_UART_HW     CUSTOM_UART_HW
#endif /* CUSTOM_UART_HW */

#ifndef CUSTOM_UART_SCB_NUMBER
#error "Option USE_CUSTOM_DEBUG_UART turned on, but CUSTOM_UART_SCB_NUMBER is undefined \r\n";
#else
#define BSP_UART_SCB_NUMBER     CUSTOM_UART_SCB_NUMBER
#endif /* CUSTOM_UART_SCB_NUMBER */

#ifndef CUSTOM_UART_PORT
#error "Option USE_CUSTOM_DEBUG_UART turned on, but CUSTOM_UART_PORT is undefined \r\n";
#else
#define BSP_UART_PORT   CUSTOM_UART_PORT
#endif /* CUSTOM_UART_PORT */

#ifndef CUSTOM_UART_RX_PIN
#error "Option USE_CUSTOM_DEBUG_UART turned on, but CUSTOM_UART_RX_PIN is undefined \r\n";
#else
#define BSP_UART_RX_PIN CUSTOM_UART_RX_PIN
#endif /* CUSTOM_UART_RX_PIN */

#ifndef CUSTOM_UART_TX_PIN
#error "Option USE_CUSTOM_DEBUG_UART turned on, but CUSTOM_UART_TX_PIN is undefined \r\n";
#else
#define BSP_UART_TX_PIN CUSTOM_UART_TX_PIN
#endif /* CUSTOM_UART_TX_PIN */

/* Use default HW, which is commonly used on Infineon kits */
#else

#define BSP_UART_HW           SCB5
#define BSP_UART_SCB_NUMBER   5
#define BSP_UART_PORT         5
#define BSP_UART_RX_PIN       0
#define BSP_UART_TX_PIN       1

#endif /* USE_CUSTOM_DEBUG_UART */

#if defined(__cplusplus)
}
#endif
#endif /* BSP_H */
