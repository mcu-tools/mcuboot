/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

/* Board-specific definitions go first, to allow maximum override. */
#if defined(MCUBOOT_TARGET_CONFIG)
#include MCUBOOT_TARGET_CONFIG
#else
#error "Board is currently not supported by bootloader"
#endif

/* SoC family configuration. */
#if defined(CONFIG_SOC_FAMILY_NRF5)
#include "soc_family_nrf5.h"
#endif

#endif
