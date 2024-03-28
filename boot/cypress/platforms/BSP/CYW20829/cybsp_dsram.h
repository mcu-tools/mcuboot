/***********************************************************************************************//**
 * \file cybsp_dsram.h
 *
 * \brief
 * Basic API for DSRAM support.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2021 Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cy_result.h"
#include "cybsp_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \addtogroup group_bsp_dsram_functions Functions
 * \{
 * All functions exposed by the board.
 */

/**
 * \brief Prepares the system to handle warm boot.
 */
void cybsp_syspm_do_warmboot(void);

/**
 * \brief Initializes the deepsleep ram setup.
 * \returns CY_RSLT_SUCCESS if the board is successfully initialized, if there is
 *          a problem initializing any hardware it returns an error code specific
 *          to the hardware module that had a problem.
 */
cy_rslt_t cybsp_syspm_dsram_init(void);


extern cy_stc_syspm_warmboot_entrypoint_t syspmBspDeepSleepEntryPoint; ///< DS entry point

/** \} group_bsp_dsram_functions */

#ifdef __cplusplus
}
#endif // __cplusplus
