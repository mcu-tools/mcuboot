/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

#define CONFIG_MCUBOOT 1

#define MCUBOOT_USE_TINYCRYPT 1

#define CONFIG_BOOT_VALIDATE_SLOT0 1

#define CONFIG_BOOT_MAX_IMG_SECTORS 128

#define CONFIG_LOG   1

#define CONFIG_BOOT_VALIDATE_SLOT0_ONCE 1

#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_INFO
#endif

#endif