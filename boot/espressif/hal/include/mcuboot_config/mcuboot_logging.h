/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "mcuboot_config.h"

extern int ets_printf(const char *fmt, ...);

#define MCUBOOT_LOG_LEVEL_OFF      0
#define MCUBOOT_LOG_LEVEL_ERROR    1
#define MCUBOOT_LOG_LEVEL_WARNING  2
#define MCUBOOT_LOG_LEVEL_INFO     3
#define MCUBOOT_LOG_LEVEL_DEBUG    4

#if CONFIG_IDF_TARGET_ESP32
#define TARGET "[esp32]"
#elif CONFIG_IDF_TARGET_ESP32S2
#define TARGET "[esp32s2]"
#elif CONFIG_IDF_TARGET_ESP32C3
#define TARGET "[esp32c3]"
#else
#error "Selected target not supported."
#endif

#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_INFO
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_ERR(_fmt, ...)                                      \
    do {                                                                \
            ets_printf(TARGET " [ERR] " _fmt "\n\r", ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_ERR(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_WRN(_fmt, ...)                                      \
    do {                                                                \
            ets_printf(TARGET " [WRN] " _fmt "\n\r", ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_WRN(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_INFO
#define MCUBOOT_LOG_INF(_fmt, ...)                                      \
    do {                                                                \
            ets_printf(TARGET " [INF] " _fmt "\n\r", ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_INF(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_DBG(_fmt, ...)                                      \
    do {                                                                \
            ets_printf(TARGET " [DBG] " _fmt "\n\r", ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_DBG(_fmt, ...)
#endif

#define MCUBOOT_LOG_MODULE_DECLARE(...)
#define MCUBOOT_LOG_MODULE_REGISTER(...)
