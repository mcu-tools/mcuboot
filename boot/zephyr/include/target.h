/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

/* Board-specific definitions go first, to allow maximum override. */
#if defined(MCUBOOT_TARGET_CONFIG)
#include MCUBOOT_TARGET_CONFIG
#endif

/* SoC family configuration. */
#if defined(CONFIG_SOC_FAMILY_NRF5)
#include "soc_family_nrf5.h"
#endif

/*
 * This information can come from DTS, a target-specific header file,
 * or an SoC-specific header file. If any of it is missing, target
 * support is incomplete.
 */
#if !defined(FLASH_DRIVER_NAME) || \
    !defined(FLASH_ALIGN) ||                  \
    !defined(FLASH_AREA_IMAGE_0_OFFSET) || \
    !defined(FLASH_AREA_IMAGE_0_SIZE) || \
    !defined(FLASH_AREA_IMAGE_1_OFFSET) || \
    !defined(FLASH_AREA_IMAGE_1_SIZE) || \
    !defined(FLASH_AREA_IMAGE_SCRATCH_OFFSET) || \
    !defined(FLASH_AREA_IMAGE_SCRATCH_SIZE)
#error "Target support is incomplete; cannot build mcuboot."
#endif

#endif
