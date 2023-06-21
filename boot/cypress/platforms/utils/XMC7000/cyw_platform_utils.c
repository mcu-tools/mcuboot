/***************************************************************************//**
* \file cyw_platform_utils.h
*
* \brief
* xmc7000 platform utilities
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

#include <string.h>
#include "cyw_platform_utils.h"

#define IVT_ALIGNMENT (0x3FFu) /* IVT alignment requires to have these bits as zeros in IVT */
#define STACK_ALIGNMENT (7u) /* Per ARM AABI, a stack should be aligned to 64 bits, thus should have these bits as zeros */
#define THUMB_CALL_MASK (1u) /* THUMB ISA requires the LSB of a function call address to be 1 */

/* Symbols below are provided by the linker script: */
extern uint8_t __data_start__[];
extern uint8_t __data_end__[];

extern uint8_t __bss_start__[];
extern uint8_t __bss_end__[];

extern uint8_t __HeapBase[];
extern uint8_t __HeapLimit[];

extern uint8_t __StackLimit[];
extern uint8_t __StackTop[];

/* An app begins with vector table that starts with: */
typedef struct
{
    uint32_t stack_pointer;
    void (*reset_handler)(void);
} vect_tbl_start_t;

/**
 * Starts the application on the current core. MCUBoot is also running on
 * this core, so we just clean up memory, set up the vector table and stack,
 * and transfer control to the app's reset handler.
 *
 * @param app_addr  FIH-protected address of the app's vector table.
 */
void xmc7000_launch_cm7_app(fih_uint app_addr)
{
    register vect_tbl_start_t* const vect_tbl1 = (vect_tbl_start_t*)fih_uint_decode(app_addr);

#if defined(APP_CORE_ID)
# if APP_CORE_ID == 0
    Cy_SysEnableCM7(CORE_CM7_0, (uintptr_t)vect_tbl1);
# elif APP_CORE_ID == 1
    Cy_SysEnableCM7(CORE_CM7_1, (uintptr_t)vect_tbl1);
# endif
#else
# error "APP_CORE_ID is incorrect"
#endif

    for(;;)
    {
        (void)Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }
}
