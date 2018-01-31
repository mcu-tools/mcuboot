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
 */
#include <soc.h>

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

/*
 * TODO: remove soc_family_kinetis.h once its flash driver supports
 * FLASH_PAGE_LAYOUT.
 */
#if defined(CONFIG_SOC_FAMILY_KINETIS)
#include "soc_family_kinetis.h"
#endif
#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

/*
 * Upstream Zephyr changed the name from FLASH_DRIVER_NAME to
 * FLASH_DEV_NAME.  For now, let's just convert the Zephyr name to the
 * one expected by MCUboot. This can be cleaned up after the upstream
 * Zephyr tree has been released and settled down.
 */
#if !defined(FLASH_DRIVER_NAME) && defined(FLASH_DEV_NAME)
#define FLASH_DRIVER_NAME FLASH_DEV_NAME
#endif

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
