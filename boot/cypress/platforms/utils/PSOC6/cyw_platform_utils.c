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
 * Should never get to this function.
 */
static __NO_RETURN void hang(void)
{
    while (true) {
        CY_ASSERT((bool)0);
    }
}

/**
 * Starts the application on the current core. MCUBoot is also running on
 * this core, so we just clean up memory, set up the vector table and stack,
 * and transfer control to the app's reset handler.
 *
 * @param app_addr  FIH-protected address of the app's vector table.
 */
#ifdef BOOT_CM0P
__NO_RETURN void psoc6_launch_cm0p_app(fih_uint app_addr)
#else
__NO_RETURN void psoc6_launch_cm4_app(fih_uint app_addr)
#endif /* BOOT_CM0P */
{
    register vect_tbl_start_t* const vect_tbl1 = (vect_tbl_start_t*)fih_uint_decode(app_addr);
    register vect_tbl_start_t* const vect_tbl2 = (vect_tbl_start_t*)fih_uint_decode(app_addr);

    do
    {
        if ( (vect_tbl1 != vect_tbl2) ||  /* validate app_addr */
             (0u != ((uint32_t)vect_tbl1 & IVT_ALIGNMENT)) || /* validate IVT alignment */
             (0u != ((uint32_t)vect_tbl1->stack_pointer & STACK_ALIGNMENT)) || /* validate stack alignment in IVT*/
             (0u == ((uint32_t)vect_tbl1->reset_handler & THUMB_CALL_MASK)) )  /* validate reset handler address is TUHMB compliant */
        {
            break;
        }

        /* Data, Heap, Stack and BSS section in RAM can contain sensitive data, used by bootloader.
        * Set these sections to the default of 0 before next application start so no data left in
        * RAM after bootloader completed execution.
        */
        (void)memset(__data_start__, 0, (size_t)((uintptr_t)__data_end__ - (uintptr_t)__data_start__));
        (void)memset(__HeapBase,     0, (size_t)((uintptr_t)__HeapLimit  - (uintptr_t)__HeapBase));
        (void)memset(__StackLimit,   0, (size_t)((uintptr_t)__StackTop   - (uintptr_t)__StackLimit));
        (void)memset(__bss_start__,  0, (size_t)((uintptr_t)__bss_end__  - (uintptr_t)__bss_start__));

        /* Relocate Vector Table */
        #ifdef BOOT_CM0P
            CPUSS->CM0_VECTOR_TABLE_BASE = (uintptr_t)vect_tbl1;
            if((uintptr_t)CPUSS->CM0_VECTOR_TABLE_BASE != (uintptr_t)vect_tbl2)
            {
                break;
            }
        #else
            CPUSS->CM4_VECTOR_TABLE_BASE = (uintptr_t)vect_tbl1;
            if((uintptr_t)CPUSS->CM4_VECTOR_TABLE_BASE != (uintptr_t)vect_tbl2)
            {
                break;
            }
        #endif /* BOOT_CM0P */
            SCB->VTOR = (uintptr_t)vect_tbl1;
            if((uintptr_t)SCB->VTOR != (uintptr_t)vect_tbl2)
            {
                break;
            }

            __DSB();
            __ISB();

            __set_MSP(vect_tbl1->stack_pointer);
            if ((uintptr_t)__get_MSP() != (uintptr_t)vect_tbl2->stack_pointer)
            {
                break;
            }

            if ((uintptr_t)vect_tbl1->reset_handler != (uintptr_t)vect_tbl2->reset_handler)
            {
                break;
            }

            /* Transfer control to the application */
            __DSB();
            __ISB();
            vect_tbl1->reset_handler(); /* Never returns */

    } while(false);

    /* Should never get here */
    hang();
}
