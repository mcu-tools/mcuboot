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

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include "cy_pdl.h"

#define TIMESTAMP_SOURCE CY_SYSTICK_CLOCK_SOURCE_CLK_LF
#define TIMESTAMP_DIVIDER (CY_SYSCLK_ILO_FREQ / 1000UL)

/*******************************************************************************
* Function Name: log_timestamp_get
****************************************************************************//**
*
* \brief Get current timestamp counter value.
*
* \return Systic counter as timestamp reference.
*/
static inline uint32_t log_timestamp_get(void) {
    return ((0x1000000UL - Cy_SysTick_GetValue()) / TIMESTAMP_DIVIDER);
}

/*******************************************************************************
* Function Name: log_timestamp_reset
****************************************************************************//**
*
* \brief Reset timestamp counter.
*/
static inline void log_timestamp_reset(void) {
    Cy_SysTick_Init(TIMESTAMP_SOURCE, 0xFFFFFFu);
    Cy_SysTick_DisableInterrupt();
}

/*******************************************************************************
* Function Name: log_timestamp_init
****************************************************************************//**
*
* \brief Initializate timestamp counter and SysTick timebase.
*/
static inline void log_timestamp_init(void) {
    log_timestamp_reset(); 
    Cy_SysTick_Clear();
}

/*******************************************************************************
* Function Name: log_timestamp_deinit
****************************************************************************//**
*
* \brief Deinitializate timestamp counter and SysTick timebase.
*/
static inline void log_timestamp_deinit(void) {
    Cy_SysTick_Disable();
    Cy_SysTick_Clear();
}

#endif /* TIMESTAMP_H */
