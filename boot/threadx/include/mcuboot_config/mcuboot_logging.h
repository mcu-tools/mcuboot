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

#endif /* H_MCUBOOT_LOGGING_H_ */
