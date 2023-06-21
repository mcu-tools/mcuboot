/***************************************************************************//**
* \file cyw_platform_utils.h
*
* \brief
* PSoC6 platform utilities
*
********************************************************************************
* \copyright
* (c) 2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
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
#ifndef CYW_PLATFORMS_UTILS_H
#define CYW_PLATFORMS_UTILS_H

#include "cy_pdl.h"
#include "bootutil/fault_injection_hardening.h"

#if defined BOOT_CM0P
/**
 * Starts the application on the Cortex-M0+ core. MCUBoot is also running on
 * this core, so we just clean up memory, set up the vector table and stack,
 * and transfer control to the app's reset handler.
 *
 * @param app_addr  FIH-protected address of the app's vector table.
 */
__NO_RETURN void psoc6_launch_cm0p_app(fih_uint app_addr);
#elif defined BOOT_CM4
/**
 * Starts the application on the Cortex-M4 core. MCUBoot is also running on
 * this core, so we just clean up memory, set up the vector table and stack,
 * and transfer control to the app's reset handler.
 *
 * @param app_addr  FIH-protected address of the app's vector table.
 */
__NO_RETURN void psoc6_launch_cm4_app(fih_uint app_addr);
#else
#error "MCUBoot should be compiled for either Cortex-M0+ or Cortex-M4"
#endif /* defined CM... */

#endif /* CYW_PLATFORMS_UTILS_H */
