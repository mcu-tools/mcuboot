/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <mcuboot_config/mcuboot_logging.h>

/* Log levels from IDF are similar to MCUboot's */

#ifndef CONFIG_BOOTLOADER_LOG_LEVEL
#define CONFIG_BOOTLOADER_LOG_LEVEL MCUBOOT_LOG_LEVEL
#endif

#define ESP_LOGE(tag, fmt, ...) MCUBOOT_LOG_ERR("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) MCUBOOT_LOG_WRN("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) MCUBOOT_LOG_INF("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) MCUBOOT_LOG_DBG("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) MCUBOOT_LOG_DBG("[%s] " fmt, tag, ##__VA_ARGS__)

#define ESP_EARLY_LOGE(tag, fmt, ...) MCUBOOT_LOG_ERR("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGW(tag, fmt, ...) MCUBOOT_LOG_WRN("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGI(tag, fmt, ...) MCUBOOT_LOG_INF("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGD(tag, fmt, ...) MCUBOOT_LOG_DBG("[%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGV(tag, fmt, ...) MCUBOOT_LOG_DBG("[%s] " fmt, tag, ##__VA_ARGS__)
