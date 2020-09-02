/***************************************************************************//**
* \file cy_wdg.h
*
* \brief
* Provides a high level interface for interacting with the Watchdog Timer.
* This interface abstracts out the chip specific details. If any chip specific
* functionality is necessary, or performance is critical the low level functions
* can be used directly.
*
********************************************************************************
* \copyright
* Copyright 2019-2020 Cypress Semiconductor Corporation
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

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "cy_result.h"

/** Initialize and start the WDT
*
* The specified timeout must be at least 1ms and at most the WDT's maximum timeout (see cy_wdg_get_max_timeout_ms()).
* @param[inout] timeout_ms The time in milliseconds before the WDT times out (1ms - max) (see cy_wdg_get_max_timeout_ms())
* @return The status of the init request
*
* Returns CY_RSLT_SUCCESS if the operation was successfull.
*/
cy_rslt_t cy_wdg_init(uint32_t timeout_ms);

/** Free the WDT
*
* Powers down the WDT.
* After calling this function no other WDT functions should be called except
* cy_wdg_init().
*/

void cy_wdg_free();

/** Resets the WDT
*
* This function should be called periodically to prevent the WDT from timing out and resetting the device.
*/
void cy_wdg_kick();

/** Start (enable) the WDT
*
* @return The status of the start request
*/
void cy_wdg_start();

/** Stop (disable) the WDT
*
* @return The status of the stop request
*/
void cy_wdg_stop();

/** Get the WDT timeout
*
* Gets the time in milliseconds before the WDT times out.
* @return The time in milliseconds before the WDT times out
*/
uint32_t cy_wdg_get_timeout_ms();

/** Gets the maximum WDT timeout in milliseconds
*
* @return The maximum timeout for the WDT
*/
uint32_t cy_wdg_get_max_timeout_ms(void);

#if defined(__cplusplus)
}
#endif
