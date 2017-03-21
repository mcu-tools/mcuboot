/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>

/*
 * Rather than hard-coding the flash size for each SoC, pull it out of
 * the factory information configuration registers.
 */
#define FLASH_AREA_IMAGE_SECTOR_SIZE (NRF_FICR->CODEPAGESIZE)
