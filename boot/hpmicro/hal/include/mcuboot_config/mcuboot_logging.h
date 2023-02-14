/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

#include "stdio.h"
#define MCUBOOT_LOG_MODULE_DECLARE(...)

#define LOG_ERR(...)   do {\
                            printf("ERR:"); printf(__VA_ARGS__); printf("\n");\
                       } while (0)

#define LOG_WRN(...)   do {\
                            printf("WRN:"); printf(__VA_ARGS__); printf("\n");\
                       } while (0)

#define LOG_INF(...)   do {\
                            printf("INF:"); printf(__VA_ARGS__); printf("\n");\
                       } while (0)

#define LOG_DBG(...)   do {\
                            printf("DBG:"); printf(__VA_ARGS__); printf("\n");\
                       } while (0)

#define MCUBOOT_LOG_ERR(...) LOG_ERR(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...) LOG_WRN(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...) LOG_INF(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...) LOG_DBG(__VA_ARGS__)
#define MCUBOOT_LOG_SIM(...) IGNORE(__VA_ARGS__)

#endif /* __MCUBOOT_LOGGING_H__ */
