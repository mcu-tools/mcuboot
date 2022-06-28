/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/*
 * This file is included by the simulator, but we don't want to
 * define almost anything here.
 *
 * Instead of using mcuboot_config.h, the simulator adds MCUBOOT_xxx
 * configuration flags to the compiler command lines based on the
 * values of environment variables. However, the file still must
 * exist, or bootutil won't build.
 */

#define MCUBOOT_WATCHDOG_FEED()         \
    do {                                \
    } while (0)

#define MCUBOOT_CPU_IDLE() \
    do {                                \
    } while (0)

#endif /* __MCUBOOT_CONFIG_H__ */
