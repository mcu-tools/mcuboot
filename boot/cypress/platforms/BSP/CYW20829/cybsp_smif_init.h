/***********************************************************************************************//**
 * \file cybsp_smif_init.h
 *
 * \brief
 * Basic API for setting up boards containing a Cypress MCU.
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

#ifndef CYBSP_SMIF_INIT_H
#define CYBSP_SMIF_INIT_H
#include "cy_smif_memslot.h"
#include "cycfg_qspi_memslot.h"

#define SMIF_HW SMIF0
#define SMIF_DESELECT_DELAY 7
#define TIMEOUT_1_MS            (1000ul)  /* 1 ms timeout for all blocking functions */
#define MEMORY_BUSY_CHECK_RETRIES    (750ul) /* Set it high enough for the sector erase operation to
                                                complete */

/**
 * \brief Initialize the smif IP.
 *
 * \returns CY_SMIF_SUCCESS if the SMIF is successfully initialized, if there is
 *          a problem initializing any hardware it returns an error code specific
 *          to the problem.
 */
cy_en_smif_status_t cybsp_smif_init(void);

/**
 * \brief Polls memory device until it is ready to receive new commands, or retry limit is exceeded
 *
 * \param memConfig Memory Device Configuration
 * \returns Status of the operation.
 * CY_SMIF_SUCCESS          - Memory is ready to accept new commands.
 * CY_SMIF_EXCEED_TIMEOUT   - Memory is busy.
 */
cy_en_smif_status_t cybsp_is_memory_ready(cy_stc_smif_mem_config_t const* memConfig);

/**
 * \brief Disables the SMIF IO.
 *
 */
void cybsp_smif_disable();

/**
 * \brief Enables the SMIF IO.
 *
 */
void cybsp_smif_enable();

#endif /*CYBSP_SMIF_INIT_H*/
