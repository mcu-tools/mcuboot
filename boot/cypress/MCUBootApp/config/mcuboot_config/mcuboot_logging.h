/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/

#ifndef MCUBOOT_LOGGING_H
#define MCUBOOT_LOGGING_H

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "timestamp.h"

static inline void print_msg(char const *format, ...)
{
    va_list args;
    va_start(args, format);

#ifdef USE_LOG_TIMESTAMP
    (void)fprintf(stderr, "[%03" PRIu32 "s"
                          ".%03" PRIu32 "ms]",
                          log_timestamp_get()/1000U,
                          log_timestamp_get()%1000U);
#endif

    (void)vfprintf(stderr, format, args);

    va_end(args);
}

#define MCUBOOT_LOG_LEVEL_OFF      0
#define MCUBOOT_LOG_LEVEL_ERROR    1
#define MCUBOOT_LOG_LEVEL_WARNING  2
#define MCUBOOT_LOG_LEVEL_INFO     3
#define MCUBOOT_LOG_LEVEL_DEBUG    4

/*
 * The compiled log level determines the maximum level that can be
 * printed.  Messages at or below this level can be printed, provided
 * they are also enabled through the Rust logging system, such as by
 * setting RUST_LOG to bootsim::api=info.
 */
#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_INFO
#endif

#ifdef __BOOTSIM__
int sim_log_enabled(int level);
#else
static inline int sim_log_enabled(int level) {
    (void)level;
    return 1;
}
#endif


#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_ERR(_fmt, ...)                           \
    do {                                                     \
        if (sim_log_enabled(MCUBOOT_LOG_LEVEL_ERROR) != 0) { \
            print_msg("[ERR] " _fmt "\n\r", ##__VA_ARGS__);  \
        }                                                    \
    } while ((bool)0)
#else
#define MCUBOOT_LOG_ERR(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_WRN(_fmt, ...)                             \
    do {                                                       \
        if (sim_log_enabled(MCUBOOT_LOG_LEVEL_WARNING) != 0) { \
            print_msg("[WRN] " _fmt "\n\r", ##__VA_ARGS__);    \
        }                                                      \
    } while ((bool)0)
#else
#define MCUBOOT_LOG_WRN(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_INFO
#define MCUBOOT_LOG_INF(_fmt, ...)                          \
    do {                                                    \
        if (sim_log_enabled(MCUBOOT_LOG_LEVEL_INFO) != 0) { \
            print_msg("[INF] " _fmt "\n\r", ##__VA_ARGS__); \
        }                                                   \
    } while ((bool)0)
#else
#define MCUBOOT_LOG_INF(...) IGNORE(__VA_ARGS__)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_DBG(_fmt, ...)                           \
    do {                                                     \
        if (sim_log_enabled(MCUBOOT_LOG_LEVEL_DEBUG) != 0) { \
            print_msg("[DBG] " _fmt "\n\r", ##__VA_ARGS__);  \
        }                                                    \
    } while ((bool)0)
#else
#define MCUBOOT_LOG_DBG(...) IGNORE(__VA_ARGS__)
#endif

#define MCUBOOT_LOG_MODULE_DECLARE(domain)  /* ignore */
#define MCUBOOT_LOG_MODULE_REGISTER(domain) /* ignore */

#endif /* MCUBOOT_LOGGING_H */
