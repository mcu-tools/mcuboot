/*
 * mcuboot_assert.h
 *
 * Cypress-specific assert() macro redefinition
 *
 */
/********************************************************************************
* \copyright
* (c) 2025, Cypress Semiconductor Corporation (an Infineon company) or
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

#ifndef MCUBOOT_ASSERT_H
#define MCUBOOT_ASSERT_H

//#include "cy_bootloader_services.h"

#define CYBL_ASSERT(...) Cy_BLServ_Assert(__VA_ARGS__)

#if !defined(NDEBUG)
#undef assert
#define assert(...) CYBL_ASSERT(__VA_ARGS__)
#else
#define assert
#endif

#endif /* MCUBOOT_ASSERT_H */
