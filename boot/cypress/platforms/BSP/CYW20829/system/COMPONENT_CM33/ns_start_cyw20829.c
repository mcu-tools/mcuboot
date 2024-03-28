/***************************************************************************//**
* \file startup_cat1b_cm33.c
* \version 1.2
*
* The CAT1B CM33 startup source.
*
********************************************************************************
* \copyright
* Copyright (c) (2020-2022), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
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

#include "cy_device.h"

#if defined (CY_IP_M33SYSCPUSS)

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "startup_cat1b.h"
#include "cy_syslib.h"
#include "cmsis_compiler.h"

/*----------------------------------------------------------------------------
  External References
 *----------------------------------------------------------------------------*/
extern unsigned int __INITIAL_SP;
extern unsigned int __STACK_LIMIT;

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
extern uint32_t __STACK_SEAL;
#endif

extern __NO_RETURN void __PROGRAM_START(void);

#if defined(__ARMCC_VERSION)
#if defined(CY_PDL_TZ_ENABLED)
cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".bss.noinit.RESET_RAM"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".bss.noinit.RESET_RAM"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#elif defined (__GNUC__)
#if defined(CY_PDL_TZ_ENABLED)
cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".ram_vectors"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".ram_vectors"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#elif defined (__ICCARM__)
#if defined(CY_PDL_TZ_ENABLED)
cy_israddress_cat1b __s_vector_table_rw[VECTORTABLE_SIZE]  __attribute__( ( section(".intvec_ram"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#else
cy_israddress_cat1b __ns_vector_table_rw[VECTORTABLE_SIZE] __attribute__( ( section(".intvec_ram"))) __attribute__((aligned(VECTORTABLE_ALIGN)));
#endif
#else
    #error "An unsupported toolchain"
#endif  /* (__ARMCC_VERSION) */


/*----------------------------------------------------------------------------
  Internal References
 *----------------------------------------------------------------------------*/
__NO_RETURN void Reset_Handler (void);
void SysLib_FaultHandler(uint32_t const *faultStackAddr);
void Default_Handler(void);

void SysLib_FaultHandler(uint32_t const *faultStackAddr)
{
    Cy_SysLib_FaultHandler(faultStackAddr);
}

/*----------------------------------------------------------------------------
  Default Handler for Exceptions / Interrupts
 *----------------------------------------------------------------------------*/
void Default_Handler(void)
{
    while(1);
}


/*----------------------------------------------------------------------------
  Exception / Interrupt Handler
 *----------------------------------------------------------------------------*/
void NMIException_Handler(void);
void HardFault_Handler(void);
void InterruptHandler(void);
__WEAK void cy_toolchain_init(void);


// Exception Vector Table & Handlers
//----------------------------------------------------------------
void NMIException_Handler(void)
{
    __asm volatile(
        "bkpt #10\n"
        "B .\n"
    );
}

void HardFault_Handler(void)
{
    __asm (
        "MRS R0, CONTROL\n"
        "TST R0, #2\n"
        "ITE EQ\n"
        "MRSEQ R0, MSP\n"
        "MRSNE R0, PSP\n"
        "B SysLib_FaultHandler\n"
    );
}


void InterruptHandler(void)
{
    __asm volatile(
        "bkpt #1\n"
        "B .\n"
    );
}

void MemManage_Handler      (void) __attribute__ ((weak, alias("Default_Handler")));
void BusFault_Handler       (void) __attribute__ ((weak, alias("HardFault_Handler")));
void UsageFault_Handler     (void) __attribute__ ((weak, alias("HardFault_Handler")));
 void SVC_Handler            (void) __attribute__ ((weak, alias("HardFault_Handler")));
void DebugMon_Handler       (void) __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler         (void) __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler        (void) __attribute__ ((weak, alias("Default_Handler")));

void Interrupt0_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt1_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt2_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt3_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt4_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt5_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt6_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt7_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt8_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));
void Interrupt9_Handler     (void) __attribute__ ((weak, alias("InterruptHandler")));

/*----------------------------------------------------------------------------
  Exception / Interrupt Vector table
 *----------------------------------------------------------------------------*/

const cy_israddress __Vectors[VECTORTABLE_SIZE];

#if defined ( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

const cy_israddress __Vectors[VECTORTABLE_SIZE] __VECTOR_TABLE_ATTRIBUTE  = {
  (cy_israddress)(&__INITIAL_SP),                          /*     Initial Stack Pointer */
  (cy_israddress)Reset_Handler,                            /*     Reset Handler */
  (cy_israddress)NMIException_Handler,                     /* -14 NMI Handler */
  (cy_israddress)HardFault_Handler,                        /* -13 Hard Fault Handler */
  (cy_israddress)MemManage_Handler,                        /* -12 MPU Fault Handler */
  (cy_israddress)BusFault_Handler,                         /* -11 Bus Fault Handler */
  (cy_israddress)UsageFault_Handler,                       /* -10 Usage Fault Handler */
  0,                                                       /*  -9 Secure Fault Handler */
  0,                                                       /*     Reserved */
  0,                                                       /*     Reserved */
  0,                                                       /*     Reserved */
  (cy_israddress)SVC_Handler,                              /*  -5 SVCall Handler */
  0,                                                       /*  -4 Debug Monitor Handler */
  0,                                        /*     Reserved */
  (cy_israddress)PendSV_Handler,                           /*  -2 PendSV Handler */
  (cy_israddress)SysTick_Handler,                          /*  -1 SysTick Handler */

  /* Interrupts */
  (cy_israddress)Interrupt0_Handler,                       /*   0 Interrupt 0 */
  (cy_israddress)Interrupt1_Handler,                       /*   1 Interrupt 1 */
  (cy_israddress)Interrupt2_Handler,                       /*   2 Interrupt 2 */
  (cy_israddress)Interrupt3_Handler,                       /*   3 Interrupt 3 */
  (cy_israddress)Interrupt4_Handler,                       /*   4 Interrupt 4 */
  (cy_israddress)Interrupt5_Handler,                       /*   5 Interrupt 5 */
  (cy_israddress)Interrupt6_Handler,                       /*   6 Interrupt 6 */
  (cy_israddress)Interrupt7_Handler,                       /*   7 Interrupt 7 */
  (cy_israddress)Interrupt8_Handler,                       /*   8 Interrupt 8 */
  (cy_israddress)Interrupt9_Handler,                       /*   9 Interrupt 9 */
                                            /* Interrupts 10 .. 480 are left out */

};

#if defined ( __GNUC__ )
#pragma GCC diagnostic pop
#endif

/* Provide empty __WEAK implementation for the low-level initialization
   routine required by the RTOS-enabled applications.
   clib-support library provides FreeRTOS-specific implementation:
   https://github.com/Infineon/clib-support */
__WEAK void cy_toolchain_init(void)
{
}

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
/* GCC: newlib crt0 _start executes software_init_hook.
   The cy_toolchain_init hook provided by clib-support library must execute
   after static data initialization and before static constructors. */
void software_init_hook();
void software_init_hook()
{
    cy_toolchain_init();
}
#elif defined(__ICCARM__)
/* Initialize data section */
void __iar_data_init3(void);

/* Call the constructors of all global objects */
void __iar_dynamic_initialization(void);

/* Define strong version to return zero for __iar_program_start
   to skip data sections initialization (__iar_data_init3). */
int __low_level_init(void);
int __low_level_init(void)
{
    return 0;
}
#else
/**/
#endif /* defined(__GNUC__) && !defined(__ARMCC_VERSION) */

/*----------------------------------------------------------------------------
  Reset Handler called on controller reset
 *----------------------------------------------------------------------------*/
__NO_RETURN void Reset_Handler(void)
{
    /* Disable I cache */
    ICACHE0->CTL = ICACHE0->CTL & (~ICACHE_CTL_CA_EN_Msk);

    /* Enable ECC */
    //ICACHE0->CTL = ICACHE0->CTL | ICACHE_CTL_ECC_EN_Msk;

    /* Enable I cache */
    ICACHE0->CTL = ICACHE0->CTL | ICACHE_CTL_CA_EN_Msk;

    __disable_irq();

    for (uint32_t count = 0; count < VECTORTABLE_SIZE; count++)
    {
        #if defined(CY_PDL_TZ_ENABLED)
        __s_vector_table_rw[count] =__Vectors[count];
        #else
        __ns_vector_table_rw[count] =__Vectors[count];
        #endif
    }
    #if defined(CY_PDL_TZ_ENABLED)
    SCB->VTOR = (uint32_t)__s_vector_table_rw;
    #else
    SCB->VTOR = (uint32_t)__ns_vector_table_rw;
    #endif

    __DMB();

    __set_MSPLIM((uint32_t)(&__STACK_LIMIT));

    SystemInit();

#if defined(__ICCARM__)
    /* Initialize data section */
    __iar_data_init3();

    /* Initialization hook for RTOS environment  */
    cy_toolchain_init();

    /* Call the constructors of all global objects */
    __iar_dynamic_initialization();
#endif

   __PROGRAM_START();
}


#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif


#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic pop
#endif

#endif /* CY_IP_M33SYSCPUSS */
