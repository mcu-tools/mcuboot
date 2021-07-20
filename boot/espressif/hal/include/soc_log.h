/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mcuboot_config/mcuboot_logging.h>
#include <esp_rom_sys.h>

#define SOC_LOGE(tag, fmt, ...) MCUBOOT_LOG_ERR(fmt, ##__VA_ARGS__)
#define SOC_LOGW(tag, fmt, ...) MCUBOOT_LOG_WRN(fmt, ##__VA_ARGS__)
#define SOC_LOGI(tag, fmt, ...) MCUBOOT_LOG_INF(fmt, ##__VA_ARGS__)
#define SOC_LOGD(tag, fmt, ...) MCUBOOT_LOG_DBG(fmt, ##__VA_ARGS__)
