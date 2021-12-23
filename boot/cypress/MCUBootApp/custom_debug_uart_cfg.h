/********************************************************************************
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

#if !defined(CUSTOM_DEBUG_UART_CFG_H)
#define CUSTOM_DEBUG_UART_CFG_H

#if USE_CUSTOM_DEBUG_UART

/* Define SCB which will be used as UART. */
#define CUSTOM_UART_HW             SCB1

/* Define SCB number which will be used as UART.
 * It is 'x' in SCBx
 * */
#define CUSTOM_UART_SCB_NUMBER     1

#define CUSTOM_UART_PORT           10

/* define RX pin */
#define CUSTOM_UART_RX_PIN         0

/* define TX pin */
#define CUSTOM_UART_TX_PIN         1

#endif /* USE_CUSTOM_DEBUG_UART */

#endif /* CUSTOM_DEBUG_UART_CFG_H */
