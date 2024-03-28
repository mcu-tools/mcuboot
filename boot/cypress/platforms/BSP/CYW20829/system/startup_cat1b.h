/***************************************************************************//**
* \file startup_cat1b.h
* \version 1.1
*
* \brief Common startup header file for CAT1B devices. This file provides
* declarations for secure and non-secure vector table.
*
********************************************************************************
* \copyright
* Copyright 2016-2021 Cypress Semiconductor Corporation
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


#ifndef STARTUP_CAT1B_H_
#define STARTUP_CAT1B_H_

#include "cy_device_headers.h"


typedef void (* cy_israddress_cat1b)(void);   /**< Type of ISR callbacks */

#define CM33_FIXED_EXP_NR       (15u)
#define VECTORTABLE_SIZE        (MXCM33_SYSTEM_INT_NR + CM33_FIXED_EXP_NR + 1u) /* +1 is for Stack pointer */
#define VECTORTABLE_ALIGN       (VECTORTABLE_SIZE <= 512u ? 512u :1024u) /* alignment for 'n' entries is (nx4) */

#if defined(__ARMCC_VERSION)
#if defined(CY_PDL_TZ_ENABLED)
    extern cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".bss.noinit.RESET_RAM"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
    extern cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".bss.noinit.RESET_RAM"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#elif defined (__GNUC__)
#if defined(CY_PDL_TZ_ENABLED)
    extern cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".ram_vectors"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
    extern cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".ram_vectors"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#elif defined (__ICCARM__)
#if defined(CY_PDL_TZ_ENABLED)
    extern cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE]  __attribute__( ( section(".intvec_ram"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
    extern cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".intvec_ram"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#else
    #error "An unsupported toolchain"
#endif  /* (__ARMCC_VERSION) */

#endif /* STARTUP_CAT1B_H_ */
