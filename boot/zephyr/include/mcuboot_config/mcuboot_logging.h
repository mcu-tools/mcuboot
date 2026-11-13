/*
 * Copyright (c) 2018 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

/*
 * When building for targets running Zephyr, delegate to its native
 * logging subsystem.
 */
#ifdef CONFIG_MCUBOOT
#define MCUBOOT_LOG_MODULE_DECLARE(domain)	LOG_MODULE_DECLARE(domain, CONFIG_MCUBOOT_LOG_LEVEL)
#define MCUBOOT_LOG_MODULE_REGISTER(domain)	LOG_MODULE_REGISTER(domain, CONFIG_MCUBOOT_LOG_LEVEL)
#else
#define MCUBOOT_LOG_MODULE_DECLARE(domain)	LOG_MODULE_DECLARE(domain, CONFIG_MCUBOOT_UTIL_LOG_LEVEL)
#define MCUBOOT_LOG_MODULE_REGISTER(domain)	LOG_MODULE_REGISTER(domain, CONFIG_MCUBOOT_UTIL_LOG_LEVEL)
#endif

#define MCUBOOT_LOG_ERR(...) LOG_ERR(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...) LOG_WRN(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...) LOG_INF(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...) LOG_DBG(__VA_ARGS__)
#define MCUBOOT_LOG_SIM(...) IGNORE(__VA_ARGS__)

#include <zephyr/logging/log.h>

/* MCUboot drains deferred logs from its own thread, which must be stopped to flush them. */
#if defined(CONFIG_MCUBOOT) && defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_IMMEDIATE) && \
    !defined(CONFIG_LOG_MODE_MINIMAL) && !defined(CONFIG_LOG_PROCESS_THREAD)
void zephyr_boot_log_start(void);
void zephyr_boot_log_stop(void);
#define ZEPHYR_BOOT_LOG_START() zephyr_boot_log_start()
#define ZEPHYR_BOOT_LOG_STOP() zephyr_boot_log_stop()
#else
#define ZEPHYR_BOOT_LOG_START() ((void)0)
#define ZEPHYR_BOOT_LOG_STOP() ((void)0)
#endif

#endif /* __MCUBOOT_LOGGING_H__ */
