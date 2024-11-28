/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
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
 */

#ifndef CLEANUP_H
#define CLEANUP_H

#include <stdint.h>

#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define STACKLESS __STATIC_INLINE
#elif defined(__ICCARM__)
#define STACKLESS __stackless __STATIC_INLINE
#elif defined(__GNUC__)
#define STACKLESS __STATIC_INLINE
#endif

#define VECTOR_TABLE_ALIGNMENT (0x400U)

typedef __NO_RETURN void (*reset_handler_t)(void);

typedef struct vect_tbl_start_s {
    uint32_t        stack_pointer;
    reset_handler_t reset_handler;
} vect_tbl_start_t;

/*******************************************************************************
 * Function Name: cleanup_helper
 ********************************************************************************
 * Summary:
 * Cleans ram region
 * This function used inside cleanup_and_boot function
 *
 * Parameters:
 *  l - region start pointer(lower address)
 *  r - region end pointer (higher address)
 *
 * Note:
 *   This function is critical to be "stackless".
 *   Two oncoming indices algorithm is used to prevent compiler optimization
 *     from calling memset function.
 *
 *******************************************************************************/
STACKLESS
void cleanup_helper(register uint8_t *l, register uint8_t *r)
{
    register uint8_t v = 0u;

    do {
        *l = v;
        ++l;

        --r;
        *r = v;
    } while (l < r);
}

/*******************************************************************************
 * Function Name: cleanup_and_boot
 ********************************************************************************
 * Summary:
 * This function cleans all ram and boots target app
 *
 * Parameters:
 * p_vect_tbl_start - target app vector table address
 *
 *
 *******************************************************************************/
STACKLESS __NO_RETURN void launch_cm33_app(register vect_tbl_start_t* p_vect_tbl_start)
{
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    {
        extern uint8_t  Image$$RW_RAM_DATA$$Base[];
        extern uint32_t Image$$RW_RAM_DATA$$Length[];
        cleanup_helper(
            Image$$RW_RAM_DATA$$Base,
            Image$$RW_RAM_DATA$$Base + (uint32_t)Image$$RW_RAM_DATA$$Length);
    }

    {
        extern uint8_t  Image$$RW_IRAM1$$Base[];
        extern uint32_t Image$$RW_IRAM1$$Length[];
        cleanup_helper(
            Image$$RW_IRAM1$$Base,
            Image$$RW_IRAM1$$Base + (uint32_t)Image$$RW_IRAM1$$Length);
    }

    {
        extern uint8_t  Image$$ER_RAM_VECTORS$$Base[];
        extern uint32_t Image$$ER_RAM_VECTORS$$Length[];
        cleanup_helper(Image$$ER_RAM_VECTORS$$Base,
                       Image$$ER_RAM_VECTORS$$Base +
                           (uint32_t)Image$$ER_RAM_VECTORS$$Length);
    }

    {
        extern uint8_t  Image$$ARM_LIB_HEAP$$Base[];
        extern uint32_t Image$$ARM_LIB_HEAP$$Length[];
        cleanup_helper(
            Image$$ARM_LIB_HEAP$$Base,
            Image$$ARM_LIB_HEAP$$Base + (uint32_t)Image$$ARM_LIB_HEAP$$Length);
    }

#elif defined(__ICCARM__)
    {
#pragma section = ".data"
        cleanup_helper(__section_begin(".data"), __section_end(".data"));
    }

    {
#pragma section = ".bss"
        cleanup_helper(__section_begin(".bss"), __section_end(".bss"));
    }

    {
#pragma section = "HSTACK"
        cleanup_helper(__section_begin("HSTACK"), __section_end("HSTACK"));
    }
#elif defined(__GNUC__)
    {
        extern uint8_t __data_start__[];
        extern uint8_t __data_end__[];
        cleanup_helper(__data_start__, __data_end__);
    }

    {
        extern uint8_t __bss_start__[];
        extern uint8_t __bss_end__[];
        cleanup_helper(__bss_start__, __bss_end__);
    }

    {
        extern uint8_t __HeapBase[];
        extern uint8_t __HeapLimit[];
        cleanup_helper(__HeapBase, __HeapLimit);
    }

    {
        extern uint8_t __StackLimit[];
        extern uint8_t __StackTop[];
        cleanup_helper(__StackLimit, __StackTop);
    }
#endif /* cleanup */
    /* Init next app vector table */
#ifdef COMPONENT_CM33
    MXCM33->CM33_NS_VECTOR_TABLE_BASE = (uint32_t)(void*)p_vect_tbl_start;
#endif /* COMPONENT_CM33 */
    SCB->VTOR = (uint32_t)(void*)p_vect_tbl_start;
    __DSB();

    /* Init next app stack pointer */
#ifdef COMPONENT_CM33
    __set_MSPLIM(0U);
#endif /* COMPONENT_CM33 */
    __set_MSP(p_vect_tbl_start->stack_pointer);
#ifdef COMPONENT_CM33
    /* FIXME: CM33_STACK_LIMIT must be defined for CM33 core */
    __set_MSPLIM(CM33_STACK_LIMIT);
#endif /* COMPONENT_CM33 */

    /* Jump to next app */
    p_vect_tbl_start->reset_handler();
}

#endif /* CLEANUP_H */
