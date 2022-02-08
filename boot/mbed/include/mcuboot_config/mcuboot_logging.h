/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 * Copyright (c) 2020 Embedded Planet
 * Copyright (c) 2020 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the Licens.
 */

#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

#define MCUBOOT_LOG_LEVEL_OFF      0
#define MCUBOOT_LOG_LEVEL_ERROR    1
#define MCUBOOT_LOG_LEVEL_WARNING  2
#define MCUBOOT_LOG_LEVEL_INFO     3
#define MCUBOOT_LOG_LEVEL_DEBUG    4

/*
 * The compiled log level determines the maximum level that can be
 * printed.
 */
#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_OFF
#endif

#if MCUBOOT_LOG_LEVEL == MCUBOOT_LOG_LEVEL_OFF
#define MBED_CONF_MBED_TRACE_ENABLE 0
#else
#define MBED_CONF_MBED_TRACE_ENABLE 1
#define MCUBOOT_HAVE_LOGGING
#endif

#if MCUBOOT_LOG_LEVEL == MCUBOOT_LOG_LEVEL_ERROR
#define MBED_TRACE_MAX_LEVEL TRACE_LEVEL_ERROR
#elif MCUBOOT_LOG_LEVEL == MCUBOOT_LOG_LEVEL_WARNING
#define MBED_TRACE_MAX_LEVEL TRACE_LEVEL_WARN
#elif MCUBOOT_LOG_LEVEL == MCUBOOT_LOG_LEVEL_INFO
#define MBED_TRACE_MAX_LEVEL TRACE_LEVEL_INFO
#elif MCUBOOT_LOG_LEVEL == MCUBOOT_LOG_LEVEL_DEBUG
#define MBED_TRACE_MAX_LEVEL TRACE_LEVEL_DEBUG
#endif

#define TRACE_GROUP "MCUb"
#include "mbed-trace/mbed_trace.h"
#include "bootutil/ignore.h"

#define MCUBOOT_LOG_MODULE_DECLARE(domain)  /* ignore */
#define MCUBOOT_LOG_MODULE_REGISTER(domain) /* ignore */

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_ERR tr_error
#else
#define MCUBOOT_LOG_ERR(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_WRN tr_warn
#else
#define MCUBOOT_LOG_WRN(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_INFO
#define MCUBOOT_LOG_INF tr_info
#else
#define MCUBOOT_LOG_INF(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_DBG tr_debug
#else
#define MCUBOOT_LOG_DBG(...) IGNORE(__VA_ARGS__)
#endif

#endif /* __MCUBOOT_LOGGING_H__ */
