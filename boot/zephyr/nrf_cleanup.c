/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <hal/nrf_rtc.h>
#include <hal/nrf_uarte.h>
#include <hal/nrf_clock.h>

#if defined(NRF_RTC0) || defined(NRF_RTC1) || defined(NRF_RTC2)
static inline void nrf_cleanup_rtc(NRF_RTC_Type * rtc_reg)
{
    nrf_rtc_task_trigger(rtc_reg, NRF_RTC_TASK_STOP);
    nrf_rtc_event_disable(rtc_reg, 0xFFFFFFFF);
    nrf_rtc_int_disable(rtc_reg, 0xFFFFFFFF);
}
#endif

static void nrf_cleanup_clock(void)
{
    nrf_clock_int_disable(NRF_CLOCK, 0xFFFFFFFF);
}

void nrf_cleanup_peripheral(void)
{
#if defined(NRF_RTC0)
    nrf_cleanup_rtc(NRF_RTC0);
#endif
#if defined(NRF_RTC1)
    nrf_cleanup_rtc(NRF_RTC1);
#endif
#if defined(NRF_RTC2)
    nrf_cleanup_rtc(NRF_RTC2);
#endif
#if defined(NRF_UARTE0)
    nrf_uarte_disable(NRF_UARTE0);
    nrf_uarte_int_disable(NRF_UARTE0, 0xFFFFFFFF);
#endif
#if defined(NRF_UARTE1)
    nrf_uarte_disable(NRF_UARTE1);
    nrf_uarte_int_disable(NRF_UARTE1, 0xFFFFFFFF);
#endif
    nrf_cleanup_clock();
}
