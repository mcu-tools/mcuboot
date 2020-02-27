/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef H_NRF_CLEANUP_
#define H_NRF_CLEANUP_

/**
 * Perform cleanup on some peripheral resources used by MCUBoot prior chainload
 * the application.
 *
 * This function disables all RTC instances and UARTE instances.
 * It Disables their interrupts signals as well.
 */
void nrf_cleanup_peripheral(void);

#endif
