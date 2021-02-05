/***************************************************************************//**
* \file cy_wdg.c
*
* \brief
* Provides a high level interface for interacting with the Cypress Watchdog Timer.
* This interface abstracts out the chip specific details. If any chip specific
* functionality is necessary, or performance is critical the low level functions
* can be used directly.
*
*
********************************************************************************
* \copyright
* Copyright 2019-2020 Cypress Semiconductor Corporation
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

#include <stdbool.h>
#include "watchdog.h"
#include "cy_wdt.h"
#include "cy_utils.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(COMPONENT_PSOC6)
#define _cy_wdg_lock()   Cy_WDT_Lock()
#define _cy_wdg_unlock() Cy_WDT_Unlock()
#else
#define _cy_wdg_lock()
#define _cy_wdg_unlock()
#endif

// ((2^16 * 2) + (2^16 - 1)) * .030518 ms
/** Maximum WDT timeout in milliseconds */
#define _cy_wdg_MAX_TIMEOUT_MS 6000

/** Maximum number of ignore bits */
#define _cy_wdg_MAX_IGNORE_BITS 12

typedef struct {
    uint16_t min_period_ms; // Minimum period in milliseconds that can be represented with this many ignored bits
    uint16_t round_threshold_ms; // Timeout threshold in milliseconds from which to round up to the minimum period
} _cy_wdg_ignore_bits_data_t;

// ILO Frequency = 32768 Hz
// ILO Period = 1 / 32768 Hz = .030518 ms
// WDT Reset Period (timeout_ms) = .030518 ms * (2 * 2^(16 - ignore_bits) + match)
// ignore_bits range: 0 - 12
// match range: 0 - (2^(16 - ignore_bits) - 1)
static const _cy_wdg_ignore_bits_data_t _cy_wdg_ignore_data[] = {
    {4001, 3001}, // 0 bits:  min period: 4001ms, max period: 6000ms, round up from 3001+ms
    {2001, 1500}, // 1 bit:   min period: 2001ms, max period: 3000ms, round up from 1500+ms
    {1001, 750},  // 2 bits:  min period: 1001ms, max period: 1499ms, round up from 750+ms
    {501,  375},  // 3 bits:  min period: 501ms,  max period: 749ms,  round up from 375+ms
    {251,  188},  // 4 bits:  min period: 251ms,  max period: 374ms,  round up from 188+ms
    {126,  94},   // 5 bits:  min period: 126ms,  max period: 187ms,  round up from 94+ms
    {63,   47},   // 6 bits:  min period: 63ms,   max period: 93ms,   round up from 47+ms
    {32,   24},   // 7 bits:  min period: 32ms,   max period: 46ms,   round up from 24+ms
    {16,   12},   // 8 bits:  min period: 16ms,   max period: 23ms,   round up from 12+ms
    {8,    6},    // 9 bits:  min period: 8ms,    max period: 11ms,   round up from 6+ms
    {4,    3},    // 10 bits: min period: 4ms,    max period: 5ms,    round up from 3+ms
    {2,    2},    // 11 bits: min period: 2ms,    max period: 2ms
    {1,    1}     // 12 bits: min period: 1ms,    max period: 1ms
};

static bool _cy_wdg_initialized = false;
static bool _cy_wdg_pdl_initialized = false;
static uint16_t _cy_wdg_initial_timeout_ms = 0;
static uint8_t _cy_wdg_initial_ignore_bits = 0;

static __INLINE uint32_t _cy_wdg_timeout_to_ignore_bits(uint32_t *timeout_ms) {
    for (uint32_t i = 0; i <= _cy_wdg_MAX_IGNORE_BITS; i++)
    {
        if (*timeout_ms >= _cy_wdg_ignore_data[i].round_threshold_ms)
        {
            if (*timeout_ms < _cy_wdg_ignore_data[i].min_period_ms)
                *timeout_ms = _cy_wdg_ignore_data[i].min_period_ms;
            return i;
        }
    }
    return _cy_wdg_MAX_IGNORE_BITS; // Should never reach this
}

static __INLINE uint16_t _cy_wdg_timeout_to_match(uint16_t timeout_ms, uint16_t ignore_bits)
{
    // match = (timeout_ms / .030518 ms) - (2 * 2^(16 - ignore_bits))
    return (uint16_t)(timeout_ms / .030518) - (1UL << (17 - ignore_bits)) + Cy_WDT_GetCount();
}

/* Start API implementing */

cy_rslt_t cy_wdg_init(uint32_t timeout_ms)
{
    if (timeout_ms == 0 || timeout_ms > _cy_wdg_MAX_TIMEOUT_MS)
    {
        return -1;
    }

    if (_cy_wdg_initialized)
    {
        return -1;
    }

    _cy_wdg_initialized = true;

    if (!_cy_wdg_pdl_initialized)
    {
        Cy_WDT_Enable();
        Cy_WDT_MaskInterrupt();
        _cy_wdg_pdl_initialized = true;
    }

    cy_wdg_stop();

    _cy_wdg_initial_timeout_ms = timeout_ms;
    uint8_t ignore_bits = _cy_wdg_timeout_to_ignore_bits(&timeout_ms);
    _cy_wdg_initial_ignore_bits = ignore_bits;

    Cy_WDT_SetIgnoreBits(ignore_bits);

    Cy_WDT_SetMatch(_cy_wdg_timeout_to_match(timeout_ms, ignore_bits));

    cy_wdg_start();

    return CY_RSLT_SUCCESS;
}

void cy_wdg_free()
{
    cy_wdg_stop();

    _cy_wdg_initialized = false;
}

void cy_wdg_kick()
{
    /* Clear to prevent reset from WDT */
    Cy_WDT_ClearWatchdog();

    _cy_wdg_unlock();
    Cy_WDT_SetMatch(_cy_wdg_timeout_to_match(_cy_wdg_initial_timeout_ms, _cy_wdg_initial_ignore_bits));
    _cy_wdg_lock();
}

void cy_wdg_start()
{
    _cy_wdg_unlock();
    Cy_WDT_Enable();
    _cy_wdg_lock();
}

void cy_wdg_stop()
{
    _cy_wdg_unlock();
    Cy_WDT_Disable();
}

uint32_t cy_wdg_get_timeout_ms()
{
    return _cy_wdg_initial_timeout_ms;
}

uint32_t cy_wdg_get_max_timeout_ms(void)
{
    return _cy_wdg_MAX_TIMEOUT_MS;
}

#if defined(__cplusplus)
}
#endif
