/*
 * Copyright (c) 2025, Jamie McCrae
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Set watchdog timer up and active it ready for feeding. */
void mcuboot_watchdog_setup(void);

/** Feed active watchdog to prevent timeout. */
void mcuboot_watchdog_feed(void);

#define MCUBOOT_WATCHDOG_SETUP() mcuboot_watchdog_setup()
#define MCUBOOT_WATCHDOG_FEED() mcuboot_watchdog_feed()
