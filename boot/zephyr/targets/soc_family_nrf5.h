/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>

#define FLASH_AREA_IMAGE_SECTOR_SIZE (NRF_FICR->CODEPAGESIZE)
#define FLASH_DRIVER_NAME CONFIG_SOC_FLASH_NRF5_DEV_NAME
#define FLASH_ALIGN 4
