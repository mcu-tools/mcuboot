/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef H_MCUBOOT_LOGGING_H_
#define H_MCUBOOT_LOGGING_H_

#include <stdio.h>

#define MCUBOOT_LOG_MODULE_DECLARE(name)
#define MCUBOOT_LOG_MODULE_REGISTER(name)

#if defined(MCUBOOT_HAVE_LOGGING)
#define MCUBOOT_LOG_ERR(_fmt, ...) printf("[BOOT_ERR] " _fmt "\n", ##__VA_ARGS__)
#define MCUBOOT_LOG_WRN(_fmt, ...) printf("[BOOT_WRN] " _fmt "\n", ##__VA_ARGS__)
#define MCUBOOT_LOG_INF(_fmt, ...) printf("[BOOT_INF] " _fmt "\n", ##__VA_ARGS__)
#define MCUBOOT_LOG_DBG(_fmt, ...) printf("[BOOT_DBG] " _fmt "\n", ##__VA_ARGS__)
#else
#define MCUBOOT_LOG_ERR(...) do {} while (0)
#define MCUBOOT_LOG_WRN(...) do {} while (0)
#define MCUBOOT_LOG_INF(...) do {} while (0)
#define MCUBOOT_LOG_DBG(...) do {} while (0)
#endif

#define MCUBOOT_LOG_SIM(...) do {} while (0)

/*
 * Standard MCUboot logging macros: MCUboot uses the BOOT_LOG_* prefix.
 */
#ifndef BOOT_LOG_ERR
#define BOOT_LOG_ERR(...) MCUBOOT_LOG_ERR(__VA_ARGS__)
#endif
#ifndef BOOT_LOG_WRN
#define BOOT_LOG_WRN(...) MCUBOOT_LOG_WRN(__VA_ARGS__)
#endif
#ifndef BOOT_LOG_INF
#define BOOT_LOG_INF(...) MCUBOOT_LOG_INF(__VA_ARGS__)
#endif
#ifndef BOOT_LOG_DBG
#define BOOT_LOG_DBG(...) MCUBOOT_LOG_DBG(__VA_ARGS__)
#endif
#ifndef BOOT_LOG_SIM
#define BOOT_LOG_SIM(...) MCUBOOT_LOG_SIM(__VA_ARGS__)
#endif

#ifndef BOOT_LOG_MODULE_DECLARE
#define BOOT_LOG_MODULE_DECLARE(module) MCUBOOT_LOG_MODULE_DECLARE(module)
#endif
#ifndef BOOT_LOG_MODULE_REGISTER
#define BOOT_LOG_MODULE_REGISTER(module) MCUBOOT_LOG_MODULE_REGISTER(module)
#endif

#endif /* H_MCUBOOT_LOGGING_H_ */
