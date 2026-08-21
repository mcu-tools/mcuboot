/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/version.h>
#include <zephyr/app_version.h>

#if defined(APP_BUILD_VERSION)
#define APPLICATION_BANNER_VERSION STRINGIFY(APP_BUILD_VERSION)
#elif defined(APP_VERSION_EXTENDED_STRING)
#define APPLICATION_BANNER_VERSION APP_VERSION_EXTENDED_STRING
#endif

#if defined(APPLICATION_BANNER_VERSION)
static int boot_banner(void)
{
	printk("*** Booting MCUboot " APPLICATION_BANNER_VERSION " ***\n");

	return 0;
}

SYS_INIT(boot_banner, APPLICATION, 0);
#endif /* APPLICATION_BANNER_VERSION */
