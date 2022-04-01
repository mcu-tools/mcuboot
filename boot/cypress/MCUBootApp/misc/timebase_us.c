/* Copyright 2022, Infineon Technologies AG.  All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "timebase_us.h"

#include "cy_pdl.h"
#include "bootutil/bootutil_log.h"

static const cy_stc_tcpwm_counter_config_t tcpwm_config =
{
    .period            = 0xFFFFFFFFU,
    .clockPrescaler    = CY_TCPWM_COUNTER_PRESCALER_DIVBY_8, /* Clk_counter = Clk_input / 4 */
    .runMode           = CY_TCPWM_COUNTER_CONTINUOUS, /* Wrap around at terminal count. */
    .countDirection    = CY_TCPWM_COUNTER_COUNT_UP, /* Up counter, counting from 0 to period value. */
    .compareOrCapture  = CY_TCPWM_COUNTER_MODE_COMPARE, /* Trigger interrupt/event signal when Counter value is equal to Compare0 */
    .compare0          = 0U,
    .compare1          = 0U,
    .enableCompareSwap = false,
    .interruptSources  = CY_TCPWM_INT_NONE,
    .captureInputMode  = CY_TCPWM_INPUT_RISINGEDGE, /* This input is NOT used, leave it in default state (CY_TCPWM_INPUT_RISINGEDGE = 0UL) */
    .captureInput      = CY_TCPWM_INPUT_0,
    .reloadInputMode   = CY_TCPWM_INPUT_RISINGEDGE, /* This input is NOT used, leave it in default state (CY_TCPWM_INPUT_RISINGEDGE = 0UL) */
    .reloadInput       = CY_TCPWM_INPUT_0,
    .startInputMode    = CY_TCPWM_INPUT_RISINGEDGE, /* This input is NOT used, leave it in default state (CY_TCPWM_INPUT_RISINGEDGE = 0UL) */
    .startInput        = CY_TCPWM_INPUT_0,
    .stopInputMode     = CY_TCPWM_INPUT_RISINGEDGE, /* This input is NOT used, leave it in default state (CY_TCPWM_INPUT_RISINGEDGE = 0UL) */
    .stopInput         = CY_TCPWM_INPUT_0,
    .countInputMode    = CY_TCPWM_INPUT_LEVEL, /* Set this input to LEVEL and 1 (high logic level) */
    .countInput        = CY_TCPWM_INPUT_1 /* So the counter will count input clock periods (Clk_counter, taking into account the clock prescaler) */
};

/*******************************************************************************
* Function Name: timebase_us_init
****************************************************************************//**
*
* \brief Performs initialization of the TCPWM0 block as a microsecond time source.
*
*/
void timebase_us_init(void)
{
#ifdef CYW20829
    (void) Cy_SysClk_PeriphAssignDivider(PCLK_TCPWM0_CLOCK_COUNTER_EN0, CY_SYSCLK_DIV_8_BIT, 0UL);
#else 
    (void) Cy_SysClk_PeriphAssignDivider(PCLK_TCPWM0_CLOCKS0, CY_SYSCLK_DIV_8_BIT, 0UL);
#endif
    (void) Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0UL, 0UL);
    (void) Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_8_BIT, 0UL);

    (void) Cy_TCPWM_Counter_Init(TCPWM0, 0, &tcpwm_config);
    Cy_TCPWM_Counter_Enable(TCPWM0, 0);
    Cy_TCPWM_TriggerStart_Single(TCPWM0, 0);
}

/*******************************************************************************
* Function Name: timebase_us_deinit
****************************************************************************//**
*
* \brief Performs deinitialization of the TCPWM0.
*
*/
void timebase_us_deinit(void)
{
    (void) Cy_SysClk_PeriphDisableDivider(CY_SYSCLK_DIV_8_BIT, 0UL);

    Cy_TCPWM_Counter_DeInit(TCPWM0, 0, &tcpwm_config);
    Cy_TCPWM_Counter_Disable(TCPWM0, 0);
    Cy_TCPWM_TriggerStopOrKill_Single(TCPWM0, 0);
}

/*******************************************************************************
* Function Name: timebase_us_get_tick
****************************************************************************//**
*
* \brief Returns current timer counter value
*
* \return current timer counter value as uint32_t
*
*/
uint32_t timebase_us_get_tick(void)
{
    return Cy_TCPWM_Counter_GetCounter(TCPWM0, 0);
}