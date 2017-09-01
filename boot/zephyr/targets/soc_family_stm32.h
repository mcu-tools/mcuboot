/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <soc.h>

#define FLASH_DRIVER_NAME               CONFIG_SOC_FLASH_STM32_DEV_NAME

#if defined(CONFIG_SOC_SERIES_STM32F4X)
/*
 * The Zephyr flash driver will let us remove the need for
 * FLASH_AREA_IMAGE_SECTOR_SIZE at some point. It doesn't make sense
 * on these targets, anyway, as they have variable sized sectors.
 *
 * For now, let's rely on the fact that on all STM32F4 chips with at
 * most 16 flash sectors, all of the sectors after the first 128 KB
 * are equal sized.
 */
#if (!defined(FLASH_AREA_IMAGE_SECTOR_SIZE) && FLASH_SECTOR_TOTAL <= 16 && \
     FLASH_AREA_IMAGE_0_OFFSET >= KB(128))
#define FLASH_AREA_IMAGE_SECTOR_SIZE    0x20000
#endif
#define FLASH_ALIGN                     1
#elif defined(CONFIG_SOC_SERIES_STM32L4X) /* !CONFIG_SOC_SERIES_STM32F4X */
#define FLASH_ALIGN                     8
#define FLASH_AREA_IMAGE_SECTOR_SIZE    FLASH_PAGE_SIZE /* from the HAL */
#endif /* CONFIG_SOC_SERIES_STM32F4X */
