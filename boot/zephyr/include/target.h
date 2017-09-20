/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

#if defined(MCUBOOT_TARGET_CONFIG)
/*
 * Target-specific definitions are permitted in legacy cases that
 * don't provide the information via DTS, etc.
 */
#include MCUBOOT_TARGET_CONFIG
#else
/*
 * Otherwise, the Zephyr SoC header and the DTS provide most
 * everything we need.
 *
 * TODO: remove soc_family_foo.h once image sector sizes come from the
 * flash driver.
 */
#include <soc.h>

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

#if defined(CONFIG_SOC_FAMILY_NRF5)
#include "soc_family_nrf5.h"
#elif defined(CONFIG_SOC_FAMILY_STM32)
#include "soc_family_stm32.h"
#elif defined(CONFIG_SOC_FAMILY_KINETIS)
#include "soc_family_kinetis.h"
#endif
#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

/*
 * Sanity check the target support.
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
