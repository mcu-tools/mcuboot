/*
 * Copyright (c) 2017 Linaro
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_SOC_SERIES_KINETIS_K6X
#define FLASH_DRIVER_NAME		CONFIG_SOC_FLASH_MCUX_DEV_NAME
#define FLASH_ALIGN			8
#define FLASH_AREA_IMAGE_SECTOR_SIZE	0x01000
#endif
