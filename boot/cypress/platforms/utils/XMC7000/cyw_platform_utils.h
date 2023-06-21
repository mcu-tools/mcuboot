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

void xmc7000_launch_cm7_app(fih_uint app_addr);

#endif /* CYW_PLATFORMS_UTILS_H */
