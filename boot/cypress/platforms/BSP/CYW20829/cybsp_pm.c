/***************************************************************************//**
* \file cybsp_pm.c
*
* Description:
* Provides initialization code for starting up the hardware contained on the
* Infineon board.
*
********************************************************************************
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
*******************************************************************************/

#include <stdlib.h>
#include "cybsp_pm_callbacks.h"
#include "cybsp.h"

#if defined(__cplusplus)
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// cybsp_pm_callbacks_register
//--------------------------------------------------------------------------------------------------
cy_rslt_t cybsp_pm_callbacks_register(void)
{
    cy_stc_syspm_callback_t** _cybsp_callbacks_array;
    size_t number_of_callbacks = 0;

    _cybsp_pm_callbacks_get_ptr_and_number(&_cybsp_callbacks_array, &number_of_callbacks);

    if ((number_of_callbacks == 0) || (*_cybsp_callbacks_array == NULL))
    {
        // Nothing to register, return success
        return CY_RSLT_SUCCESS;
    }

    for (size_t cb_idx = 0; cb_idx < number_of_callbacks; ++cb_idx)
    {
        if (!Cy_SysPm_RegisterCallback(_cybsp_callbacks_array[cb_idx]))
        {
            return CYBSP_RSLT_ERR_SYSCLK_PM_CALLBACK;
        }
    }

    return CY_RSLT_SUCCESS;
}


#if defined(__cplusplus)
}
#endif
