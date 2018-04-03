/*
 * Copyright (c) 2017 Linaro Limited
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
 * limitations under the License.
 */

#ifndef H_BOOTUTIL_LOG_H_
#define H_BOOTUTIL_LOG_H_

#include "ignore.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * When building for targets running Zephyr, delegate to its native
 * logging subsystem.
 *
 * In this case:
 *
 * - BOOT_LOG_LEVEL determines SYS_LOG_LEVEL,
 * - BOOT_LOG_ERR() and friends are SYS_LOG_ERR() etc.
 * - SYS_LOG_DOMAIN is unconditionally set to "MCUBOOT"
 */
#ifdef __ZEPHYR__

#define BOOT_LOG_LEVEL_OFF      SYS_LOG_LEVEL_OFF
#define BOOT_LOG_LEVEL_ERROR    SYS_LOG_LEVEL_ERROR
#define BOOT_LOG_LEVEL_WARNING  SYS_LOG_LEVEL_WARNING
#define BOOT_LOG_LEVEL_INFO     SYS_LOG_LEVEL_INFO
#define BOOT_LOG_LEVEL_DEBUG    SYS_LOG_LEVEL_DEBUG

/* Treat BOOT_LOG_LEVEL equivalently to SYS_LOG_LEVEL. */
#ifndef BOOT_LOG_LEVEL
#define BOOT_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#elif (BOOT_LOG_LEVEL < CONFIG_SYS_LOG_OVERRIDE_LEVEL)
#undef BOOT_LOG_LEVEL
#define BOOT_LOG_LEVEL CONFIG_SYS_LOG_OVERRIDE_LEVEL
#endif

#define SYS_LOG_LEVEL BOOT_LOG_LEVEL

#undef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "MCUBOOT"

#define BOOT_LOG_ERR(...) SYS_LOG_ERR(__VA_ARGS__)
#define BOOT_LOG_WRN(...) SYS_LOG_WRN(__VA_ARGS__)
#define BOOT_LOG_INF(...) SYS_LOG_INF(__VA_ARGS__)
#define BOOT_LOG_DBG(...) SYS_LOG_DBG(__VA_ARGS__)

#include <logging/sys_log.h>

/*
 * When built on the simulator, just use printf().
 */
#elif defined(__BOOTSIM__)      /* !defined(__ZEPHYR__) */

#include <stdio.h>

#define BOOT_LOG_LEVEL_OFF      0
#define BOOT_LOG_LEVEL_ERROR    1
#define BOOT_LOG_LEVEL_WARNING  2
#define BOOT_LOG_LEVEL_INFO     3
#define BOOT_LOG_LEVEL_DEBUG    4

/*
 * The compiled log level determines the maximum level that can be
 * printed.  Messages at or below this level can be printed, provided
 * they are also enabled through the Rust logging system, such as by
 * setting RUST_LOG to bootsim::api=info.
 */
#ifndef BOOT_LOG_LEVEL
#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#endif

int sim_log_enabled(int level);

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_ERROR
#define BOOT_LOG_ERR(_fmt, ...)                                         \
    do {                                                                \
        if (sim_log_enabled(BOOT_LOG_LEVEL_ERROR)) {                    \
            fprintf(stderr, "[ERR] " _fmt "\n", ##__VA_ARGS__);         \
        }                                                               \
    } while (0)
#else
#define BOOT_LOG_ERR(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_WARNING
#define BOOT_LOG_WRN(_fmt, ...)                                         \
    do {                                                                \
        if (sim_log_enabled(BOOT_LOG_LEVEL_WARNING)) {                  \
            fprintf(stderr, "[WRN] " _fmt "\n", ##__VA_ARGS__);         \
        }                                                               \
    } while (0)
#else
#define BOOT_LOG_WRN(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_INFO
#define BOOT_LOG_INF(_fmt, ...)                                         \
    do {                                                                \
        if (sim_log_enabled(BOOT_LOG_LEVEL_INFO)) {                     \
            fprintf(stderr, "[INF] " _fmt "\n", ##__VA_ARGS__);         \
        }                                                               \
    } while (0)
#else
#define BOOT_LOG_INF(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_DEBUG
#define BOOT_LOG_DBG(_fmt, ...)                                         \
    do {                                                                \
        if (sim_log_enabled(BOOT_LOG_LEVEL_DEBUG)) {                    \
            fprintf(stderr, "[DBG] " _fmt "\n", ##__VA_ARGS__);         \
        }                                                               \
    } while (0)
#else
#define BOOT_LOG_DBG(...) IGNORE(__VA_ARGS__)
#endif

/*
 * When built on Mynewt, just use printf().
 */
#elif defined(MCUBOOT_MYNEWT)

#include <stdio.h>

#define BOOT_LOG_LEVEL_OFF      0
#define BOOT_LOG_LEVEL_ERROR    1
#define BOOT_LOG_LEVEL_WARNING  2
#define BOOT_LOG_LEVEL_INFO     3
#define BOOT_LOG_LEVEL_DEBUG    4

#ifndef BOOT_LOG_LEVEL
#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_ERROR
#define BOOT_LOG_ERR(_fmt, ...)                                         \
    do {                                                                \
        printf("[ERR] " _fmt "\n", ##__VA_ARGS__);                      \
    } while (0)
#else
#define BOOT_LOG_ERR(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_WARNING
#define BOOT_LOG_WRN(_fmt, ...)                                         \
    do {                                                                \
        printf("[WRN] " _fmt "\n", ##__VA_ARGS__);                      \
    } while (0)
#else
#define BOOT_LOG_WRN(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_INFO
#define BOOT_LOG_INF(_fmt, ...)                                         \
    do {                                                                \
        printf("[INF] " _fmt "\n", ##__VA_ARGS__);                      \
    } while (0)
#else
#define BOOT_LOG_INF(...) IGNORE(__VA_ARGS__)
#endif

#if BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_DEBUG
#define BOOT_LOG_DBG(_fmt, ...)                                         \
    do {                                                                \
        printf("[DBG] " _fmt "\n", ##__VA_ARGS__);                      \
    } while (0)
#else
#define BOOT_LOG_DBG(...) IGNORE(__VA_ARGS__)
#endif

/*
 * In other environments, logging calls are no-ops.
 */
#else  /* !defined(__BOOTSIM__) */

#define BOOT_LOG_LEVEL_OFF      0
#define BOOT_LOG_LEVEL_ERROR    1
#define BOOT_LOG_LEVEL_WARNING  2
#define BOOT_LOG_LEVEL_INFO     3
#define BOOT_LOG_LEVEL_DEBUG    4

#define BOOT_LOG_ERR(...) IGNORE(__VA_ARGS__)
#define BOOT_LOG_WRN(...) IGNORE(__VA_ARGS__)
#define BOOT_LOG_INF(...) IGNORE(__VA_ARGS__)
#define BOOT_LOG_DBG(...) IGNORE(__VA_ARGS__)

#endif

#ifdef __cplusplus
}
#endif

#endif
