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
 */

#define MCUBOOT_LOG_MODULE_DECLARE(domain)	LOG_MODULE_DECLARE(domain)
#define MCUBOOT_LOG_MODULE_REGISTER(domain)	LOG_MODULE_REGISTER(domain)

#define MCUBOOT_LOG_ERR(...) LOG_ERR(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...) LOG_WRN(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...) LOG_INF(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...) LOG_DBG(__VA_ARGS__)

#include <logging/log.h>

#endif /* !__BOOTSIM__ */

#endif /* __MCUBOOT_LOGGING_H__ */
