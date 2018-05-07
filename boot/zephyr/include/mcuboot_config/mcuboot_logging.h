/*
 * Copyright (c) 2018 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

#ifndef __BOOTSIM__

/*
 * When building for targets running Zephyr, delegate to its native
 * logging subsystem.
 *
 * In this case:
 *
 * - MCUBOOT_LOG_LEVEL determines SYS_LOG_LEVEL,
 * - MCUBOOT_LOG_ERR() and friends are SYS_LOG_ERR() etc.
 * - SYS_LOG_DOMAIN is unconditionally set to "MCUBOOT"
 */
#define MCUBOOT_LOG_LEVEL_OFF      SYS_LOG_LEVEL_OFF
#define MCUBOOT_LOG_LEVEL_ERROR    SYS_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_LEVEL_WARNING  SYS_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_LEVEL_INFO     SYS_LOG_LEVEL_INFO
#define MCUBOOT_LOG_LEVEL_DEBUG    SYS_LOG_LEVEL_DEBUG

/* Treat MCUBOOT_LOG_LEVEL equivalently to SYS_LOG_LEVEL. */
#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#elif (MCUBOOT_LOG_LEVEL < CONFIG_SYS_LOG_OVERRIDE_LEVEL)
#undef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL CONFIG_SYS_LOG_OVERRIDE_LEVEL
#endif

#define SYS_LOG_LEVEL MCUBOOT_LOG_LEVEL

#undef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "MCUBOOT"

#define MCUBOOT_LOG_ERR(...) SYS_LOG_ERR(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...) SYS_LOG_WRN(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...) SYS_LOG_INF(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...) SYS_LOG_DBG(__VA_ARGS__)

#include <logging/sys_log.h>

#endif /* !__BOOTSIM__ */

#endif /* __MCUBOOT_LOGGING_H__ */
