/***********************************************************************************************//**
 * \file cybsp_pm_callbacks.h
 *
 * \brief
 * Basic API for setting up boards containing an Infineon MCU.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
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
 **************************************************************************************************/

#pragma once

#include "cy_syspm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \brief Get number of PM callbacks, that are available for specific BSP
 * \param[out] arr_ptr  Pointer to store array of callback pointers
 * \param[out] number_of_elements   Pointer to store number of elements in array of callback
 * pointers
 */
void _cybsp_pm_callbacks_get_ptr_and_number(cy_stc_syspm_callback_t*** arr_ptr,
                                            size_t* number_of_elements);

#ifdef __cplusplus
}
#endif // __cplusplus
